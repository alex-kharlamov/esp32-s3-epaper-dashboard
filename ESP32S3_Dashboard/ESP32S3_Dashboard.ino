#include "EPD_10in85g.h"
#include "Dashboard.h"
#include "LiveData.h"
#include "Configuration.h"
#include <string.h>
#include <esp_heap_caps.h>
#include <sys/time.h>
#include "ClockSchedule.h"

const char *demoStage="boot";


[[noreturn]] void demoAbort(const char *reason) {
  Serial.printf("ERROR stage=%s: %s\n",demoStage,reason);
  DEV_Module_Exit();
  Serial.println("Stopped: PWR LOW; no retry. Sleep unconfirmed on error.");
  Serial.flush();for(;;)delay(1000);
}
void setup() {
  pinMode(EPD_PWR_PIN,OUTPUT);digitalWrite(EPD_PWR_PIN,LOW);
  Serial.begin(115200);delay(2000);
  Serial.printf("\nESP32-S3 Dashboard clock + weather dashboard; built %s %s\n",__DATE__,__TIME__);
  Serial.println("UI source: czuryk/Waveshare-ePaper-10.85-dashboard @ ceced5eb");
  Serial.println("Live weather and time; no fictional fallback will be displayed.");
  Serial.printf("Chip=%s rev=%d flash=%lu PSRAM=%lu heap=%lu\n",ESP.getChipModel(),ESP.getChipRevision(),
    (unsigned long)ESP.getFlashChipSize(),(unsigned long)ESP.getPsramSize(),(unsigned long)ESP.getFreeHeap());
  Serial.println("VCC=3V3 GND=GND DIN=G11 CLK=G12 CS_M=G10 CS_S=G9 DC=G13 RST=G14 BUSY=G4 PWR=G5; native USB CDC enabled");
  if(String(ESP.getChipModel())!="ESP32-S3" || ESP.getFlashChipSize()!=16777216 || ESP.getPsramSize()!=8388608)
    demoAbort("Unexpected chip or memory configuration");
  beginLiveData();
}
constexpr uint32_t DISPLAY_INTERVAL_MS=DASH_CLOCK_INTERVAL_MS;
constexpr uint32_t WEATHER_INTERVAL_MS=DASH_FULL_INTERVAL_MS;
constexpr uint32_t BOOT_COOLDOWN_MS=DASH_BOOT_COOLDOWN_MS;
uint32_t lastDisplayStarted=0,lastWeatherAttempt=0,lastFullStarted=0;
char displayedClock[6]={};
int64_t displayedMinute=-1,lastFullMinute=-1;
uint32_t fullCycleEstimate=21000,clockOverheadEstimate=3500,clockWaveEstimate=(DASH_CLOCK_WINDOW_ENABLED && DASH_CLOCK_PLL==0x07)?5270:12090;
int64_t wallMillis(){timeval tv{};gettimeofday(&tv,nullptr);return int64_t(tv.tv_sec)*1000+tv.tv_usec/1000;}

bool hasDrawn=false,weatherAttempted=false,weatherAvailable=false,bootReady=false;
void loop() {
  serviceLiveSetup();
  uint32_t now=millis();int64_t wallNow=wallMillis();
  // Keep network work away from the end-of-minute rendering deadline.
  bool weatherDue=!weatherAttempted || now-lastWeatherAttempt>=(weatherAvailable?WEATHER_INTERVAL_MS:30000);
  if(weatherDue && (!hasDrawn || wallNow%60000<15000)) {
    weatherAttempted=true;lastWeatherAttempt=now;
    weatherAvailable=fetchLiveData() || weatherAvailable;
    wallNow=wallMillis();
  }
  if(!bootReady && millis()>=BOOT_COOLDOWN_MS)bootReady=true;
  if(!bootReady || wallNow<1700000000000LL){delay(20);return;}
  const int64_t upcoming=(wallNow/DISPLAY_INTERVAL_MS+1)*DISPLAY_INTERVAL_MS;
  bool fullRefresh=!hasDrawn || upcoming-lastFullMinute>=WEATHER_INTERVAL_MS || millis()-lastFullStarted>=WEATHER_INTERVAL_MS+DISPLAY_INTERVAL_MS;
  ClockWindow windows[4];int windowCount=1;
  DashboardData forecast{};
  if(!getLiveDashboard(forecast,time_t(upcoming/1000))){delay(20);return;}
  if(hasDrawn && DASH_CLOCK_WINDOW_ENABLED)windowCount=changedClockWindows(displayedClock,forecast.clock,windows);
  uint32_t lead=fullRefresh?fullCycleEstimate:(clockOverheadEstimate+clockWaveEstimate*(windowCount?windowCount:1));
  if(lead>=DISPLAY_INTERVAL_MS)lead=DISPLAY_INTERVAL_MS-1000;
  int64_t target=clockDisplayTarget(wallNow,displayedMinute,lead,DISPLAY_INTERVAL_MS);
  if(target<0){delay(20);return;}
  // A late wake or NTP correction may target the current, not upcoming, minute.
  fullRefresh=!hasDrawn || target-lastFullMinute>=WEATHER_INTERVAL_MS || millis()-lastFullStarted>=WEATHER_INTERVAL_MS+DISPLAY_INTERVAL_MS;
  DashboardData data{};if(!getLiveDashboard(data,time_t(target/1000))){delay(20);return;}
  windowCount=0;
  if(!fullRefresh && DASH_CLOCK_WINDOW_ENABLED) {
    windowCount=changedClockWindows(displayedClock,data.clock,windows);
    if(!windowCount) {displayedMinute=target;delay(20);return;}
    Serial.printf("CLOCK_DIRTY: %s -> %s, %d window(s)\n",displayedClock,data.clock,windowCount);
  }
  uint32_t cycleStarted=millis();
  Serial.printf("CLOCK_SCHEDULE: target_epoch_ms=%lld start_epoch_ms=%lld lead_ms=%lu\n",(long long)target,(long long)wallMillis(),(unsigned long)lead);
  uint8_t *frame=(uint8_t*)heap_caps_malloc(DASH_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!frame)demoAbort("Cannot allocate framebuffer in PSRAM");
  demoStage="render dashboard";uint32_t started=millis();
  renderDashboard(frame,data);
  Serial.printf("Native UI rendered: 1360x480, %d bytes, %lu ms\n",DASH_BYTES,(unsigned long)(millis()-started));
  delay(1);
  lastDisplayStarted=millis();
  Serial.printf("WINDOW_CYCLE_START: uptime=%lu ms clock=%s mode=%s\n",(unsigned long)lastDisplayStarted,data.clock,fullRefresh?"FULL":(DASH_CLOCK_WINDOW_ENABLED?"CLOCK_WINDOW":"FAST_FULL"));
  DEV_Module_Init();
  if(fullRefresh){lastFullStarted=lastDisplayStarted;EPD_10in85g_Init();}
  else {
    EPD_10in85g_Init_Fast();
    if(DASH_CLOCK_WINDOW_ENABLED)EPD_10in85g_SetClockFrameRate(DASH_CLOCK_PLL);
  }
  demoStage="dashboard transfer";
  Serial.println("Transfer UI to both controllers (81,600 bytes each)");
  if(fullRefresh)EPD_10in85g_Display(frame);
  else if(DASH_CLOCK_WINDOW_ENABLED)EPD_10in85g_DisplayClockWindows(frame,windows,windowCount);
  else EPD_10in85g_Display(frame);
  Serial.println("Entering display sleep");EPD_10in85g_Sleep();DEV_Module_Exit();
  heap_caps_free(frame);
  uint32_t elapsed=millis()-cycleStarted,wave=EPD_10in85g_LastWaveformMs();
  unsigned passes=fullRefresh?1:(windowCount?windowCount:1);
  if(fullRefresh){fullCycleEstimate=elapsed;lastFullMinute=target;}
  else clockWaveEstimate=wave;
  if(elapsed>=wave*passes)clockOverheadEstimate=elapsed-wave*passes;
  displayedMinute=target;
  Serial.printf("CLOCK_COMPLETE: target_epoch_ms=%lld finish_epoch_ms=%lld offset_ms=%lld cycle_ms=%lu\n",(long long)target,(long long)wallMillis(),(long long)(wallMillis()-target),(unsigned long)elapsed);
  snprintf(displayedClock,sizeof(displayedClock),"%s",data.clock);
  hasDrawn=true;
  Serial.println("LIVE_REFRESH_DONE: refresh completed; panel asleep, PWR LOW.");
  Serial.flush();
}
