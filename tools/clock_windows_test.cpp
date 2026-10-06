#include "Dashboard.h"
#include "ClockUpdate.h"
#include "ClockSchedule.h"
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
  auto w=windows[i];if(w.x%4 || w.width%4 || w.x<472 || w.x+w.width>1340 || w.y!=40 || w.height!=284)abort();
  // A valid HH:MM never refreshes the colon: digits 0/1 and 3/4 are separate groups.
  if(w.x<906 && w.x+w.width>906)abort();
 }
 for(int y=40;y<324;y++)for(int x=472;x<1340;x++) {
  int idx=y*340+x/4,shift=6-2*(x%4);
  if(((oldFrame[idx]^newFrame[idx])>>shift)&3) {
   bool covered=false;for(int i=0;i<count;i++) {auto w=windows[i];covered|=x>=w.x && x<w.x+w.width && y>=w.y && y<w.y+w.height;}
   if(!covered) {fprintf(stderr,"Uncovered %s -> %s at %d,%d\n",before,after,x,y);abort();}
  }
 }
}
int main() {
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
 puts("Wall-clock schedule boundaries, lateness, NTP corrections and startup passed.");
 char before[6],after[6];
 for(int minute=0;minute<1440;minute++) {
  int next=(minute+1)%1440;
  snprintf(before,sizeof(before),"%02d:%02d",minute/60,minute%60);
  snprintf(after,sizeof(after),"%02d:%02d",next/60,next%60);check(before,after);
 }
 check("12:34","12:34");check("02:59","02:00");check("01:59","03:00");check("19:48","07:13");
 ClockWindow w[4];int n=changedClockWindows("12:34","12:35",w);
 if(n!=1 || w[0].x<1130)abort();
 n=changedClockWindows("12:39","12:40",w);if(n!=1 || w[0].x<940)abort();
 n=changedClockWindows("09:59","10:00",w);if(n!=2)abort();
 puts("Clock windows cover all 1440 minute transitions, midnight, time corrections and unchanged time; colon excluded.");
}
