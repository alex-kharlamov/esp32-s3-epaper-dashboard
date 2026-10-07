#pragma once
#include "TransportData.h"
inline void displayText(char *out, size_t n, const char *value) {
  size_t j = 0;
  bool space = false;
  for (const unsigned char *p = (const unsigned char *)(value ? value : "");
       *p && j + 1 < n; p++) {
    char c = *p >= 32 && *p < 127 && *p != '<' && *p != '>' ? char(*p) : ' ';
    if (c == ' ') {
      if (!j || space)
        continue;
      space = true;
    } else
      space = false;
    out[j++] = c;
  }
  while (j && out[j - 1] == ' ')
    --j;
  out[j] = 0;
}
inline void lineDetails(JsonVariantConst root, const char *id, int64_t now,
                        TransportCheck &out) {
  out.label[0] = out.reason[0] = out.planned[0] = 0;
  out.plannedAt = 0;
  for (JsonObjectConst line : root.as<JsonArrayConst>())
    if (!strcmp(line["id"] | "", id)) {
      for (JsonObjectConst status : line["lineStatuses"].as<JsonArrayConst>()) {
        bool active = status["validityPeriods"].isNull() ||
                      status["validityPeriods"].size() == 0;
        for (JsonObjectConst period :
             status["validityPeriods"].as<JsonArrayConst>()) {
          if (transportPeriod(period, now) == 1)
            active = true;
          int64_t future = transportEpoch(period["fromDate"] | "");
          if (future > now && future <= now + 7 * 86400 &&
              (!out.plannedAt || future < out.plannedAt)) {
            out.plannedAt = future;
            displayText(out.planned, sizeof(out.planned),
                        status["statusSeverityDescription"] | "Planned works");
          }
        }
        int severity = status["statusSeverity"] | 0;
        if (active &&
            ((out.health == ServiceHealth::Good && severity == 10) ||
             (out.health == ServiceHealth::Issue && severity != 10))) {
          displayText(out.label, sizeof(out.label),
                      status["statusSeverityDescription"] | "Service issue");
          displayText(out.reason, sizeof(out.reason), status["reason"] | "");
          if (out.health == ServiceHealth::Issue)
            break;
        }
      }
    }
}
inline void stationDetails(JsonVariantConst root, const char *id, int64_t now,
                           TransportCheck &out) {
  out.label[0] = out.reason[0] = out.planned[0] = 0;
  out.plannedAt = 0;
  for (JsonObjectConst item : root.as<JsonArrayConst>()) {
    const char *station = item["stationAtcoCode"] | "";
    if (!*station)
      station = item["atcoCode"] | "";
    if (strcmp(station, id))
      continue;
    int64_t future = transportEpoch(item["fromDate"] | "");
    if (future > now && future <= now + 7 * 86400 &&
        (!out.plannedAt || future < out.plannedAt)) {
      out.plannedAt = future;
      displayText(out.planned, sizeof(out.planned),
                  item["description"] | "Planned station works");
    }
    if (transportPeriod(item, now) == 1) {
      const char *type = item["type"] | "";
      bool closure = strstr(type, "Closure") || strstr(type, "Closed") || strstr(type, "Suspended");
      if (out.health == ServiceHealth::Issue && !closure)
        continue;
      displayText(out.label, sizeof(out.label),
                  item["type"] | "Station notice");
      displayText(out.reason, sizeof(out.reason),
                  item["description"] | "Station notice");
      if (out.health == ServiceHealth::Issue)
        break;
    }
  }
}
inline const TransportCheck &journeyDetail(const TransportCheck &line,
                                           const TransportCheck &station) {
  if (station.health == ServiceHealth::Issue)
    return station;
  if (line.health == ServiceHealth::Issue)
    return line;
  if (station.health == ServiceHealth::Notice)
    return station;
  if (line.health != ServiceHealth::Good)
    return line;
  if (station.plannedAt &&
      (!line.plannedAt || station.plannedAt < line.plannedAt))
    return station;
  return line;
}
