#pragma once
#include <stdint.h>
void powerBegin();
void powerWorkBegin();
void powerWorkEnd();
void powerNetworkBegin();
void powerNetworkEnd();
void powerIdleWait(uint32_t milliseconds);
void powerBusyWait(int pin,uint32_t milliseconds);
void powerPanelFinished();
void powerReport();
bool powerNightImageHeld();
bool powerMorningWarmup();
uint32_t powerBootCooldown();
void powerNightSleep();
void powerRequestDeepTest();
bool powerDeepTestPending();
void powerSetBatteryTest(bool enabled);
void powerEmergencyStop();

void powerWakeComplete();

void powerLightTest();

bool powerDisplayActive();
void powerDisplayBegin();
void powerDisplayEnd();

void powerClockFinished(int64_t offset);
