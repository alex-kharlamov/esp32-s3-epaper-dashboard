#pragma once
#include "Dashboard.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>
struct TransportCheck {
  ServiceHealth health=ServiceHealth::Unknown;
  int64_t checkedAt=0;
  bool latestRequestOk=false;
};
struct TransportSnapshot { TransportCheck dlr,jubilee,canningDlr,canningTube,eastIndia; };
inline ServiceHealth currentTransportHealth(const TransportCheck &c,int64_t now,bool online,uint32_t staleSeconds) {
  if(c.health==ServiceHealth::Unknown || !c.checkedAt || now<1700000000LL)return ServiceHealth::Unknown;
  if(!online || !c.latestRequestOk || now<c.checkedAt || now-c.checkedAt>staleSeconds)return ServiceHealth::Stale;
  return c.health;
}
inline ServiceHealth combineStationHealth(ServiceHealth a,ServiceHealth b) {
  if(a==ServiceHealth::Issue || b==ServiceHealth::Issue)return ServiceHealth::Issue;
  if(a==ServiceHealth::Notice || b==ServiceHealth::Notice)return ServiceHealth::Notice;
  if(a==ServiceHealth::Stale || b==ServiceHealth::Stale)return ServiceHealth::Stale;
  if(a==ServiceHealth::Unknown || b==ServiceHealth::Unknown)return ServiceHealth::Unknown;
  return ServiceHealth::Good;
}
inline int64_t transportEpoch(const char *text) {
  if(!text || strlen(text)<19)return -1;
  int y,m,d,h,minute,second,n=0;
  if(sscanf(text,"%4d-%2d-%2dT%2d:%2d:%2d%n",&y,&m,&d,&h,&minute,&second,&n)!=6 || n!=19)return -1;
  if(y<1970 || y>2100 || m<1 || m>12 || h<0 || h>23 || minute<0 || minute>59 || second<0 || second>59)return -1;
  auto leap=[](int year){return year%4==0 && (year%100!=0 || year%400==0);};
  const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
  if(d<1 || d>lengths[m-1]+(m==2 && leap(y)))return -1;
  int64_t days=0;for(int year=1970;year<y;year++)days+=365+leap(year);
  for(int month=1;month<m;month++)days+=lengths[month-1]+(month==2 && leap(y));days+=d-1;
  const char *suffix=text+19;
  if(*suffix=='.'){suffix++;if(*suffix<'0'||*suffix>'9')return -1;while(*suffix>='0'&&*suffix<='9')suffix++;}
  int offset=0;
  if(*suffix=='+' || *suffix=='-') {
    int oh,om,used=0;char sign=*suffix;
    if(sscanf(suffix+1,"%2d:%2d%n",&oh,&om,&used)!=2 || used!=5 || suffix[6] || oh>23 || om>59 || oh<0 || om<0)return -1;
    offset=(oh*60+om)*60*(sign=='+'?1:-1);
  }else if(*suffix && strcmp(suffix,"Z"))return -1;
  return days*86400+h*3600+minute*60+second-offset;
}
// Missing boundaries are open-ended; malformed dates never mean good service.
inline int transportPeriod(JsonObjectConst item,int64_t now) {
  int64_t from=0,to=INT64_MAX;
  if(!item["fromDate"].isNull()) {
    if(!item["fromDate"].is<const char*>() || (from=transportEpoch(item["fromDate"].as<const char*>()))<0)return -1;
  }
  if(!item["toDate"].isNull()) {
    if(!item["toDate"].is<const char*>() || (to=transportEpoch(item["toDate"].as<const char*>()))<0)return -1;
  }
  if(to<from)return -1;return now>=from && now<to;
}
inline ServiceHealth parseLineStatus(JsonVariantConst root,const char *lineId,int64_t now) {
  if(!root.is<JsonArrayConst>())return ServiceHealth::Unknown;
  bool good=false,issue=false,uncertain=false,found=false;
  for(JsonObjectConst line:root.as<JsonArrayConst>()) {
    if(strcmp(line["id"]|"",lineId))continue;found=true;
    if(!line["lineStatuses"].is<JsonArrayConst>())return ServiceHealth::Unknown;
    for(JsonObjectConst status:line["lineStatuses"].as<JsonArrayConst>()) {
      bool active=true;
      if(!status["validityPeriods"].isNull()) {
        if(!status["validityPeriods"].is<JsonArrayConst>()){uncertain=true;continue;}
        JsonArrayConst periods=status["validityPeriods"].as<JsonArrayConst>();
        if(periods.size()) {
          active=false;
          for(JsonVariantConst value:periods) {
            if(!value.is<JsonObjectConst>()){uncertain=true;continue;}
            JsonObjectConst period=value.as<JsonObjectConst>();
            if(period["fromDate"].isNull() && period["toDate"].isNull() && !(period["isNow"].is<bool>() && period["isNow"].as<bool>())){uncertain=true;continue;}
            int current=transportPeriod(period,now);
            if(current<0)uncertain=true;else if(current)active=true;
          }
        }
      }
      if(!active)continue;
      if(!status["statusSeverity"].is<int>()){uncertain=true;continue;}
      int severity=status["statusSeverity"];
      if(severity==10)good=true;else if(severity>=1 && severity<=20)issue=true;else uncertain=true;
    }
  }
  if(issue)return ServiceHealth::Issue;
  if(!found || uncertain || !good)return ServiceHealth::Unknown;
  return ServiceHealth::Good;
}
inline ServiceHealth parseStationStatus(JsonVariantConst root,const char *stationId,int64_t now) {
  if(!root.is<JsonArrayConst>())return ServiceHealth::Unknown;
  bool notice=false,uncertain=false;
  for(JsonVariantConst value:root.as<JsonArrayConst>()) {
    if(!value.is<JsonObjectConst>()){uncertain=true;continue;}
    JsonObjectConst item=value.as<JsonObjectConst>();
    const char *id=item["stationAtcoCode"]|"";
    if(!*id)id=item["atcoCode"]|"";
    if(!*id){uncertain=true;continue;}if(strcmp(id,stationId))continue;
    int active=transportPeriod(item,now);
    if(active<0){uncertain=true;continue;}if(!active)continue;
    const char *type=item["type"]|"";
    if(!*type){uncertain=true;continue;}
    if(strstr(type,"Closure") || strstr(type,"Closed") || strstr(type,"Suspended"))return ServiceHealth::Issue;
    notice=true;
  }
  if(notice)return ServiceHealth::Notice;
  return uncertain?ServiceHealth::Unknown:ServiceHealth::Good;
}
