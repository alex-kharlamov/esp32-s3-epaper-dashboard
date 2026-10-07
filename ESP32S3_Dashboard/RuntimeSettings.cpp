#include "RuntimeSettings.h"
#include "Configuration.h"
#include <Preferences.h>
namespace {
DeviceSettings settings;
struct Record {
  uint32_t magic, version, checksum;
  DeviceSettings value;
};
void defaults() {
  settings = {};
  settings.latitude = DASH_LATITUDE;
  settings.longitude = DASH_LONGITUDE;
  snprintf(settings.location, sizeof(settings.location), "%s",
           DASH_LOCATION_LABEL);
  snprintf(settings.timezone, sizeof(settings.timezone), "%s", DASH_TIMEZONE);
  strcpy(settings.dlrStation, "940GZZDLEIN");
  strcpy(settings.jubileeStation, "940GZZLUCGT");
}
bool stringField(JsonVariantConst v, const char *key, char *out, size_t n) {
  if (v[key].isNull())
    return true;
  if (!v[key].is<const char *>())
    return false;
  const char *s = v[key];
  if (strlen(s) >= n)
    return false;
  strcpy(out, s);
  return true;
}
} // namespace
void settingsBegin() {
  defaults();
  Preferences p;
  if (p.begin("dashsettings", true)) {
    Record r{};
    if (p.getBytesLength("record") == sizeof(r) &&
        p.getBytes("record", &r, sizeof(r)) == sizeof(r) &&
        r.magic == 0x44534346 && r.version == 1 &&
        r.checksum == settingsChecksum(r.value) && validSettings(r.value))
      settings = r.value;
    p.end();
  }
}
const DeviceSettings &deviceSettings() { return settings; }
bool settingsSave(JsonVariantConst v, String &error) {
  if (!v.is<JsonObjectConst>()) {
    error = "Settings must be an object";
    return false;
  }
  DeviceSettings next = settings;
  if (!v["latitude"].isNull()) {
    if (!v["latitude"].is<double>()) {
      error = "Latitude must be numeric";
      return false;
    }
    next.latitude = v["latitude"];
  }
  if (!v["longitude"].isNull()) {
    if (!v["longitude"].is<double>()) {
      error = "Longitude must be numeric";
      return false;
    }
    next.longitude = v["longitude"];
  }
  if (!stringField(v, "location", next.location, sizeof(next.location)) ||
      !stringField(v, "timezone", next.timezone, sizeof(next.timezone)) ||
      !stringField(v, "dlr_station", next.dlrStation,
                   sizeof(next.dlrStation)) ||
      !stringField(v, "jubilee_station", next.jubileeStation,
                   sizeof(next.jubileeStation)) ||
      !stringField(v, "ha_url", next.haUrl, sizeof(next.haUrl)) ||
      !stringField(v, "ha_token", next.haToken, sizeof(next.haToken)) ||
      !stringField(v, "ha_temperature", next.haTemperature,
                   sizeof(next.haTemperature)) ||
      !stringField(v, "ha_note", next.haNote, sizeof(next.haNote))) {
    error = "Invalid or overlong text";
    return false;
  }
  const char *hours[] = {"quiet_start", "quiet_end"};
  for (int i = 0; i < 2; i++)
    if (!v[hours[i]].isNull()) {
      if (!v[hours[i]].is<int>() || v[hours[i]].as<int>() < 0 ||
          v[hours[i]].as<int>() > 23) {
        error = "Quiet hours must be 0-23";
        return false;
      }
      if (i)
        next.quietEnd = v[hours[i]];
      else
        next.quietStart = v[hours[i]];
    }
  const char *flags[] = {"fahrenheit", "wind_mph", "rtc_enabled",
                         "gauge_enabled"};
  bool *targets[] = {&next.fahrenheit, &next.windMph, &next.rtcEnabled,
                     &next.gaugeEnabled};
  for (int i = 0; i < 4; i++)
    if (!v[flags[i]].isNull()) {
      if (!v[flags[i]].is<bool>()) {
        error = "Flags must be boolean";
        return false;
      }
      *targets[i] = v[flags[i]];
    }
  if (!validSettings(next)) {
    error = "Invalid coordinates, station, timezone, or HTTPS Home Assistant "
            "settings";
    return false;
  }
  Record r{};
  r.magic = 0x44534346;
  r.version = 1;
  memcpy(&r.value, &next, sizeof(next));
  r.checksum = settingsChecksum(r.value);
  Preferences p;
  if (!p.begin("dashsettings", false)) {
    error = "Cannot open settings storage";
    return false;
  }
  bool ok = p.putBytes("record", &r, sizeof(r)) == sizeof(r);
  p.end();
  if (!ok)
    error = "Cannot save settings";
  return ok;
}
void settingsJson(JsonDocument &d) {
  const auto &s = settings;
  d["latitude"] = s.latitude;
  d["longitude"] = s.longitude;
  d["location"] = s.location;
  d["timezone"] = s.timezone;
  d["dlr_station"] = s.dlrStation;
  d["jubilee_station"] = s.jubileeStation;
  d["quiet_start"] = s.quietStart;
  d["quiet_end"] = s.quietEnd;
  d["fahrenheit"] = s.fahrenheit;
  d["wind_mph"] = s.windMph;
  d["rtc_enabled"] = s.rtcEnabled;
  d["gauge_enabled"] = s.gaugeEnabled;
  d["ha_url"] = s.haUrl;
  d["ha_temperature"] = s.haTemperature;
  d["ha_note"] = s.haNote;
  d["ha_token_configured"] = bool(*s.haToken);
}
