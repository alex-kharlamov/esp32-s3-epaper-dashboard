#pragma once
#include <stdint.h>
struct ClockWindow {uint16_t x,y,width,height;};
// Up to four changed digits; adjacent changed digits are merged.
int changedClockWindows(const char *previous,const char *current,ClockWindow windows[4]);
