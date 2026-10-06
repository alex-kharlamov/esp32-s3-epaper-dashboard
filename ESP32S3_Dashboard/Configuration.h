#pragma once
#include <stdint.h>
// Generic London defaults. Change these for your own location and rebuild.
constexpr double DASH_LATITUDE = 51.5074;
constexpr double DASH_LONGITUDE = -0.1278;
constexpr const char *DASH_LOCATION_LABEL = "London";
// POSIX timezone rule, not an IANA name. This handles UK daylight-saving changes.
constexpr const char *DASH_TIMEZONE = "GMT0BST,M3.5.0/1,M10.5.0";
constexpr uint32_t DASH_CLOCK_INTERVAL_MS = 60000;
constexpr uint32_t DASH_FULL_INTERVAL_MS = 600000;
constexpr uint32_t DASH_BOOT_COOLDOWN_MS = 180000;
// Set false to use full-screen fast updates between ten-minute normal refreshes.
constexpr bool DASH_CLOCK_WINDOW_ENABLED = true;
// Verified workaround on our board. Units are quarter-dBm: 34 = 8.5 dBm.
constexpr int8_t DASH_WIFI_TX_POWER = 34;
