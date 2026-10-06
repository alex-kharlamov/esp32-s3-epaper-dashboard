#pragma once
#include <stdint.h>
// Return a display timestamp, or -1 while waiting. Rendering the upcoming minute
// compensates for the panel waveform: the image should settle at its boundary.
inline int64_t clockDisplayTarget(int64_t now,int64_t displayed,uint32_t lead,uint32_t interval=60000) {
  int64_t current=now/interval*interval,next=current+interval;
  if(displayed<0)return (now+lead)/interval*interval;
  if(next!=displayed && next-now<=lead)return next;
  if(current!=displayed && (now>=displayed || displayed-now>lead+2000))return current;
  return -1;
}
