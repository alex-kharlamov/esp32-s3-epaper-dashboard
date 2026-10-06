#pragma once
#include "Dashboard.h"
#include <time.h>
bool beginLiveData();
void serviceLiveSetup();
bool fetchLiveData();
bool getLiveDashboard(DashboardData &data,time_t displayAt=0);

// Explicit USB diagnostic: repaint live transport values with the colour waveform.
bool transportRepaintPending();
void clearTransportRepaint();

// Scheduled quiet period, or a short USB diagnostic preview.
bool liveDataQuiet();
