#pragma once
#include "TransportData.h"
struct TransportRecord {
  uint32_t magic = 0x54464348, version = 1, checksum = 0;
  char dlrStation[25]{}, jubileeStation[25]{};
  TransportSnapshot snapshot;
};
inline uint32_t transportChecksum(const TransportRecord &r) {
  uint32_t h = 2166136261u;
  const auto *p = (const uint8_t *)&r;
  for (size_t i = offsetof(TransportRecord, dlrStation); i < sizeof(r); i++)
    h = (h ^ p[i]) * 16777619u;
  return h;
}
inline bool loadTransportRecord(const TransportRecord &r, const char *dlr,
                                const char *tube, TransportSnapshot &out) {
  if (r.magic != 0x54464348 || r.version != 1 ||
      r.checksum != transportChecksum(r) ||
      !memchr(r.dlrStation, 0, sizeof(r.dlrStation)) ||
      !memchr(r.jubileeStation, 0, sizeof(r.jubileeStation)) ||
      strcmp(r.dlrStation, dlr) || strcmp(r.jubileeStation, tube))
    return false;
  const TransportCheck *checks[] = {
      &r.snapshot.dlr, &r.snapshot.jubilee, &r.snapshot.canningDlr,
      &r.snapshot.canningTube, &r.snapshot.eastIndia};
  for (auto *c : checks) {
    if (unsigned(c->health) > unsigned(ServiceHealth::Stale) ||
        (c->checkedAt && c->checkedAt < 1700000000) ||
        !memchr(c->label, 0, sizeof(c->label)) ||
        !memchr(c->reason, 0, sizeof(c->reason)) ||
        !memchr(c->planned, 0, sizeof(c->planned)))
      return false;
  }
  out = r.snapshot;
  out.dlr.latestRequestOk = out.jubilee.latestRequestOk =
      out.canningDlr.latestRequestOk = out.canningTube.latestRequestOk =
          out.eastIndia.latestRequestOk = false;
  return true;
}
