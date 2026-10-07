#include "Dashboard.h"
#include "ClockUpdate.h"
#include "ClockSchedule.h"
#include "QuietHours.h"
#include "PowerPolicy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void check(const char *before,const char *after) {
 static uint8_t oldFrame[DASH_BYTES],newFrame[DASH_BYTES];
 DashboardData d=sampleDashboard();d.clock=before;renderDashboard(oldFrame,d);
 d.clock=after;renderDashboard(newFrame,d);
 ClockWindow windows[4];int count=changedClockWindows(before,after,windows);
 if(!strcmp(before,after) && count)abort();
 for(int i=0;i<count;i++) {
  auto w=windows[i];if(w.x%4 || w.width%4 || w.x<472 || w.x+w.width>1340 || w.y!=CLOCK_AREA.y || w.height!=CLOCK_AREA.height)abort();
  // A valid HH:MM never refreshes the colon: digits 0/1 and 3/4 are separate groups.
  if(w.x<906 && w.x+w.width>906)abort();
 }
 for(int y=CLOCK_AREA.y;y<CLOCK_AREA.y+CLOCK_AREA.height;y++)for(int x=472;x<1340;x++) {
  int idx=y*340+x/4,shift=6-2*(x%4);
  if(((oldFrame[idx]^newFrame[idx])>>shift)&3) {
   bool covered=false;for(int i=0;i<count;i++) {auto w=windows[i];covered|=x>=w.x && x<w.x+w.width && y>=w.y && y<w.y+w.height;}
   if(!covered) {fprintf(stderr,"Uncovered %s -> %s at %d,%d\n",before,after,x,y);abort();}
  }
 }
}
int main() {
 for(unsigned i=1;i<100;i++){uint32_t expected=i<6?(30000u<<(i-1)):600000u;if(powerRetryDelay(i)!=expected)abort();}
 if(powerCooldownRemaining(true,1800000000090LL,1800000000000LL,180000)!=179910)abort();
 if(powerCooldownRemaining(true,1800000180000LL,1800000000000LL,180000)!=0)abort();
 if(powerCooldownRemaining(false,1800000180000LL,1800000000000LL,180000)!=180000)abort();
 if(powerCooldownRemaining(true,1799999999999LL,1800000000000LL,180000)!=180000)abort();
 if(powerClockWait(53000,0,7000)!=20 || powerClockWait(50000,0,7000)!=1000 || powerClockWait(59000,60000,0)!=1000)abort();
 // Quiet mode uses London civil time, including both DST transitions.
 setenv("TZ","GMT0BST,M3.5.0/1,M10.5.0",1);tzset();
 if(quietHours(0))abort();
 const int months[]={0,6,2,9};const int days[]={6,6,29,25};
 for(int day=0;day<4;day++) {
  QuietDisplayState state;
  for(int hour=0;hour<24;hour++) {
   tm local{};local.tm_year=126;local.tm_mon=months[day];local.tm_mday=days[day];local.tm_hour=hour;local.tm_isdst=-1;
   time_t epoch=mktime(&local);tm normalized{};localtime_r(&epoch,&normalized);
   bool expected=normalized.tm_hour<7;
   time_t early=powerMorningWake(epoch);if(expected){tm wakeLocal{};localtime_r(&early,&wakeLocal);if(normalized.tm_hour<6 && (wakeLocal.tm_hour!=6 || wakeLocal.tm_min!=55))abort();if(early<epoch)abort();}else if(early!=0)abort();
   if(quietHours(epoch)!=expected)abort();
   QuietAction action=state.update(expected);
   if(hour==0 && action!=QuietAction::Sleep)abort();
   if(hour==7 && action!=QuietAction::Wake)abort();
   if(hour!=0 && hour!=7 && action!=QuietAction::None)abort();
   for(int repeat=0;repeat<100;repeat++)if(state.update(expected)!=QuietAction::None)abort();
  }
 }
 tm boundary{};boundary.tm_year=126;boundary.tm_mon=9;boundary.tm_mday=6;boundary.tm_hour=7;boundary.tm_isdst=-1;
 time_t wake=mktime(&boundary);
 if(!quietHours(wake-1) || quietHours(wake))abort();
 boundary.tm_hour=0;time_t midnight=mktime(&boundary);
 if(quietHours(midnight-1) || !quietHours(midnight))abort();
 puts("Power policy: retry cap, retained cooldown, clock deadlines and DST wake passed.");
 puts("Quiet hours: exact midnight/07:00 boundaries, invalid time, winter/summer/DST and one-shot sleep/wake passed.");
 // Before the deadline, at it, after a completed predictive update, and late.
 if(clockDisplayTarget(112999,60000,7000)!=-1)abort();
 if(clockDisplayTarget(113000,60000,7000)!=120000)abort();
 if(clockDisplayTarget(119000,120000,7000)!=-1)abort();
 if(clockDisplayTarget(120001,120000,7000)!=-1)abort();
 if(clockDisplayTarget(125000,60000,7000)!=120000)abort();
 // NTP jumps in both directions and a first frame spanning a minute change.
 if(clockDisplayTarget(185000,60000,7000)!=180000)abort();
 if(clockDisplayTarget(65000,180000,7000)!=60000)abort();
 if(clockDisplayTarget(118000,-1,21000)!=120000)abort();
 // Regression: an early completed frame makes the next dirty lead zero.
 // It must remain displayed even as lead estimates change before the boundary.
 const uint32_t leads[]={0,1000,7000,9000,21000,59000};
 for(int64_t now=60000;now<120000;now+=20)
  for(uint32_t lead:leads)if(clockDisplayTarget(now,120000,lead)!=-1)abort();
 if(clockDisplayTarget(117000,120000,0)!=-1)abort();
 if(clockDisplayTarget(119800,120000,0)!=-1)abort(); // small NTP step back
 if(clockDisplayTarget(120200,120000,0)!=-1)abort();
 // Simulate a full day with alternating fast/slow phases and early completion.
 // Recompute lead from dirty windows exactly as the device does after drawing.
 int64_t shown=-1;unsigned updates=0;
 for(int64_t now=0;now<86400000;now+=20) {
  int64_t next=(now/60000+1)*60000;
  uint32_t lead=shown==next?0:((next/60000)%10==0?21000:9000);
  int64_t target=clockDisplayTarget(now,shown,lead);
  if(target<0)continue;
  if(shown>=0 && target<=shown)abort();
  shown=target;++updates;
  now+=lead>3000?lead-3000:lead; // frame settles up to three seconds early
 }
 if(updates!=1441)abort(); // initial 00:00 plus each upcoming minute
 // A real large backward correction converges once, then holds steady.
 if(clockDisplayTarget(65000,180000,7000)!=60000)abort();
 if(clockDisplayTarget(65020,60000,7000)!=-1)abort();
 if(clockDisplayTarget(179000,60000,9000)!=180000)abort();
 puts("Schedule regression: early frames, changing leads, full-day monotonicity and NTP corrections passed.");
 char before[6],after[6];
 for(int minute=0;minute<1440;minute++) {
  int next=(minute+1)%1440;
  snprintf(before,sizeof(before),"%02d:%02d",minute/60,minute%60);
  snprintf(after,sizeof(after),"%02d:%02d",next/60,next%60);check(before,after);
 }
 check("12:34","12:34");check("02:59","02:00");check("01:59","03:00");check("19:48","07:13");
 ClockWindow w[4];int n=changedClockWindows("12:34","12:35",w);
 if(n!=1 || w[0].x<1100)abort();
 n=changedClockWindows("12:39","12:40",w);if(n!=1 || w[0].x<920)abort();
 n=changedClockWindows("09:59","10:00",w);if(n!=2)abort();
 puts("Clock windows cover all 1440 minute transitions, midnight, time corrections and unchanged time; colon excluded.");
}
