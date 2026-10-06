#include "LiveData.h"
#include "Configuration.h"
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

namespace {
constexpr double LAT=DASH_LATITUDE,LON=DASH_LONGITUDE;
constexpr const char *LOCAL_TIMEZONE=DASH_TIMEZONE;
DashboardData live{};
char clockText[6],dateText[40],weekdayText[8],hours[8][6],status[110];
time_t fetchedAt=0;
bool haveWeather=false;
String setupLine;
uint32_t lastWifiTry=0;
const char *weatherIcon(int code,bool day=true) {
  if(code==0)return day?"icon_sun":"icon_night";
  if(code<=2)return day?"icon_partly-cloudy-day":"icon_clouds";
  if(code<=48)return "icon_clouds";
  if(code>=95)return "icon_storm";
  if((code>=71&&code<=77)||code==85||code==86)return "icon_snow";
  if(code==65||code==67||code==82)return "icon_heavy_rain";
  return "icon_rain";
}
bool getJson(const String &url,JsonDocument &doc) {
  NetworkClientSecure tls;tls.useBuiltinCACertBundle();tls.setHandshakeTimeout(12);
  HTTPClient http;http.setConnectTimeout(10000);http.setTimeout(12000);
  if(!http.begin(tls,url))return false;
  int code=http.GET();
  if(code!=200){Serial.printf("Weather HTTP failure: %d\n",code);http.end();return false;}
  String body=http.getString();http.end();
  if(body.length()>60000)return false;
  auto error=deserializeJson(doc,body);
  if(error){Serial.println("Weather JSON parse failed");return false;}
  return true;
}
bool number(JsonVariantConst v) {return v.is<float>()&&isfinite(v.as<float>());}
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
void beginLiveData() {
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
}
void serviceLiveSetup() {
  if(WiFi.status()!=WL_CONNECTED && millis()-lastWifiTry>=30000)connectSaved();
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
    }else if(setupLine=="STATUS")Serial.printf("LIVE status: WiFi=%s time=%s weather=%s\n",WiFi.status()==WL_CONNECTED?"connected":"disconnected",time(nullptr)>1700000000?"synced":"waiting",haveWeather?"available":"waiting");
    setupLine.clear();
  }
}
bool fetchLiveData() {
  time_t now=time(nullptr);
  if(WiFi.status()!=WL_CONNECTED){Serial.println("Live data waiting for saved Wi-Fi connection.");return false;}
  if(now<1700000000){Serial.println("Live data waiting for NTP time (required for TLS).");return false;}
  String location="latitude="+String(LAT,6)+"&longitude="+String(LON,6);
  String url="https://api.open-meteo.com/v1/forecast?"+location+
    "&current=temperature_2m,relative_humidity_2m,pressure_msl,wind_speed_10m,wind_direction_10m,weather_code,is_day"
    "&hourly=temperature_2m,precipitation_probability,weather_code,uv_index,is_day&forecast_days=2&timeformat=unixtime&timezone=UTC";
  JsonDocument doc;if(!getJson(url,doc))return false;
  JsonObjectConst current=doc["current"].as<JsonObjectConst>();
  for(const char *key:{"temperature_2m","relative_humidity_2m","pressure_msl","wind_speed_10m","wind_direction_10m","weather_code","is_day"})
    if(!number(current[key])){Serial.println("Live weather missing current values; retaining previous data.");return false;}
  JsonObjectConst hourly=doc["hourly"].as<JsonObjectConst>();
  JsonArrayConst times=hourly["time"].as<JsonArrayConst>();
  int first=-1,currentHour=-1;
  for(unsigned i=0;i<times.size();i++) {
    if(!times[i].is<long long>())return false;
    time_t t=times[i].as<long long>();if(t<=now)currentHour=i;else if(first<0)first=i;
  }
  if(first<0||first+8>int(times.size())||currentHour<0)return false;
  for(const char *key:{"temperature_2m","precipitation_probability","weather_code","is_day","uv_index"})
    if(hourly[key].size()!=times.size())return false;
  for(int i=first;i<first+8;i++)for(const char *key:{"temperature_2m","precipitation_probability","weather_code","is_day"})
    if(!number(hourly[key][i]))return false;
  DashboardData next{};
  next.temperature=lround(current["temperature_2m"].as<float>());
  next.humidity=lround(current["relative_humidity_2m"].as<float>());
  next.pressure=lround(current["pressure_msl"].as<float>());
  next.windSpeed=current["wind_speed_10m"].as<float>();next.windDirection=lround(current["wind_direction_10m"].as<float>());
  next.weatherIcon=weatherIcon(current["weather_code"].as<int>(),current["is_day"].as<int>()!=0);
  next.uv=number(hourly["uv_index"][currentHour])?lround(hourly["uv_index"][currentHour].as<float>()):-1;
  for(int i=0;i<8;i++) {
    int j=first+i;time_t t=times[j].as<long long>();tm local{};localtime_r(&t,&local);
    strftime(hours[i],sizeof(hours[i]),"%H:%M",&local);
    next.forecast[i]={hours[i],int(lround(hourly["temperature_2m"][j].as<float>())),weatherIcon(hourly["weather_code"][j].as<int>(),hourly["is_day"][j].as<int>()!=0),int(lround(hourly["precipitation_probability"][j].as<float>()))};
  }
  next.aqi=-1;
  JsonDocument air;
  if(getJson("https://air-quality-api.open-meteo.com/v1/air-quality?"+location+"&current=us_aqi",air)&&number(air["current"]["us_aqi"]))next.aqi=lround(air["current"]["us_aqi"].as<float>());
  live=next;haveWeather=true;fetchedAt=time(nullptr);
  Serial.printf("LIVE fetch OK: temperature=%d humidity=%d AQI=%d; eight hourly forecasts.\n",live.temperature,live.humidity,live.aqi);
  return true;
}
bool getLiveDashboard(DashboardData &data,time_t displayAt) {
  time_t now=time(nullptr);if(!haveWeather||now<1700000000)return false;
  time_t clockAt=displayAt?displayAt:now;
  tm local{};localtime_r(&clockAt,&local);
  strftime(clockText,sizeof(clockText),"%H:%M",&local);strftime(dateText,sizeof(dateText),"%d %B %Y",&local);
  const char *days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};snprintf(weekdayText,sizeof(weekdayText),"%s",days[local.tm_wday]);
  tm updated{};localtime_r(&fetchedAt,&updated);char stamp[6];strftime(stamp,sizeof(stamp),"%H:%M",&updated);
  snprintf(status,sizeof(status),"%s  /  %s  /  OPEN-METEO  /  WEATHER %s  /  °C / RAIN CHANCE",now-fetchedAt>600?"STALE":"LIVE",DASH_LOCATION_LABEL,stamp);
  data=live;data.clock=clockText;data.date=dateText;data.weekday=weekdayText;data.statusText=status;data.sampleData=false;return true;
}
