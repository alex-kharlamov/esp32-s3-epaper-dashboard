#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>
constexpr int WEATHER_HOURS=48;
constexpr int64_t MIN_VALID_EPOCH=1700000000LL;
struct WeatherHour {
  int64_t epoch;
  int16_t temperature, code;
  uint8_t rainProbability, isDay;
  float precipitation=0;
  bool precipitationAvailable=false;
};
// Persist values, not DashboardData's pointers into RAM or flash.
struct WeatherSnapshot {
  int64_t fetchedAt, observedAt;
  double latitude, longitude;
  float windSpeed;
  int16_t temperature, windDirection, code;
  uint8_t isDay, hourCount;
  WeatherHour hourly[WEATHER_HOURS];
  float apparentTemperature=0,gustSpeed=0;
  bool extrasAvailable=false;
};
struct WeatherRecord {
  uint32_t magic, version, size, checksum;
  WeatherSnapshot weather;
};
inline uint32_t weatherChecksum(const WeatherSnapshot &w) {
  const uint8_t *bytes=reinterpret_cast<const uint8_t*>(&w);
  uint32_t result=2166136261u;
  for(size_t i=0;i<sizeof(w);i++)result=(result^bytes[i])*16777619u;
  return result;
}
inline bool validWeatherCode(int c) {
  return c==0 || c==1 || c==2 || c==3 || c==45 || c==48 ||
    (c>=51 && c<=57 && c%2==1) || c==56 || (c>=61 && c<=67 && c%2==1) || c==66 ||
    (c>=71 && c<=77 && c%2==1) || (c>=80 && c<=82) || c==85 || c==86 || c==95 || c==96 || c==99;
}
inline bool validWeather(const WeatherSnapshot &w,double latitude,double longitude) {
  if(!isfinite(w.latitude) || !isfinite(w.longitude) || fabs(w.latitude-latitude)>0.000001 || fabs(w.longitude-longitude)>0.000001)return false;
  if(w.fetchedAt<MIN_VALID_EPOCH || w.observedAt<MIN_VALID_EPOCH || w.fetchedAt>4102444800LL || w.observedAt>w.fetchedAt+3600)return false;
  if(w.temperature < -90 || w.temperature>80)return false;
  if(!isfinite(w.windSpeed) || w.windSpeed<0 || w.windSpeed>400 || w.windDirection<0 || w.windDirection>360)return false;
  if(w.extrasAvailable && (!isfinite(w.apparentTemperature)||w.apparentTemperature < -100||w.apparentTemperature>100||!isfinite(w.gustSpeed)||w.gustSpeed<0||w.gustSpeed>500))return false;
  if(!validWeatherCode(w.code) || w.isDay>1 || !w.hourCount || w.hourCount>WEATHER_HOURS)return false;
  for(int i=0;i<w.hourCount;i++) {
    const auto &h=w.hourly[i];
    if(h.epoch<MIN_VALID_EPOCH || h.epoch>4102444800LL || (i && h.epoch!=w.hourly[i-1].epoch+3600))return false;
    if(h.precipitationAvailable && (!isfinite(h.precipitation)||h.precipitation<0||h.precipitation>500))return false;
    if(h.temperature < -90 || h.temperature>80 || h.rainProbability>100 || h.isDay>1 || !validWeatherCode(h.code))return false;
  }
  return true;
}
inline WeatherRecord makeWeatherRecord(const WeatherSnapshot &w) {
  WeatherRecord result{};result.magic=0x57584348;result.version=2;result.size=sizeof(w);
  memcpy(&result.weather,&w,sizeof(w));result.checksum=weatherChecksum(result.weather);return result;
}
struct LegacyWeatherHour {int64_t epoch;int16_t temperature,code;uint8_t rainProbability,isDay;};
struct LegacyWeatherSnapshot {int64_t fetchedAt,observedAt;double latitude,longitude;float windSpeed;int16_t temperature,windDirection,code;uint8_t isDay,hourCount;LegacyWeatherHour hourly[WEATHER_HOURS];};
struct LegacyWeatherRecord {uint32_t magic,version,size,checksum;LegacyWeatherSnapshot weather;};
inline bool loadWeatherRecord(const void *bytes,size_t length,double latitude,double longitude,WeatherSnapshot &out) {
  if(length==sizeof(LegacyWeatherRecord)) {
    LegacyWeatherRecord old{};memcpy(&old,bytes,sizeof(old));uint32_t hash=2166136261u;const auto *raw=(const uint8_t*)&old.weather;for(size_t i=0;i<sizeof(old.weather);i++)hash=(hash^raw[i])*16777619u;
    if(old.magic!=0x57584348||old.version!=1||old.size!=sizeof(old.weather)||hash!=old.checksum||old.weather.hourCount>WEATHER_HOURS)return false;
    WeatherSnapshot migrated{};auto &v=old.weather;migrated.fetchedAt=v.fetchedAt;migrated.observedAt=v.observedAt;migrated.latitude=v.latitude;migrated.longitude=v.longitude;migrated.windSpeed=v.windSpeed;migrated.temperature=v.temperature;migrated.windDirection=v.windDirection;migrated.code=v.code;migrated.isDay=v.isDay;migrated.hourCount=v.hourCount;
    for(int i=0;i<v.hourCount;i++){auto &h=v.hourly[i];migrated.hourly[i]={h.epoch,h.temperature,h.code,h.rainProbability,h.isDay};}
    if(!validWeather(migrated,latitude,longitude))return false;out=migrated;return true;
  }
  if(length!=sizeof(WeatherRecord))return false;
  WeatherRecord record{};memcpy(&record,bytes,sizeof(record));
  if(record.magic!=0x57584348 || record.version!=2 || record.size!=sizeof(WeatherSnapshot) || record.checksum!=weatherChecksum(record.weather))return false;
  if(!validWeather(record.weather,latitude,longitude))return false;
  memcpy(&out,&record.weather,sizeof(out));return true;
}
inline int forecastHourIndex(const WeatherSnapshot &w,int64_t epoch) {
  for(int i=0;i<w.hourCount;i++)if(w.hourly[i].epoch==epoch)return i;
  return -1;
}
