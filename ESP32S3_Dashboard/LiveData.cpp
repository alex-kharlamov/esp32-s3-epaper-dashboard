#include "LiveData.h"
#include "Configuration.h"
#include "WeatherCache.h"
#include "WeatherParsing.h"
#include "TransportData.h"
#include "DashboardState.h"
#include "QuietHours.h"
#include "PowerManager.h"
#include "PowerPolicy.h"
#include "RuntimeSettings.h"
#include "ProviderPolicy.h"
#include "TransportDetails.h"
#include "TransportCache.h"
#include "HardwareServices.h"
#include "DeviceServices.h"
#include "HttpBodyReader.h"
#include <esp_heap_caps.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include <esp_sntp.h>
#include <math.h>
#include <atomic>
#include <memory>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace {
double LAT=0,LON=0;
const char *LOCAL_TIMEZONE=nullptr;
WeatherSnapshot weather{};
TransportSnapshot transport{};
SemaphoreHandle_t stateMutex=nullptr;
TaskHandle_t networkTask=nullptr;
DashboardText dashboardText{};
bool haveWeather=false,restoredWeather=false,repaintTransport=false;
std::atomic<uint32_t> sleepPreviewUntil{0},ntpGeneration{0},sessionDeadline{0};
std::atomic<bool> refreshRequested{false},networkHealthy{false},stopped{false},networkActive{false};
ProviderState weatherProvider,lineProvider,stationProvider,timeProvider,haProvider;
std::atomic<int64_t> ntpEpoch{0};
std::atomic<uint32_t> batchVersion{0};
char haNote[81]{};float indoorTemperature=0;bool indoorAvailable=false;
char weatherAdvice[121]{},dlrLabel[33]{},tubeLabel[33]{},dlrReason[161]{},tubeReason[161]{},statusFooter[321]{};
int64_t preparedTarget=-1;uint32_t preparationStarted=0;
void finishProvider(ProviderState &state,bool ok,int64_t source=0){xSemaphoreTake(stateMutex,portMAX_DELAY);state.finish(ok,time(nullptr),millis(),source);xSemaphoreGive(stateMutex);}
void networkWorker(void*);
String setupLine;

bool getJson(const String &url,JsonDocument &doc,const char *bearer=nullptr) {
  uint32_t deadline=sessionDeadline.load();int32_t remaining=int32_t(deadline-millis());
  if(stopped.load()||deviceMaintenanceRequested()||liveDataQuiet()||remaining<1500)return false;
  NetworkClientSecure tls;tls.useBuiltinCACertBundle();tls.setHandshakeTimeout(remaining<12000?(remaining+999)/1000:12);
  HTTPClient http;http.useHTTP10(true);http.setConnectTimeout(remaining<10000?remaining:10000);http.setTimeout(remaining<12000?remaining:12000);
  http.setUserAgent("ESP32-ePaper-Dashboard/2.0");
  const char *headers[]={"Transfer-Encoding"};http.collectHeaders(headers,1);
  if(!http.begin(tls,url))return false;if(bearer&&*bearer)http.addHeader("Authorization",String("Bearer ")+bearer);
  int code=http.GET();if(code!=200){Serial.printf("Data HTTP failure: %d\n",code);http.end();return false;}
  int length=http.getSize();if(length>60000){http.end();return false;}
  bool chunked=http.header("Transfer-Encoding").equalsIgnoreCase("chunked");
  char *body=(char*)heap_caps_malloc(60001,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!body){http.end();return false;}
  auto *stream=http.getStreamPtr();size_t used=0;
  auto read=[&](){for(;;){if(int32_t(deadline-millis())<=0||stopped.load()||deviceMaintenanceRequested()||liveDataQuiet())return -2;if(stream->available())return stream->read();if(!http.connected())return -1;vTaskDelay(pdMS_TO_TICKS(10));}};
  bool complete=readHttpBody(read,length,chunked,body,60001,used);http.end();
  if(!complete||!used){free(body);Serial.println("Data body over limit, truncated or deadline exceeded");return false;}
  JsonDocument filter;
  if(url.indexOf("open-meteo")>=0){filter["current"]=true;filter["hourly"]=true;}
  else if(bearer){filter["state"]=true;filter["attributes"]["unit_of_measurement"]=true;}
  else if(url.indexOf("/Line/")>=0){filter[0]["id"]=true;filter[0]["lineStatuses"]=true;}
  else {const char *keys[]={"stationAtcoCode","atcoCode","type","description","fromDate","toDate"};for(const char *key:keys)filter[0][key]=true;}
  auto error=deserializeJson(doc,body,used,DeserializationOption::Filter(filter));free(body);
  if(error){Serial.println("Data JSON parse failed");return false;}return true;

}
void connectSaved(bool scan=false) {
  WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);WiFi.setSleep(true);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  Preferences p;p.begin("dashboardwifi",true);
  String ssid=p.getString("ssid",""),pass=p.getString("pass","");p.end();
  if(scan && ssid.length()) {
    WiFi.disconnect();
    int count=WiFi.scanNetworks(false,true),matches=0;
    for(int i=0;i<count;i++)if(WiFi.SSID(i)==ssid) {
      matches++;
      Serial.printf("WIFI matching AP: channel=%d signal=%d dBm security=%d\n",int(WiFi.channel(i)),int(WiFi.RSSI(i)),int(WiFi.encryptionType(i)));
    }
    Serial.printf("WIFI scan: %d matching access points (SSID/password omitted).\n",matches);
    WiFi.scanDelete();
  }
  esp_err_t powerResult=esp_wifi_set_max_tx_power(DASH_WIFI_TX_POWER); // Configurable radio limit.
  int8_t tx=0;esp_wifi_get_max_tx_power(&tx);
  Serial.printf("WIFI radio: reduced TX limit result=%d actual=%.2f dBm\n",int(powerResult),tx/4.0);
  if(ssid.length())WiFi.begin(ssid.c_str(),pass.c_str());else WiFi.begin();

  Serial.printf("WIFI credentials: %s\n",ssid.length()?"local setup credentials loaded":"trying ESP32 system configuration");
  pass.clear();
  Serial.println("Connecting using existing ESP32 Wi-Fi configuration; credentials are not logged.");
}
}
bool beginLiveData() {
  LAT=deviceSettings().latitude;LON=deviceSettings().longitude;LOCAL_TIMEZONE=deviceSettings().timezone;
  stateMutex=xSemaphoreCreateMutex();if(!stateMutex)return false;
  Preferences cache;
  if(cache.begin("dashboardwx",true)) {
    size_t length=cache.getBytesLength("snapshot");
    uint8_t bytes[sizeof(WeatherRecord)]{};
    if(length<=sizeof(bytes)&&cache.getBytes("snapshot",bytes,length)==length&&loadWeatherRecord(bytes,length,LAT,LON,weather)){
      haveWeather=restoredWeather=true;weatherProvider.fetchedAt=weather.fetchedAt;weatherProvider.sourceAt=weather.observedAt;
      Serial.printf("WEATHER_CACHE_RESTORED: fetched_epoch=%lld hours=%u\n",(long long)weather.fetchedAt,weather.hourCount);
    }else Serial.println("WEATHER_CACHE: no valid cache for this location.");
    cache.end();
  }
  Preferences transportCache;if(transportCache.begin("dashtfl",true)){TransportRecord r{};if(transportCache.getBytesLength("snapshot")==sizeof(r)&&transportCache.getBytes("snapshot",&r,sizeof(r))==sizeof(r)&&loadTransportRecord(r,deviceSettings().dlrStation,deviceSettings().jubileeStation,transport))Serial.println("TFL_CACHE_RESTORED: last known status is explicitly old until revalidated");transportCache.end();}
  setenv("TZ",LOCAL_TIMEZONE,1);tzset();
  esp_sntp_set_time_sync_notification_cb([](struct timeval *tv) {
    ntpGeneration.fetch_add(1);ntpEpoch.store(tv->tv_sec);hardwareNtpSynced(tv->tv_sec);
    Serial.printf("NTP_SYNC: epoch=%lld.%06ld\n",(long long)tv->tv_sec,(long)tv->tv_usec);
  });
  WiFi.onEvent([](arduino_event_id_t event,arduino_event_info_t info) {
    if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED)Serial.printf("WIFI disconnected: reason=%d\n",info.wifi_sta_disconnected.reason);
    if(event==ARDUINO_EVENT_WIFI_STA_GOT_IP)Serial.println("WIFI connected; IP obtained.");
  });
  Serial.printf("LIVE: %s / timezone=%s; clock=%lu ms, full/weather=%lu ms.\n",DASH_LOCATION_LABEL,DASH_TIMEZONE,(unsigned long)DASH_CLOCK_INTERVAL_MS,(unsigned long)DASH_FULL_INTERVAL_MS);
  Serial.println("If Wi-Fi is not configured, run tools/wifi_setup.py locally over USB.");
  return xTaskCreate(networkWorker,"dashboard-data",16384,nullptr,1,&networkTask)==pdPASS;
}
void serviceLiveSetup() {
  // Manual light sleep advances esp_timer but not the RTOS tick count.
  // Wake the idle worker using monotonic elapsed time after every main wake.
  static uint32_t lastWorkerPulse=0;
  uint32_t pulseNow=millis();
  if(networkTask && pulseNow-lastWorkerPulse>=1000){lastWorkerPulse=pulseNow;xTaskNotifyGive(networkTask);}
  static bool reported=false;
  if(!reported && millis()>15000) {
    reported=true;
    Serial.printf("WIFI startup: %s; status=%d.\n",WiFi.status()==WL_CONNECTED?"connected":"disconnected",int(WiFi.status()));
  }
  hardwareService();deviceService();
  while(Serial.available()) {
    char c=Serial.read();if(c=='\r')continue;
    if(c!='\n'){if(setupLine.length()<2048)setupLine+=c;else setupLine.clear();continue;}
    if(deviceHandleCommand(setupLine)){
    }else if(setupLine.startsWith("WIFI ")) {
      JsonDocument doc;
      if(!deserializeJson(doc,setupLine.substring(5))&&doc["ssid"].is<const char*>()&&doc["password"].is<const char*>()) {
        String ssid=doc["ssid"].as<String>(),pass=doc["password"].as<String>();
        if(ssid.length()>0&&ssid.length()<=32&&pass.length()<=63) {
          Preferences p;p.begin("dashboardwifi",false);
          bool ok=p.putString("ssid",ssid)>0;
          p.putString("pass",pass);ok=p.isKey("pass")&&p.getString("pass")==pass&&ok;p.end();
          if(ok){Serial.println("WIFI configuration saved; connecting.");refreshRequested.store(true);networkHealthy.store(false);}
          else Serial.println("WIFI configuration could not be saved.");
        }else Serial.println("WIFI configuration has invalid lengths.");
        pass.clear();
      }else Serial.println("WIFI configuration invalid.");
    }else if(setupLine=="POWER STATUS") {
      powerReport();
    }else if(setupLine=="POWER LIGHT TEST") {
      powerLightTest();
    }else if(setupLine=="POWER BATTERY TEST") {
      powerSetBatteryTest(true);
    }else if(setupLine=="POWER DEBUG") {
      powerSetBatteryTest(false);
    }else if(setupLine=="POWER DEEP TEST") {
      sleepPreviewUntil.store(millis()+120000);powerRequestDeepTest();
      Serial.println("POWER_DEEP_TEST_REQUESTED: sleep image, 90-second timer wake; panel cooldown preserved.");
    }else if(setupLine=="SLEEP PREVIEW") {
      sleepPreviewUntil.store(millis()+90000);
      Serial.println("SLEEP_PREVIEW: 90 seconds; NTP unchanged, data/display quiet gate enabled.");
    }else if(setupLine=="REPAINT TRANSPORT") {
      repaintTransport=true;Serial.println("TRANSPORT_REPAINT_REQUESTED: same live values, normal colour waveform.");
    }else if(setupLine=="STATUS") {
      xSemaphoreTake(stateMutex,portMAX_DELAY);bool cached=haveWeather;xSemaphoreGive(stateMutex);
      Serial.printf("LIVE status: WiFi=%s time=%s weather=%s\n",WiFi.status()==WL_CONNECTED?"connected":"disconnected",time(nullptr)>1700000000?"available":"waiting",cached?"available":"waiting");
    }
    setupLine.clear();
  }
}
bool fetchLiveData() {
  time_t now=time(nullptr);
  if(WiFi.status()!=WL_CONNECTED || now<MIN_VALID_EPOCH || liveDataQuiet())return false;
  String location="latitude="+String(LAT,6)+"&longitude="+String(LON,6);
  String url="https://api.open-meteo.com/v1/forecast?"+location+
    "&current=temperature_2m,apparent_temperature,wind_speed_10m,wind_gusts_10m,wind_direction_10m,weather_code,is_day"
    "&hourly=temperature_2m,precipitation_probability,precipitation,weather_code,is_day&forecast_days=2&timeformat=unixtime&timezone=UTC";
  JsonDocument doc;WeatherSnapshot next{};
  if(!getJson(url,doc) || !parseWeatherResponse(doc.as<JsonVariantConst>(),time(nullptr),LAT,LON,next)) {
    Serial.println("WEATHER_FETCH_FAILED: keeping last good snapshot.");return false;
  }
  if(stopped.load() || liveDataQuiet())return false; // Discard a request crossing midnight.
  xSemaphoreTake(stateMutex,portMAX_DELAY);
  memcpy(&weather,&next,sizeof(next));haveWeather=true;restoredWeather=false;
  xSemaphoreGive(stateMutex);
  Preferences cache;bool saved=false;
  if(cache.begin("dashboardwx",false)) {
    WeatherRecord record=makeWeatherRecord(next);
    saved=cache.putBytes("snapshot",&record,sizeof(record))==sizeof(record);cache.end();
  }
  Serial.printf("WEATHER_FETCH_OK: temperature=%d wind=%.1f hours=%u; CACHE_%s\n",next.temperature,next.windSpeed,next.hourCount,saved?"SAVED":"SAVE_FAILED");
  return true;
}
namespace {
void updateTransportCheck(TransportCheck &c,ServiceHealth health,time_t checkedAt) {
  c.latestRequestOk=health!=ServiceHealth::Unknown;
  if(c.latestRequestOk){c.health=health;c.checkedAt=checkedAt;}
}
void fetchTransportData(bool fetchLines=true,bool fetchStations=true) {
  if(liveDataQuiet())return;
  TransportSnapshot next{};
  xSemaphoreTake(stateMutex,portMAX_DELAY);next=transport;xSemaphoreGive(stateMutex);
  JsonDocument lines,stations;
  bool linesOk=false,stationsOk=false;
  if(fetchLines)linesOk=getJson("https://api.tfl.gov.uk/Line/dlr,jubilee/Status?detail=true",lines);
  time_t now=time(nullptr);
  if(fetchLines){updateTransportCheck(next.dlr,linesOk?parseLineStatus(lines.as<JsonVariantConst>(),"dlr",now):ServiceHealth::Unknown,now);
  updateTransportCheck(next.jubilee,linesOk?parseLineStatus(lines.as<JsonVariantConst>(),"jubilee",now):ServiceHealth::Unknown,now);
  if(next.dlr.latestRequestOk)lineDetails(lines.as<JsonVariantConst>(),"dlr",now,next.dlr);
  if(next.jubilee.latestRequestOk)lineDetails(lines.as<JsonVariantConst>(),"jubilee",now,next.jubilee);
  finishProvider(lineProvider,linesOk&&next.dlr.latestRequestOk&&next.jubilee.latestRequestOk);}
  const auto &cfg=deviceSettings();
  if(fetchStations)stationsOk=getJson(String("https://api.tfl.gov.uk/StopPoint/")+cfg.dlrStation+","+cfg.jubileeStation+"/Disruption?includeRouteBlockedStops=false",stations);
  now=time(nullptr);
  if(fetchStations){
  updateTransportCheck(next.canningTube,stationsOk?parseStationStatus(stations.as<JsonVariantConst>(),cfg.jubileeStation,now):ServiceHealth::Unknown,now);
  updateTransportCheck(next.eastIndia,stationsOk?parseStationStatus(stations.as<JsonVariantConst>(),cfg.dlrStation,now):ServiceHealth::Unknown,now);
  if(next.canningTube.latestRequestOk)stationDetails(stations.as<JsonVariantConst>(),cfg.jubileeStation,now,next.canningTube);
  if(next.eastIndia.latestRequestOk)stationDetails(stations.as<JsonVariantConst>(),cfg.dlrStation,now,next.eastIndia);
  finishProvider(stationProvider,stationsOk&&next.canningTube.latestRequestOk&&next.eastIndia.latestRequestOk);
  }
  if(stopped.load() || liveDataQuiet())return; // Do not publish overnight state.
  xSemaphoreTake(stateMutex,portMAX_DELAY);transport=next;xSemaphoreGive(stateMutex);
  if(linesOk||stationsOk){TransportRecord r{};strcpy(r.dlrStation,cfg.dlrStation);strcpy(r.jubileeStation,cfg.jubileeStation);r.snapshot=next;r.checksum=transportChecksum(r);Preferences cache;if(cache.begin("dashtfl",false)){cache.putBytes("snapshot",&r,sizeof(r));cache.end();}}
  Serial.printf("TFL_FETCH: lines=%s stations=%s DLR=%u Jubilee=%u CanningTown=%u EastIndia=%u\n",linesOk?"OK":"FAILED",stationsOk?"OK":"FAILED",unsigned(next.dlr.health),unsigned(next.jubilee.health),unsigned(combineStationHealth(next.canningDlr.health,next.canningTube.health)),unsigned(next.eastIndia.health));
}
void fetchHomeAssistant(){
 const auto &cfg=deviceSettings();if(!*cfg.haUrl)return;bool ok=true;char note[81]{};float temperature=0;bool haveTemperature=false;
 String base=cfg.haUrl;while(base.endsWith("/"))base.remove(base.length()-1);
 if(*cfg.haTemperature){JsonDocument d;bool got=getJson(base+"/api/states/"+cfg.haTemperature,d,cfg.haToken);if(got&&d["state"].is<const char*>()){char *end=nullptr;double value=strtod(d["state"],&end);haveTemperature=end&&!*end&&isfinite(value)&&value>=-200&&value<=200;if(haveTemperature){const char *unit=d["attributes"]["unit_of_measurement"]|"";if(!strcmp(unit,"°F")||!strcmp(unit,"F"))value=(value-32)/1.8;else if(strcmp(unit,"°C")&&strcmp(unit,"C"))haveTemperature=false;haveTemperature &= value>=-90 && value<=80;temperature=value;}}ok&=got&&haveTemperature;}
 if(*cfg.haNote){JsonDocument d;bool got=getJson(base+"/api/states/"+cfg.haNote,d,cfg.haToken);if(got&&d["state"].is<const char*>()&&strcmp(d["state"],"unavailable")&&strcmp(d["state"],"unknown"))displayText(note,sizeof(note),d["state"]);else got=false;ok&=got;}
 if(stopped.load()||liveDataQuiet())return;
 xSemaphoreTake(stateMutex,portMAX_DELAY);if(ok){snprintf(haNote,sizeof(haNote),"%s",note);indoorTemperature=temperature;indoorAvailable=haveTemperature;}xSemaphoreGive(stateMutex);finishProvider(haProvider,ok);
}
void networkWorker(void*) {
 bool wasQuiet=false,warmupDone=false,firstSession=true;BatchSchedule regularBatches;
 for(;;){
  if(stopped.load()||deviceMaintenanceRequested()){ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));continue;}
  bool quiet=liveDataQuiet(),warmup=quiet&&powerMorningWarmup()&&!warmupDone;
  if(quiet&&!warmup){if(!wasQuiet)Serial.println("NIGHT_DATA_PAUSE: weather/TfL polling suspended; radio off.");wasQuiet=true;ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));continue;}
  bool morning=wasQuiet&&!quiet;if(morning){wasQuiet=false;Serial.println("NIGHT_DATA_RESUME: immediate morning batch.");}
  if(powerDisplayActive()){ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));continue;}
  uint32_t now=millis();bool requested=refreshRequested.exchange(false);
  bool regular=!warmup&&(requested||morning||regularBatches.due(now));
  bool wx=!warmup&&(regular||(!weatherProvider.latestOk&&weatherProvider.due(now)));
  bool lines=!warmup&&(regular||(!lineProvider.latestOk&&lineProvider.due(now)));
  bool stations=!warmup&&(regular||(!stationProvider.latestOk&&stationProvider.due(now)));
  bool ha=!warmup&&*deviceSettings().haUrl&&(regular||(!haProvider.latestOk&&haProvider.due(now)));
  bool ntp=warmup||regular||(!timeProvider.latestOk&&timeProvider.due(now));
  if(wx||lines||stations||ha||ntp){
   networkActive.store(true);powerNetworkBegin();uint32_t start=millis(),generation=ntpGeneration.load();sessionDeadline.store(start+DASH_POWER_SESSION_BUDGET_MS);
   if(stopped.load()||deviceMaintenanceRequested()||(liveDataQuiet()&&!warmup)){powerNetworkEnd();networkActive=false;continue;}
   if(regular)regularBatches.begin(start);
   Serial.printf("POWER_RADIO_ON: weather=%u lines=%u stations=%u ntp=%u ha=%u\n",wx,lines,stations,ntp,ha);
   connectSaved(firstSession);firstSession=false;
   while(WiFi.status()!=WL_CONNECTED&&millis()-start<15000&&!stopped.load()&&!deviceMaintenanceRequested()&&(!liveDataQuiet()||warmup))vTaskDelay(pdMS_TO_TICKS(50));
   bool connected=WiFi.status()==WL_CONNECTED;
   if(connected){
    if(ntp){configTzTime(LOCAL_TIMEZONE,"pool.ntp.org","time.cloudflare.com","time.google.com");esp_sntp_set_sync_interval(DASH_FULL_INTERVAL_MS);}
    while(time(nullptr)<MIN_VALID_EPOCH&&millis()-start<DASH_POWER_SESSION_BUDGET_MS&&!stopped.load()&&!deviceMaintenanceRequested())vTaskDelay(pdMS_TO_TICKS(50));
    if(!warmup&&!stopped.load()&&!liveDataQuiet()&&!deviceMaintenanceRequested()){
     Serial.println("DATA_BATCH: independently tracked providers; interval=10 minutes");
     if(wx){bool ok=fetchLiveData();finishProvider(weatherProvider,ok,ok?weather.observedAt:0);}
     if(lines||stations)fetchTransportData(lines,stations);
     if(ha)fetchHomeAssistant();
    }
    uint32_t waitStart=millis();while(ntp&&ntpGeneration.load()==generation&&millis()-waitStart<8000&&millis()-start<DASH_POWER_SESSION_BUDGET_MS&&!stopped.load()&&!deviceMaintenanceRequested())vTaskDelay(pdMS_TO_TICKS(50));
   }else{if(wx)finishProvider(weatherProvider,false);if(lines)finishProvider(lineProvider,false);if(stations)finishProvider(stationProvider,false);if(ha)finishProvider(haProvider,false);}
   if(ntp)finishProvider(timeProvider,ntpGeneration.load()!=generation,ntpEpoch.load());
   if(warmup)warmupDone=true;
   networkHealthy.store(connected&&weatherProvider.latestOk);
   esp_sntp_stop();WiFi.disconnect(true,false);WiFi.mode(WIFI_OFF);
   Serial.printf("POWER_RADIO_OFF: session_ms=%lu connected=%u NTP_new=%u batch_version=%lu\n",(unsigned long)(millis()-start),connected,ntpGeneration.load()!=generation,(unsigned long)(batchVersion.load()+1));
   powerNetworkEnd();networkActive.store(false);batchVersion.fetch_add(1);
  }
  ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));
 }
}

}
bool getLiveDashboard(DashboardData &data,time_t displayAt) {
  // Only copy state under the mutex: no network/flash I/O on the render path.
  WeatherSnapshot w{};TransportSnapshot t{};bool available,restored,weatherOk;
  xSemaphoreTake(stateMutex,portMAX_DELAY);
  memcpy(&w,&weather,sizeof(w));t=transport;available=haveWeather;restored=restoredWeather;weatherOk=weatherProvider.latestOk;
  xSemaphoreGive(stateMutex);
  int64_t now=time(nullptr);bool currentAvailable=available&&!sourceExpired(w.observedAt,now,7200);
  composeDashboard(data,dashboardText,available?&w:nullptr,restored,t,now,displayAt,networkHealthy.load(),DASH_TRANSPORT_STALE_SECONDS,deviceSettings().location);
  data.weatherAvailable=currentAvailable;
  const auto &cfg=deviceSettings();data.fahrenheit=cfg.fahrenheit;data.windMph=cfg.windMph;
  if(cfg.fahrenheit){data.temperature=lround(data.temperature*1.8+32);for(auto &h:data.forecast)h.temperature=lround(h.temperature*1.8+32);}
  if(cfg.windMph)data.windSpeed*=.621371;
  const auto &east=journeyDetail(t.dlr,t.eastIndia),&tube=journeyDetail(t.jubilee,t.canningTube);
  auto detail=[&](const TransportCheck &check,char *label,char *reason,ServiceHealth health){
    snprintf(label,33,"%s",health==ServiceHealth::Good?"GOOD":health==ServiceHealth::Stale?"OLD":health==ServiceHealth::Unknown?"UNKNOWN":*check.label?check.label:health==ServiceHealth::Issue?"ISSUE":"NOTICE");
    if(health==ServiceHealth::Good&&check.plannedAt>now){time_t when=check.plannedAt;tm local{};localtime_r(&when,&local);char stamp[24];strftime(stamp,sizeof(stamp),"%a %H:%M",&local);snprintf(reason,161,"Planned %s: %s",stamp,check.planned);}
    else snprintf(reason,161,"%s",check.reason);
  };
  detail(east,dlrLabel,dlrReason,journeyHealth(data.dlr,data.eastIndia));detail(tube,tubeLabel,tubeReason,journeyHealth(data.jubilee,data.canningTown));
  data.dlrLabel=dlrLabel;data.jubileeLabel=tubeLabel;data.dlrReason=dlrReason;data.jubileeReason=tubeReason;
  if(strcmp(cfg.dlrStation,"940GZZDLEIN"))data.dlrName="DLR / SELECTED STATION";
  if(strcmp(cfg.jubileeStation,"940GZZLUCGT"))data.jubileeName="JUBILEE / SELECTED STATION";
  weatherAdvice[0]=0;
  if(currentAvailable&&w.extrasAvailable){float feels=cfg.fahrenheit?w.apparentTemperature*1.8+32:w.apparentTemperature;snprintf(weatherAdvice,sizeof(weatherAdvice),"FEELS %.0f%s / GUSTS %.0f %s",feels,cfg.fahrenheit?"F":"C",w.gustSpeed*(cfg.windMph?.621371:1),cfg.windMph?"MPH":"KM/H");}
  if(available&&now>=MIN_VALID_EPOCH){for(int i=0;i<w.hourCount;i++)if(w.hourly[i].epoch>=now&&w.hourly[i].epoch<=now+8*3600&&w.hourly[i].rainProbability>50){time_t when=w.hourly[i].epoch;tm local{};localtime_r(&when,&local);char hour[8];strftime(hour,sizeof(hour),"%H:%M",&local);size_t length=strlen(weatherAdvice);snprintf(weatherAdvice+length,sizeof(weatherAdvice)-length,"%sRAIN LIKELY %s",length?" / ":"",hour);break;}}
  char extra[101]="";xSemaphoreTake(stateMutex,portMAX_DELAY);bool haFresh=haProvider.latestOk&&!sourceExpired(haProvider.fetchedAt,now,900);if(haFresh){if(indoorAvailable)snprintf(extra,sizeof(extra)," / IN %.0f%s",cfg.fahrenheit?indoorTemperature*1.8+32:indoorTemperature,cfg.fahrenheit?"F":"C");if(*haNote){size_t n=strlen(extra);snprintf(extra+n,sizeof(extra)-n," / %.60s",haNote);}}xSemaphoreGive(stateMutex);
  auto battery=batteryReading();data.batteryPercent=battery.available?battery.percent:-1;
  char batteryNote[40]="";if(battery.available)snprintf(batteryNote,sizeof(batteryNote)," / BAT %.0f%%%s",battery.percent,battery.percent<=15?" CHARGE":"");
  char stamp[16]="--";if(available){time_t fetched=w.fetchedAt;tm local{};localtime_r(&fetched,&local);strftime(stamp,sizeof(stamp),"%H:%M",&local);}
  snprintf(statusFooter,sizeof(statusFooter),"%s%sWX %s %s / OPEN-METEO%s%s%s",*batteryNote?batteryNote+3:"",*batteryNote?" / ":"",!available?"UNAVAILABLE":!currentAvailable?"EXPIRED":restored?"CACHED":weatherOk?"LIVE":"OLD",stamp,*weatherAdvice?" / ":"",weatherAdvice,extra);data.statusText=statusFooter;
  return true;
}

bool transportRepaintPending(){return repaintTransport;}
void clearTransportRepaint(){repaintTransport=false;}

bool liveDataQuiet() {
  const uint32_t until=sleepPreviewUntil.load();
  time_t now=time(nullptr);tm local{};localtime_r(&now,&local);
  return (now>=MIN_VALID_EPOCH&&quietAtHour(local.tm_hour,deviceSettings().quietStart,deviceSettings().quietEnd)) || (until && int32_t(until-millis())>0);
}

void stopLiveData(){stopped.store(true);}

bool liveNetworkActive(){return networkActive.load();}
bool livePrepareFullRefresh(int64_t target){
 if(preparedTarget!=target){
   if(networkActive.load())return false;
   uint32_t now=millis();bool due; xSemaphoreTake(stateMutex,portMAX_DELAY);
   due=weatherProvider.due(now)||lineProvider.due(now)||stationProvider.due(now);
   xSemaphoreGive(stateMutex);
   preparedTarget=target;preparationStarted=now;if(due)refreshRequested.store(true);else return true;
 }
 return (!refreshRequested.load()&&!networkActive.load())||uint32_t(millis()-preparationStarted)>=DASH_POWER_SESSION_BUDGET_MS+5000;
}
void liveDiagnostics(JsonDocument &d){
 xSemaphoreTake(stateMutex,portMAX_DELAY);
 auto provider=[&](const char *name,const ProviderState &s){d[name]["latest_ok"]=s.latestOk;d[name]["fetched_at"]=s.fetchedAt;d[name]["source_at"]=s.sourceAt;d[name]["failures"]=s.failures;d[name]["retry_ms"]=s.retryMs;};
 provider("weather",weatherProvider);provider("tfl_lines",lineProvider);provider("tfl_stations",stationProvider);provider("ntp",timeProvider);provider("home_assistant",haProvider);
 d["dlr_reason"]=transport.dlr.reason;d["jubilee_reason"]=transport.jubilee.reason;
 xSemaphoreGive(stateMutex);d["clock_epoch"]=int64_t(time(nullptr));d["ntp_last_sync"]=ntpEpoch.load();d["batch_version"]=batchVersion.load();d["network_active"]=networkActive.load();d["quiet"]=liveDataQuiet();
}
