#pragma once
#include "WeatherCache.h"
#include <ArduinoJson.h>
inline bool weatherNumber(JsonVariantConst v,double low,double high) {
  return v.is<double>() && isfinite(v.as<double>()) && v.as<double>()>=low && v.as<double>()<=high;
}
inline bool parseWeatherResponse(JsonVariantConst root,int64_t now,double lat,double lon,WeatherSnapshot &out) {
  JsonObjectConst c=root["current"].as<JsonObjectConst>();
  if(!weatherNumber(c["temperature_2m"],-90,80) || !weatherNumber(c["wind_speed_10m"],0,400) ||
     !weatherNumber(c["wind_direction_10m"],0,360) || !c["weather_code"].is<int>() || !c["is_day"].is<int>() || !c["time"].is<int64_t>())return false;
  JsonObjectConst h=root["hourly"].as<JsonObjectConst>();
  if(!h["time"].is<JsonArrayConst>())return false;
  int count=h["time"].size();if(count<8 || count>WEATHER_HOURS)return false;
  const char *keys[]={"temperature_2m","precipitation_probability","weather_code","is_day"};
  for(const char *key:keys)
    if(!h[key].is<JsonArrayConst>() || int(h[key].size())!=count)return false;
  WeatherSnapshot w{};w.latitude=lat;w.longitude=lon;w.fetchedAt=now;w.observedAt=c["time"];
  w.temperature=lround(c["temperature_2m"].as<double>());w.windSpeed=c["wind_speed_10m"];
  w.windDirection=lround(c["wind_direction_10m"].as<double>());
  int code=c["weather_code"],day=c["is_day"];
  if(!validWeatherCode(code) || day<0 || day>1)return false;
  w.code=code;w.isDay=day;w.hourCount=count;int future=0;
  if(weatherNumber(c["apparent_temperature"],-100,100)&&weatherNumber(c["wind_gusts_10m"],0,500)){w.apparentTemperature=c["apparent_temperature"];w.gustSpeed=c["wind_gusts_10m"];w.extrasAvailable=true;}
  for(int i=0;i<count;i++) {
    if(!h["time"][i].is<int64_t>() || !weatherNumber(h["temperature_2m"][i],-90,80) ||
       !weatherNumber(h["precipitation_probability"][i],0,100) || !h["weather_code"][i].is<int>() || !h["is_day"][i].is<int>())return false;
    code=h["weather_code"][i];day=h["is_day"][i];
    if(!validWeatherCode(code) || day<0 || day>1)return false;
    w.hourly[i]={h["time"][i].as<int64_t>(),int16_t(lround(h["temperature_2m"][i].as<double>())),int16_t(code),uint8_t(lround(h["precipitation_probability"][i].as<double>())),uint8_t(day)};
    if(h["precipitation"].is<JsonArrayConst>()&&int(h["precipitation"].size())==count&&weatherNumber(h["precipitation"][i],0,500)){w.hourly[i].precipitation=h["precipitation"][i];w.hourly[i].precipitationAvailable=true;}
    if(w.hourly[i].epoch>now)future++;
  }
  if(future<8 || !validWeather(w,lat,lon))return false;
  memcpy(&out,&w,sizeof(w));return true;
}
