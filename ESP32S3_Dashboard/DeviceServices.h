#pragma once
#include <Arduino.h>
void deviceBegin();
void deviceService();
bool deviceHandleCommand(const String &line);
bool deviceMaintenanceRequested();
void deviceMarkHealthy();
[[noreturn]] void deviceFault(const char *stage,const char *reason,bool recoverable);
