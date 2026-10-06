#include "LiveData.h"
#include "Configuration.h"
#include "WeatherCache.h"
#include "WeatherParsing.h"
#include "TransportData.h"
#include "DashboardState.h"
#include "QuietHours.h"
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
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace {
constexpr double LAT=DASH_LATITUDE,LON=DASH_LONGITUDE;
constexpr const char *LOCAL_TIMEZONE=DASH_TIMEZONE;
WeatherSnapshot weather{};
TransportSnapshot transport{};
SemaphoreHandle_t stateMutex=nullptr;
DashboardText dashboardText{};
bool haveWeather=false,restoredWeather=false,repaintTransport=false;
std::atomic<uint32_t> sleepPreviewUntil{0};
void networkWorker(void*);
String setupLine;
uint32_t lastWifiTry=0;
bool getJson(const String &url,JsonDocument &doc) {
  if(liveDataQuiet())return false;
  NetworkClientSecure tls;tls.useBuiltinCACertBundle();tls.setHandshakeTimeout(12);
  HTTPClient http;http.setConnectTimeout(10000);http.setTimeout(12000);
  http.setUserAgent("ESP32-ePaper-Dashboard/1.1");
  if(!http.begin(tls,url))return false;
  int code=http.GET();
  if(code!=200){Serial.printf("Data HTTP failure: %d\n",code);http.end();return false;}
  if(http.getSize()>60000){http.end();return false;}
  String body=http.getString();http.end();
  if(body.length()>60000)return false;
  auto error=deserializeJson(doc,body);
  if(error){Serial.println("Data JSON parse failed");return false;}
  return true;
}
void connectSaved(bool scan=false) {
  WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);WiFi.setSleep(false);
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
  lastWifiTry=millis();
  Serial.printf("WIFI credentials: %s\n",ssid.length()?"local setup credentials loaded":"trying ESP32 system configuration");
  pass.clear();
  Serial.println("Connecting using existing ESP32 Wi-Fi configuration; credentials are not logged.");
}
}
bool beginLiveData() {
  stateMutex=xSemaphoreCreateMutex();if(!stateMutex)return false;
  Preferences cache;
  if(cache.begin("dashboardwx",true)) {
    WeatherRecord record{};
    if(cache.getBytesLength("snapshot")==sizeof(record) && cache.getBytes("snapshot",&record,sizeof(record))==sizeof(record) && loadWeatherRecord(&record,sizeof(record),LAT,LON,weather)) {
      haveWeather=restoredWeather=true;
      Serial.printf("WEATHER_CACHE_RESTORED: fetched_epoch=%lld hours=%u\n",(long long)weather.fetchedAt,weather.hourCount);
    }else Serial.println("WEATHER_CACHE: no valid cache for this location.");
    cache.end();
  }
  setenv("TZ",LOCAL_TIMEZONE,1);tzset();
  configTzTime(LOCAL_TIMEZONE,"pool.ntp.org","time.cloudflare.com","time.google.com");
  esp_sntp_set_sync_interval(15*60*1000);
  esp_sntp_set_time_sync_notification_cb([](struct timeval *tv) {
    Serial.printf("NTP_SYNC: epoch=%lld.%06ld\n",(long long)tv->tv_sec,(long)tv->tv_usec);
  });
  WiFi.onEvent([](arduino_event_id_t event,arduino_event_info_t info) {
    if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED)Serial.printf("WIFI disconnected: reason=%d\n",info.wifi_sta_disconnected.reason);
    if(event==ARDUINO_EVENT_WIFI_STA_GOT_IP)Serial.println("WIFI connected; IP obtained.");
  });
  connectSaved(true);
  Serial.printf("LIVE: %s / timezone=%s; clock=%lu ms, full/weather=%lu ms.\n",DASH_LOCATION_LABEL,DASH_TIMEZONE,(unsigned long)DASH_CLOCK_INTERVAL_MS,(unsigned long)DASH_FULL_INTERVAL_MS);
  Serial.println("If Wi-Fi is not configured, run tools/wifi_setup.py locally over USB.");
  return xTaskCreate(networkWorker,"dashboard-data",16384,nullptr,1,nullptr)==pdPASS;
}
void serviceLiveSetup() {
  if(!liveDataQuiet() && WiFi.status()!=WL_CONNECTED && millis()-lastWifiTry>=30000)connectSaved();
  static bool reported=false;
  if(!reported && millis()>15000) {
    reported=true;
    Serial.printf("WIFI startup: %s; status=%d.\n",WiFi.status()==WL_CONNECTED?"connected":"disconnected",int(WiFi.status()));
  }
  while(Serial.available()) {
    char c=Serial.read();if(c=='\r')continue;
    if(c!='\n'){if(setupLine.length()<512)setupLine+=c;else setupLine.clear();continue;}
    if(setupLine.startsWith("WIFI ")) {
      JsonDocument doc;
      if(!deserializeJson(doc,setupLine.substring(5))&&doc["ssid"].is<const char*>()&&doc["password"].is<const char*>()) {
        String ssid=doc["ssid"].as<String>(),pass=doc["password"].as<String>();
        if(ssid.length()>0&&ssid.length()<=32&&pass.length()<=63) {
          Preferences p;p.begin("dashboardwifi",false);
          bool ok=p.putString("ssid",ssid)>0;
          ok=(p.putString("pass",pass)>0)&&ok;p.end();
          if(ok){Serial.println("WIFI configuration saved; connecting.");WiFi.disconnect();connectSaved(true);}
          else Serial.println("WIFI configuration could not be saved.");
        }else Serial.println("WIFI configuration has invalid lengths.");
        pass.clear();
      }else Serial.println("WIFI configuration invalid.");
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
    "&current=temperature_2m,wind_speed_10m,wind_direction_10m,weather_code,is_day"
    "&hourly=temperature_2m,precipitation_probability,weather_code,is_day&forecast_days=2&timeformat=unixtime&timezone=UTC";
  JsonDocument doc;WeatherSnapshot next{};
  if(!getJson(url,doc) || !parseWeatherResponse(doc.as<JsonVariantConst>(),time(nullptr),LAT,LON,next)) {
    Serial.println("WEATHER_FETCH_FAILED: keeping last good snapshot.");return false;
  }
  if(liveDataQuiet())return false; // Discard a request crossing midnight.
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
void fetchTransportData() {
  if(liveDataQuiet())return;
  TransportSnapshot next{};
  xSemaphoreTake(stateMutex,portMAX_DELAY);next=transport;xSemaphoreGive(stateMutex);
  JsonDocument lines,stations;
  bool linesOk=getJson("https://api.tfl.gov.uk/Line/dlr,jubilee/Status?detail=true",lines);
  time_t now=time(nullptr);
  updateTransportCheck(next.dlr,linesOk?parseLineStatus(lines.as<JsonVariantConst>(),"dlr",now):ServiceHealth::Unknown,now);
  updateTransportCheck(next.jubilee,linesOk?parseLineStatus(lines.as<JsonVariantConst>(),"jubilee",now):ServiceHealth::Unknown,now);
  bool stationsOk=getJson("https://api.tfl.gov.uk/StopPoint/940GZZDLCGT,940GZZLUCGT,940GZZDLEIN/Disruption?includeRouteBlockedStops=false",stations);
  now=time(nullptr);
  updateTransportCheck(next.canningDlr,stationsOk?parseStationStatus(stations.as<JsonVariantConst>(),"940GZZDLCGT",now):ServiceHealth::Unknown,now);
  updateTransportCheck(next.canningTube,stationsOk?parseStationStatus(stations.as<JsonVariantConst>(),"940GZZLUCGT",now):ServiceHealth::Unknown,now);
  updateTransportCheck(next.eastIndia,stationsOk?parseStationStatus(stations.as<JsonVariantConst>(),"940GZZDLEIN",now):ServiceHealth::Unknown,now);
  if(liveDataQuiet())return; // Do not publish overnight state.
  xSemaphoreTake(stateMutex,portMAX_DELAY);transport=next;xSemaphoreGive(stateMutex);
  Serial.printf("TFL_FETCH: lines=%s stations=%s DLR=%u Jubilee=%u CanningTown=%u EastIndia=%u\n",linesOk?"OK":"FAILED",stationsOk?"OK":"FAILED",unsigned(next.dlr.health),unsigned(next.jubilee.health),unsigned(combineStationHealth(next.canningDlr.health,next.canningTube.health)),unsigned(next.eastIndia.health));
}
void networkWorker(void*) {
  bool batchAttempted=false,lastWeatherOk=false,wasQuiet=false;
  uint32_t lastBatchStarted=0,lastWeatherAttempt=0;
  for(;;) {
    if(liveDataQuiet()) {
      if(!wasQuiet)Serial.println("NIGHT_DATA_PAUSE: weather/TfL polling suspended.");
      wasQuiet=true;batchAttempted=false;
      vTaskDelay(pdMS_TO_TICKS(250));continue;
    }
    if(wasQuiet){wasQuiet=false;Serial.println("NIGHT_DATA_RESUME: immediate morning batch.");}
    if(WiFi.status()==WL_CONNECTED && time(nullptr)>=MIN_VALID_EPOCH) {
      uint32_t now=millis();
      if(!batchAttempted || now-lastBatchStarted>=DASH_FULL_INTERVAL_MS) {
        batchAttempted=true;lastBatchStarted=now;
        Serial.println("DATA_BATCH: weather + TfL; interval=10 minutes");
        lastWeatherOk=fetchLiveData();lastWeatherAttempt=millis();
        fetchTransportData();
      }else if(!lastWeatherOk && now-lastWeatherAttempt>=30000) {
        // Weather recovery can retry sooner without increasing TfL polling.
        lastWeatherOk=fetchLiveData();lastWeatherAttempt=millis();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
}
bool getLiveDashboard(DashboardData &data,time_t displayAt) {
  // Only copy state under the mutex: no network/flash I/O on the render path.
  WeatherSnapshot w{};TransportSnapshot t{};bool available,restored;
  xSemaphoreTake(stateMutex,portMAX_DELAY);
  memcpy(&w,&weather,sizeof(w));t=transport;available=haveWeather;restored=restoredWeather;
  xSemaphoreGive(stateMutex);
  composeDashboard(data,dashboardText,available?&w:nullptr,restored,t,time(nullptr),displayAt,WiFi.status()==WL_CONNECTED,DASH_TRANSPORT_STALE_SECONDS,DASH_LOCATION_LABEL);
  return true;
}

bool transportRepaintPending(){return repaintTransport;}
void clearTransportRepaint(){repaintTransport=false;}

bool liveDataQuiet() {
  const uint32_t until=sleepPreviewUntil.load();
  return quietHours(time(nullptr)) || (until && int32_t(until-millis())>0);
}
