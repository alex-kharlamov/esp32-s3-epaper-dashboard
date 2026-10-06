#include "EPD_10in85g.h"
#include "Dashboard.h"
#include "LiveData.h"
#include "Configuration.h"
#include <string.h>
#include <esp_heap_caps.h>

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
bool hasDrawn=false,weatherAttempted=false,weatherAvailable=false,bootReady=false;
void loop() {
  serviceLiveSetup();
  // Fetch off-screen into cached data; the minute-by-minute renderer reads it.
  uint32_t now=millis();
  if(!weatherAttempted || now-lastWeatherAttempt>=(weatherAvailable?WEATHER_INTERVAL_MS:30000)) {
    weatherAttempted=true;lastWeatherAttempt=now;
    weatherAvailable=fetchLiveData() || weatherAvailable;
  }
  if(!bootReady && millis()>=BOOT_COOLDOWN_MS)bootReady=true;
  if(!bootReady || (hasDrawn && millis()-lastDisplayStarted<DISPLAY_INTERVAL_MS)){delay(20);return;}
  const bool fullRefresh=!hasDrawn || millis()-lastFullStarted>=WEATHER_INTERVAL_MS;
  if(fullRefresh && weatherAvailable) {
    lastWeatherAttempt=millis();fetchLiveData(); // Fresh weather accompanies each full refresh.
  }
  DashboardData data{};if(!getLiveDashboard(data)){delay(20);return;}
  ClockWindow windows[4];int windowCount=0;
  if(!fullRefresh && DASH_CLOCK_WINDOW_ENABLED) {
    windowCount=changedClockWindows(displayedClock,data.clock,windows);
    if(!windowCount) {lastDisplayStarted=millis();delay(20);return;}
    Serial.printf("CLOCK_DIRTY: %s -> %s, %d window(s)\n",displayedClock,data.clock,windowCount);
  }
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
  snprintf(displayedClock,sizeof(displayedClock),"%s",data.clock);
  hasDrawn=true;
  Serial.println("LIVE_REFRESH_DONE: refresh completed; panel asleep, PWR LOW.");
  Serial.flush();
}
