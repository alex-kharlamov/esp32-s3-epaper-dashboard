#pragma once
#include "WeatherCache.h"
#include "TransportData.h"
#include <time.h>
struct DashboardText {
  char clock[6],date[40],weekday[8],hours[8][6],status[180];
};
inline const char *snapshotWeatherIcon(int code,bool day) {
  if(code==0)return day?"icon_sun":"icon_night";
  if(code<=2)return day?"icon_partly-cloudy-day":"icon_clouds";
  if(code<=48)return "icon_clouds";
  if(code>=95)return "icon_storm";
  if((code>=71 && code<=77) || code==85 || code==86)return "icon_snow";
  if(code==65 || code==67 || code==82)return "icon_heavy_rain";
  return "icon_rain";
}
inline void composeDashboard(DashboardData &data,DashboardText &text,const WeatherSnapshot *w,bool restored,
  const TransportSnapshot &t,time_t now,time_t displayAt,bool online,uint32_t staleSeconds,const char *location) {
  time_t clockAt=displayAt?displayAt:now;bool timeAvailable=now>=MIN_VALID_EPOCH;
  data={};data.weatherAvailable=w!=nullptr;data.location=location;
  if(timeAvailable) {
    tm local{};localtime_r(&clockAt,&local);
    strftime(text.clock,sizeof(text.clock),"%H:%M",&local);strftime(text.date,sizeof(text.date),"%e %b",&local);
    const char *days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};snprintf(text.weekday,sizeof(text.weekday),"%s",days[local.tm_wday]);
  }else {
    snprintf(text.clock,sizeof(text.clock),"--:--");snprintf(text.date,sizeof(text.date),"Waiting for network time");text.weekday[0]=0;
  }
  data.clock=text.clock;data.date=text.date;data.weekday=text.weekday;data.sampleData=false;
  if(w) {
    data.temperature=w->temperature;data.windSpeed=w->windSpeed;data.windDirection=w->windDirection;
    data.weatherIcon=snapshotWeatherIcon(w->code,w->isDay);
  }else {data.windSpeed=-1;data.weatherIcon="";}
  for(int i=0;i<8;i++) {
    time_t epoch=(clockAt/3600+1+i)*3600;
    if(timeAvailable){tm local{};localtime_r(&epoch,&local);strftime(text.hours[i],sizeof(text.hours[i]),"%H:%M",&local);}
    else snprintf(text.hours[i],sizeof(text.hours[i]),"--:--");
    int j=w && timeAvailable?forecastHourIndex(*w,epoch):-1;
    if(j>=0){const auto &h=w->hourly[j];data.forecast[i]={text.hours[i],h.temperature,snapshotWeatherIcon(h.code,h.isDay),h.rainProbability,true};}
    else data.forecast[i]={text.hours[i],0,"",0,false};
  }
  // A successful TfL check is independent of weather/API reachability.
  auto health=[&](const TransportCheck &check){return currentTransportHealth(check,now,check.latestRequestOk,staleSeconds);};
  data.dlr=health(t.dlr);data.jubilee=health(t.jubilee);
  data.canningTown=health(t.canningTube);data.eastIndia=health(t.eastIndia);
  if(t.dlr.checkedAt && t.jubilee.checkedAt && t.canningTube.checkedAt && t.eastIndia.checkedAt) {
    data.transportCheckedAt=t.dlr.checkedAt;
    const int64_t stamps[]={t.jubilee.checkedAt,t.canningTube.checkedAt,t.eastIndia.checkedAt};
    for(auto stamp:stamps)
      if(stamp<data.transportCheckedAt)data.transportCheckedAt=stamp;
  }
  char stamp[32]="--";
  if(w){time_t fetched=w->fetchedAt;tm local{};localtime_r(&fetched,&local);strftime(stamp,sizeof(stamp),"%d %b %H:%M",&local);}
  const char *state=!w?"UNAVAILABLE":(!timeAvailable || restored)?"CACHED":(!online || now<w->fetchedAt || now-w->fetchedAt>600)?"STALE":"LIVE";
  snprintf(text.status,sizeof(text.status),"WEATHER %s %s / OPEN-METEO%s",state,stamp,!online?" / OFFLINE":"");
  data.statusText=text.status;
}
