#include "HardwareServices.h"
#include "HardwareModel.h"
#include "RuntimeSettings.h"
#include "TransportData.h"
#include "WeatherCache.h"
#include <Wire.h>
#include <atomic>
#include <sys/time.h>
namespace {
constexpr int SDA_PIN = 17, SCL_PIN = 18;
BatteryReading battery;
std::atomic<int64_t> rtcWriteEpoch{0};
bool rtcPresent = false, rtcTimeValid = false, busStarted = false;
uint32_t lastRead = 0;
bool readRegisters(uint8_t address, uint8_t reg, uint8_t *out, size_t n) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false))
    return false;
  if (Wire.requestFrom(address, uint8_t(n)) != n)
    return false;
  for (size_t i = 0; i < n; i++)
    out[i] = Wire.read();
  return true;
}
bool writeRegisters(uint8_t address, uint8_t reg, const uint8_t *data,
                    size_t n) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(data, n);
  return Wire.endTransmission() == 0;
}
int bcd(uint8_t n) { return (n >> 4) * 10 + (n & 15); }
uint8_t encode(int n) { return (n / 10 << 4) | (n % 10); }
void readBattery() {
  uint8_t version[2], values[4];
  battery.available = false;
  float voltage, soc;
  if (!readRegisters(0x36, 0x08, version, 2) ||
      !readRegisters(0x36, 0x02, values, 4) ||
      !decodeBattery(version, values, voltage, soc))
    return;
  battery = {true, voltage, soc, millis()};
}
} // namespace
void hardwareBegin() {
  const auto &s = deviceSettings();
  if (!s.rtcEnabled && !s.gaugeEnabled)
    return;
  busStarted = Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Wire.setTimeOut(50);
  if (!busStarted)
    return;
  if (s.rtcEnabled) {
    uint8_t status = 0, r[7];
    rtcPresent = readRegisters(0x68, 0x0f, &status, 1);
    if (rtcPresent && !(status & 0x80) && readRegisters(0x68, 0, r, 7)) {
      int64_t epoch = decodeRtcEpoch(r, status);
      if (epoch >= MIN_VALID_EPOCH && epoch < 4102444800LL) {
        timeval tv{time_t(epoch), 0};
        if (time(nullptr) < MIN_VALID_EPOCH)
          settimeofday(&tv, nullptr);
        rtcTimeValid = true;
      }
    }
  }
  if (s.gaugeEnabled)
    readBattery();
  Serial.printf("HARDWARE: RTC=%s gauge=%s; absent/invalid readings remain "
                "unavailable.\n",
                rtcTimeValid ? "valid"
                : rtcPresent ? "time invalid"
                             : "unavailable",
                battery.available ? "available" : "unavailable");
}
void hardwareNtpSynced(int64_t epoch) { rtcWriteEpoch.store(epoch); }
void hardwareService() {
  if (!busStarted)
    return;
  int64_t epoch = rtcWriteEpoch.exchange(0);
  if (deviceSettings().rtcEnabled && epoch >= MIN_VALID_EPOCH) {
    time_t current = time(nullptr);
    if (current >= MIN_VALID_EPOCH)
      epoch = current;
    tm utc{};
    time_t t = epoch;
    gmtime_r(&t, &utc);
    if (utc.tm_year >= 100 && utc.tm_year < 200) {
      uint8_t r[] = {encode(utc.tm_sec),       encode(utc.tm_min),
                     encode(utc.tm_hour),      encode(utc.tm_wday + 1),
                     encode(utc.tm_mday),      encode(utc.tm_mon + 1),
                     encode(utc.tm_year - 100)};
      uint8_t status;
      if (writeRegisters(0x68, 0, r, 7) &&
          readRegisters(0x68, 0x0f, &status, 1)) {
        status &= ~0x80;
        rtcTimeValid = writeRegisters(0x68, 0x0f, &status, 1);
        rtcPresent = true;
      }
    }
  }
  if (deviceSettings().gaugeEnabled && millis() - lastRead >= 30000) {
    lastRead = millis();
    readBattery();
  }
}
BatteryReading batteryReading() {
  BatteryReading b = battery;
  if (millis() - b.readMs > 90000)
    b.available = false;
  return b;
}
void hardwareJson(JsonDocument &d) {
  auto b = batteryReading();
  d["rtc_enabled"] = deviceSettings().rtcEnabled;
  d["rtc_present"] = rtcPresent;
  d["rtc_time_valid"] = rtcTimeValid;
  d["gauge_enabled"] = deviceSettings().gaugeEnabled;
  d["battery_available"] = b.available;
  if (b.available) {
    d["battery_volts"] = b.volts;
    d["battery_percent"] = b.percent;
    d["battery_low"] = b.percent <= 15;
  }
}
