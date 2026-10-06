#include "Dashboard.h"
// Illustrative fixture for the selected design; never used by live firmware.
const DashboardData &sampleDashboard() {
  static const DashboardData data=[] {
    DashboardData d{};
    d.temperature=18;d.windSpeed=12;d.windDirection=315;
    d.weatherIcon="icon_partly-cloudy-day";
    d.forecast[0]={"13:00",18,"icon_partly-cloudy-day",10};
    d.forecast[1]={"14:00",19,"icon_clouds",15};
    d.forecast[2]={"15:00",19,"icon_clouds",25};
    d.forecast[3]={"16:00",18,"icon_rain",55};
    d.forecast[4]={"17:00",17,"icon_heavy_rain",70};
    d.forecast[5]={"18:00",16,"icon_rain",60};
    d.forecast[6]={"19:00",15,"icon_night",35};
    d.forecast[7]={"20:00",14,"icon_night",20};
    d.clock="12:34";d.date="6 OCT 2026";d.weekday="TUE";
    d.dlr=ServiceHealth::Good;d.jubilee=ServiceHealth::Good;
    d.canningTown=ServiceHealth::Notice;d.eastIndia=ServiceHealth::Good;
    d.sampleData=true;return d;
  }();return data;
}
