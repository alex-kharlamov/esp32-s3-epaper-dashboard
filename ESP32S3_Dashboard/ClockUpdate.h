#pragma once
#include <stdint.h>
#include "Dashboard.h"
struct ClockWindow {uint16_t x,y,width,height;};
// Up to four changed digits; adjacent changed digits are merged.
int changedClockWindows(const char *previous,const char *current,ClockWindow windows[4]);
constexpr ClockWindow CLOCK_AREA={472,8,868,236};
constexpr ClockWindow TRANSPORT_WINDOW={0,252,1360,44};
inline bool transportStatusChanged(const DashboardData &before,const DashboardData &after) {
  return journeyHealth(before.dlr,before.eastIndia)!=journeyHealth(after.dlr,after.eastIndia) ||
    journeyHealth(before.jubilee,before.canningTown)!=journeyHealth(after.jubilee,after.canningTown);
}
inline int changedDashboardWindows(const char *previous,const DashboardData &before,const DashboardData &after,ClockWindow windows[4],bool forceTransport=false) {
  int count=changedClockWindows(previous,after.clock,windows);
  bool east=journeyHealth(before.dlr,before.eastIndia)!=journeyHealth(after.dlr,after.eastIndia);
  bool canning=journeyHealth(before.jubilee,before.canningTown)!=journeyHealth(after.jubilee,after.canningTown);
  if(forceTransport)east=canning=true;
  if(east || canning)windows[count++]={uint16_t(east?0:680),252,uint16_t(east && canning?1360:680),44};
  return count;
}
