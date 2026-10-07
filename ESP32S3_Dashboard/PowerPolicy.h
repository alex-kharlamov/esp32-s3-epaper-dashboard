#pragma once
#include <stdint.h>
#include <time.h>
inline uint32_t powerRetryDelay(unsigned failures) {
  uint32_t wait=30000;while(failures>1 && wait<600000){wait*=2;--failures;}
  return wait>600000?600000:wait;
}
inline time_t powerMorningWake(time_t now,unsigned earlySeconds=300,unsigned quietStart=0,unsigned quietEnd=7) {
  if(now<1700000000)return 0;
  tm local{};localtime_r(&now,&local);
  bool quiet=quietStart==quietEnd?false:quietStart<quietEnd?local.tm_hour>=int(quietStart)&&local.tm_hour<int(quietEnd):local.tm_hour>=int(quietStart)||local.tm_hour<int(quietEnd);
  if(!quiet)return 0;
  int originalHour=local.tm_hour;
  local.tm_hour=quietEnd;local.tm_min=local.tm_sec=0;local.tm_isdst=-1;
  time_t morning=mktime(&local);
  if(morning<=now){local.tm_mday++;local.tm_isdst=-1;morning=mktime(&local);}
  (void)originalHour;
  return morning>now+earlySeconds?morning-earlySeconds:now;
}
inline uint32_t powerCooldownRemaining(bool timerWake,int64_t nowMs,int64_t lastPanelMs,uint32_t cooldown) {
  if(!timerWake || lastPanelMs<1700000000000LL || nowMs<lastPanelMs)return cooldown;
  int64_t elapsed=nowMs-lastPanelMs;
  return elapsed>=cooldown?0:uint32_t(cooldown-elapsed);
}
inline uint32_t powerClockWait(int64_t now,int64_t shown,uint32_t lead,uint32_t interval=60000) {
  int64_t next=(now/interval+1)*interval;
  if(shown==next)next+=interval;
  int64_t wait=next-now-lead;
  return wait<=0?20:uint32_t(wait>1000?1000:wait);
}
