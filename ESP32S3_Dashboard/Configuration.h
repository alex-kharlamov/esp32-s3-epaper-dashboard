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
constexpr uint32_t DASH_TRANSPORT_INTERVAL_MS = DASH_FULL_INTERVAL_MS;
constexpr uint32_t DASH_TRANSPORT_STALE_SECONDS = DASH_FULL_INTERVAL_MS / 1000 + 180;
// Set false to use full-screen fast updates between ten-minute normal refreshes.
constexpr bool DASH_CLOCK_WINDOW_ENABLED = true;
// Measured 5.27 s clock waveform. Small coloured residue was observed.
// Use 0x08 for the original vendor dynamic mode (about 12.1 s, cleaner).
constexpr uint8_t DASH_CLOCK_PLL = 0x07;
// Verified workaround on our board. Units are quarter-dBm: 34 = 8.5 dBm.
constexpr int8_t DASH_WIFI_TX_POWER = 34;

// CPU/connection work stays at 240 MHz; idle uses 80 MHz.
constexpr uint32_t DASH_POWER_IDLE_CPU_MHZ=80;
constexpr bool DASH_POWER_LIGHT_SLEEP=true;
constexpr bool DASH_POWER_NIGHT_DEEP_SLEEP=true;
// Preserve native USB debugging while a host is attached; battery/charger sleeps.
constexpr bool DASH_POWER_USB_DEBUG=true;
constexpr unsigned DASH_POWER_MORNING_EARLY_SECONDS=300;
constexpr uint32_t DASH_POWER_SESSION_BUDGET_MS=45000;
