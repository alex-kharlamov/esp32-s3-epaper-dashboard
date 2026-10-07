#pragma once
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
struct DeviceSettings {
  double latitude = 0, longitude = 0;
  char location[33]{}, timezone[80]{}, dlrStation[25]{}, jubileeStation[25]{};
  uint8_t quietStart = 0, quietEnd = 7;
  bool fahrenheit = false, windMph = false, rtcEnabled = false,
       gaugeEnabled = false;
  char haUrl[161]{}, haToken[257]{}, haTemperature[81]{}, haNote[81]{};
};
inline bool settingsText(const char *s, size_t length, bool required = false) {
  if (!memchr(s, 0, length) || (required && !*s))
    return false;
  for (; *s; s++)
    if ((unsigned char)*s < 32 || (unsigned char)*s > 126 || *s == '<' ||
        *s == '>')
      return false;
  return true;
}
inline bool stationId(const char *s) {
  if (strncmp(s, "940GZZ", 6))
    return false;
  for (; *s; s++)
    if (!((*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9')))
      return false;
  return true;
}
inline bool entityId(const char *s) {
  for (; *s; s++)
    if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '_' ||
          *s == '.'))
      return false;
  return true;
}
inline bool posixTimezone(const char *zone) {
  const char *p = zone;
  auto name = [&]() {
    int n = 0;
    while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
      ++p;
      ++n;
    }
    return n >= 3 && n <= 10;
  };
  auto number = [&](int low, int high) {
    if (*p < '0' || *p > '9')
      return false;
    char *end;
    long n = strtol(p, &end, 10);
    p = end;
    return n >= low && n <= high;
  };
  auto offset = [&](int max) {
    if (*p == '+' || *p == '-')
      ++p;
    if (!number(0, max))
      return false;
    if (*p == ':') {
      ++p;
      if (!number(0, 59))
        return false;
      if (*p == ':') {
        ++p;
        if (!number(0, 59))
          return false;
      }
    }
    return true;
  };
  if (!name() || !offset(24))
    return false;
  if (!*p)
    return true;
  if (!name())
    return false;
  if (*p == '+' || *p == '-' || (*p >= '0' && *p <= '9'))
    if (!offset(24))
      return false;
  for (int i = 0; i < 2; i++) {
    if (*p++ != ',')
      return false;
    if (*p == 'M') {
      ++p;
      if (!number(1, 12) || *p++ != '.' || !number(1, 5) || *p++ != '.' ||
          !number(0, 6))
        return false;
    } else if (*p == 'J') {
      ++p;
      if (!number(1, 365))
        return false;
    } else if (!number(0, 365))
      return false;
    if (*p == '/') {
      ++p;
      if (!offset(167))
        return false;
    }
  }
  return !*p;
}
inline bool validSettings(const DeviceSettings &s) {
  return isfinite(s.latitude) && isfinite(s.longitude) &&
         fabs(s.latitude) <= 90 && fabs(s.longitude) <= 180 &&
         settingsText(s.location, sizeof(s.location), true) &&
         settingsText(s.timezone, sizeof(s.timezone), true) &&
         posixTimezone(s.timezone) &&
         settingsText(s.dlrStation, sizeof(s.dlrStation), true) &&
         stationId(s.dlrStation) &&
         settingsText(s.jubileeStation, sizeof(s.jubileeStation), true) &&
         stationId(s.jubileeStation) && s.quietStart < 24 && s.quietEnd < 24 &&
         settingsText(s.haUrl, sizeof(s.haUrl)) &&
         (!*s.haUrl || !strncmp(s.haUrl, "https://", 8)) &&
         settingsText(s.haToken, sizeof(s.haToken)) &&
         settingsText(s.haTemperature, sizeof(s.haTemperature)) &&
         entityId(s.haTemperature) &&
         settingsText(s.haNote, sizeof(s.haNote)) && entityId(s.haNote);
}
inline bool quietAtHour(int hour, int start, int end) {
  return start == end  ? false
         : start < end ? hour >= start && hour < end
                       : hour >= start || hour < end;
}
inline uint32_t settingsChecksum(const DeviceSettings &s) {
  uint32_t h = 2166136261u;
  const auto *b = (const uint8_t *)&s;
  for (size_t i = 0; i < sizeof(s); i++)
    h = (h ^ b[i]) * 16777619u;
  return h;
}
