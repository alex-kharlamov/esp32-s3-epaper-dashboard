#pragma once
#include <stdint.h>
constexpr int DASH_WIDTH=1360, DASH_HEIGHT=480, DASH_BYTES=DASH_WIDTH*DASH_HEIGHT/4;
enum class ServiceHealth : uint8_t { Unknown, Good, Notice, Issue, Stale };
inline ServiceHealth journeyHealth(ServiceHealth line,ServiceHealth station) {
  const ServiceHealth states[]={ServiceHealth::Issue,ServiceHealth::Notice,ServiceHealth::Stale,ServiceHealth::Unknown};
  for(auto state:states)
    if(line==state || station==state)return state;
  return ServiceHealth::Good;
}
struct Forecast { const char *time; int temperature; const char *icon; int rainProbability; bool available=true; };
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
  const char *location = "London";
  bool weatherAvailable = true;
  ServiceHealth dlr=ServiceHealth::Unknown, jubilee=ServiceHealth::Unknown;
  ServiceHealth canningTown=ServiceHealth::Unknown, eastIndia=ServiceHealth::Unknown;
  int64_t transportCheckedAt=0;
};
void renderDashboard(uint8_t *buffer,const DashboardData &data);
void renderSleepScreen(uint8_t *buffer);
const DashboardData &sampleDashboard();
