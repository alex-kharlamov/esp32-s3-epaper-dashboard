#include "Dashboard.h"
// Deliberately fictional values: no service credentials or network calls.
static const float cpu[]={12,19,15,22,18,31,24,14,26,20,17,28,33,18,25,16,21,27,19,23,30,22,18,26,32,20,16,25,28,23};
static const float btc[]={72,74,73,78,76,80,79,82,85,83,88,90,86,89,87,92,94,93,96,94,95,98,96,99,102,100,98,103,101,104};
static const float eth[]={32,31,34,33,35,36,34,38,37,40,38,39,42,40,41,43,44,42,45,43,46,45,48,46,49,47,50,48,49,51};
static const float ping[]={23,28,25,30,26,24,29,33,27,25,30,26,23,29,31,28,25,27,24,29,32,28,25,30,26,24,29,31,26,28};
const DashboardData &sampleDashboard() {
  static const DashboardData data={
    23,7.6f,cpu,30,"96,420",btc,30,"3,240",eth,30,28,ping,30,
    18,64,1016,3,225,12.4f,24,"icon_partly-cloudy-day",
    {{"09:00",17,"icon_clouds",10},{"10:00",18,"icon_partly-cloudy-day",10},{"11:00",19,"icon_sun",5},{"12:00",20,"icon_sun",5},{"13:00",20,"icon_sun",5},{"14:00",19,"icon_partly-cloudy-day",15},{"15:00",18,"icon_clouds",25},{"16:00",17,"icon_rain",60}},
    "08:42","05 October 2026","MON",.3625f,.141f,.761f,7,true
  };return data;
}
