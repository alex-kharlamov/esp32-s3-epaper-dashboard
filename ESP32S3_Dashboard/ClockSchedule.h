#pragma once
#include <stdint.h>
// Return a display timestamp, or -1 while waiting. Rendering the upcoming minute
// compensates for the panel waveform: the image should settle at its boundary.
inline int64_t clockDisplayTarget(int64_t now,int64_t displayed,uint32_t lead,uint32_t interval=60000) {
  int64_t current=now/interval*interval,next=current+interval;
  if(displayed<0)return (now+lead)/interval*interval;
  // A predictive frame can finish early. Its next dirty-window estimate may be
  // zero, so never use that changing estimate to roll it back to this minute.
  // Also tolerate small backward NTP adjustments without bouncing the digits.
  if(displayed==next)return -1;
  if(next-now<=lead)return next;
  // Missed minutes or a larger NTP correction: converge once to wall time.
  if(current!=displayed)return current;
  return -1;
}
