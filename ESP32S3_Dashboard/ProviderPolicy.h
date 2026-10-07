#pragma once
#include <stdint.h>
struct ProviderState {
  int64_t fetchedAt = 0, sourceAt = 0;
  uint32_t attemptedMs = 0, retryMs = 30000;
  unsigned failures = 0;
  bool attempted = false, latestOk = false;
  void finish(bool ok, int64_t now, uint32_t elapsedMs, int64_t source = 0) {
    attempted = true;
    attemptedMs = elapsedMs;
    latestOk = ok;
    if (ok) {
      fetchedAt = now;
      sourceAt = source ? source : now;
      failures = 0;
      retryMs = 30000;
    } else {
      ++failures;
      retryMs = 30000;
      for (unsigned i = 1; i < failures && retryMs < 600000; i++)
        retryMs *= 2;
      if (retryMs > 600000)
        retryMs = 600000;
    }
  }
  bool due(uint32_t now, uint32_t interval = 600000) const {
    return !attempted ||
           uint32_t(now - attemptedMs) >= (latestOk ? interval : retryMs);
  }
};
inline bool sourceExpired(int64_t source, int64_t now, uint32_t limit) {
  return source < 1700000000 || now < source || now - source > limit;
}

// Normal providers share a batch deadline; failure retries do not move it.
struct BatchSchedule {
  uint32_t startedMs = 0;
  bool started = false;
  bool due(uint32_t now, uint32_t interval = 600000) const {
    return !started || uint32_t(now - startedMs) >= interval;
  }
  void begin(uint32_t now) { startedMs = now; started = true; }
};
