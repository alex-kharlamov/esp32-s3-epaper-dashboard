#pragma once
#include <stdint.h>
constexpr int DASH_WIDTH=1360, DASH_HEIGHT=480, DASH_BYTES=DASH_WIDTH*DASH_HEIGHT/4;
struct Forecast { const char *time; int temperature; const char *icon; int rainProbability; };
struct DashboardData {
  int cpuPercent; float ramFreeMb; const float *cpuHistory; int cpuCount;
  const char *btcPrice; const float *btcHistory; int btcCount;
  const char *ethPrice; const float *ethHistory; int ethCount;
  int pingMs; const float *pingHistory; int pingCount;
  int temperature, humidity, pressure, uv, windDirection; float windSpeed; int aqi;
  const char *weatherIcon; Forecast forecast[8];
  const char *clock, *date, *weekday;
  float dayProgress, monthProgress, yearProgress; int unread;
  bool sampleData;
  const char *statusText = nullptr;
};
void renderDashboard(uint8_t *buffer,const DashboardData &data);
const DashboardData &sampleDashboard();
