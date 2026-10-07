#pragma once
#include <stdint.h>
#include <ArduinoJson.h>
struct BatteryReading {bool available=false;float volts=0,percent=0;uint32_t readMs=0;};
void hardwareBegin();
void hardwareService();
void hardwareNtpSynced(int64_t epoch);
BatteryReading batteryReading();
void hardwareJson(JsonDocument &d);
