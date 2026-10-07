#pragma once
#include "SettingsModel.h"
#include <ArduinoJson.h>
void settingsBegin();
const DeviceSettings &deviceSettings();
bool settingsSave(JsonVariantConst values,String &error);
void settingsJson(JsonDocument &doc);
