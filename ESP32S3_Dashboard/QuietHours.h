#pragma once
#include <time.h>
// Use local civil time, including London DST. Invalid time never enters quiet mode.
inline bool quietHours(time_t now) {
  if(now<1700000000)return false;
  tm local{};localtime_r(&now,&local);
  return local.tm_hour>=0 && local.tm_hour<7;
}
enum class QuietAction { None, Sleep, Wake };
struct QuietDisplayState {
  bool sleeping=false;
  QuietAction update(bool quiet) {
    if(quiet==sleeping)return QuietAction::None;
    sleeping=quiet;return quiet?QuietAction::Sleep:QuietAction::Wake;
  }
};
