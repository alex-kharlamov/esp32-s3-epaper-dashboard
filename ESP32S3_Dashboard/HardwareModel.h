#pragma once
#include "TransportData.h"
#include "WeatherCache.h"
inline bool validBcd(uint8_t value) {
  return (value & 15) <= 9 && (value >> 4) <= 9;
}
inline int bcdValue(uint8_t value) { return (value >> 4) * 10 + (value & 15); }
inline int64_t decodeRtcEpoch(const uint8_t *r, uint8_t status) {
  if (status & 0x80)
    return -1;
  if (!validBcd(r[0] & 0x7f) || !validBcd(r[1] & 0x7f) ||
      !validBcd(r[2] & ((r[2] & 0x40) ? 0x1f : 0x3f)) ||
      !validBcd(r[4] & 0x3f) || !validBcd(r[5] & 0x1f) || !validBcd(r[6]))
    return -1;
  int hour = bcdValue(r[2] & ((r[2] & 0x40) ? 0x1f : 0x3f));
  if (r[2] & 0x40) {
    if (hour < 1 || hour > 12)
      return -1;
    hour = hour % 12 + ((r[2] & 0x20) ? 12 : 0);
  }
  char iso[32];
  snprintf(iso, sizeof(iso), "%04d-%02d-%02dT%02d:%02d:%02dZ",
           2000 + bcdValue(r[6]) + ((r[5] & 0x80) ? 100 : 0),
           bcdValue(r[5] & 0x1f), bcdValue(r[4] & 0x3f), hour,
           bcdValue(r[1] & 0x7f), bcdValue(r[0] & 0x7f));
  int64_t epoch = transportEpoch(iso);
  return epoch >= MIN_VALID_EPOCH && epoch < 4102444800LL ? epoch : -1;
}
inline bool decodeBattery(const uint8_t *version, const uint8_t *values,
                          float &volts, float &percent) {
  if (version[0] || (version[1] & 0xf0) != 0x10)
    return false;
  volts = (values[0] << 8 | values[1]) * .000078125f;
  percent = values[2] + values[3] / 256.f;
  if (volts < 2.5 || volts > 4.5 || percent > 101)
    return false;
  if (percent > 100)
    percent = 100;
  return true;
}
