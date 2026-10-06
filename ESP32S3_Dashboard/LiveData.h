#pragma once
#include "Dashboard.h"
#include <time.h>
void beginLiveData();
void serviceLiveSetup();
bool fetchLiveData();
bool getLiveDashboard(DashboardData &data,time_t displayAt=0);
