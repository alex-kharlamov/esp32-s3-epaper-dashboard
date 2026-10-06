#include "EPD_10in85g.h"
#include "Dashboard.h"
#include "LiveData.h"
#include "Configuration.h"
#include <string.h>
#include <esp_heap_caps.h>
#include <sys/time.h>
#include "ClockSchedule.h"
#include "QuietHours.h"

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
  Serial.printf("\nESP32-S3 Dashboard LIVE v17 NIGHT QUIET MODE; built %s %s\n",__DATE__,__TIME__);
  Serial.println("UI: compact two-journey colour dashboard / changed-digit clock");
  Serial.println("Live weather and time; no fictional fallback will be displayed.");
  Serial.printf("Chip=%s rev=%d flash=%lu PSRAM=%lu heap=%lu\n",ESP.getChipModel(),ESP.getChipRevision(),
    (unsigned long)ESP.getFlashChipSize(),(unsigned long)ESP.getPsramSize(),(unsigned long)ESP.getFreeHeap());
  Serial.println("VCC=3V3 GND=GND DIN=G11 CLK=G12 CS_M=G10 CS_S=G9 DC=G13 RST=G14 BUSY=G4 PWR=G5; native USB CDC enabled");
  if(String(ESP.getChipModel())!="ESP32-S3" || ESP.getFlashChipSize()!=16777216 || ESP.getPsramSize()!=8388608)
    demoAbort("Unexpected chip or memory configuration");
  if(!beginLiveData())demoAbort("Cannot start background data service");
}
constexpr uint32_t DISPLAY_INTERVAL_MS=DASH_CLOCK_INTERVAL_MS;
constexpr uint32_t WEATHER_INTERVAL_MS=DASH_FULL_INTERVAL_MS;
constexpr uint32_t BOOT_COOLDOWN_MS=DASH_BOOT_COOLDOWN_MS;
uint32_t lastDisplayStarted=0,lastFullStarted=0;
DashboardData displayedData{};
char displayedClock[6]={};
int64_t displayedMinute=-1,lastFullMinute=-1;
uint32_t colourWaveEstimate=17000,colourOverheadEstimate=3500;
uint32_t fullCycleEstimate=21000,clockOverheadEstimate=3500,clockWaveEstimate=(DASH_CLOCK_WINDOW_ENABLED && DASH_CLOCK_PLL==0x07)?5270:12090;
int64_t wallMillis(){timeval tv{};gettimeofday(&tv,nullptr);return int64_t(tv.tv_sec)*1000+tv.tv_usec/1000;}

uint32_t runDisplayPhase(const uint8_t *frame,const ClockWindow *windows,unsigned count,bool colour,bool whole) {
  DEV_Module_Init();
  if(colour)EPD_10in85g_Init();
  else {EPD_10in85g_Init_Fast();if(DASH_CLOCK_WINDOW_ENABLED)EPD_10in85g_SetClockFrameRate(DASH_CLOCK_PLL);}
  demoStage="dashboard transfer";
  Serial.printf("DISPLAY_PHASE: %s, %u window(s)\n",colour?"NORMAL_COLOUR":"FAST_CLOCK",count);
  if(whole)EPD_10in85g_Display(frame);
  else EPD_10in85g_DisplayClockWindows(frame,windows,count);
  uint32_t wave=EPD_10in85g_LastWaveformMs();
  Serial.println("Entering display sleep");EPD_10in85g_Sleep();DEV_Module_Exit();
  return wave;
}

void holdTransportUntilFullRefresh(DashboardData &next,const DashboardData &shown) {
  next.dlr=shown.dlr;next.jubilee=shown.jubilee;
  next.canningTown=shown.canningTown;next.eastIndia=shown.eastIndia;
  next.transportCheckedAt=shown.transportCheckedAt;
}

bool hasDrawn=false,bootReady=false;
QuietDisplayState quietDisplay;
void loop() {
  serviceLiveSetup();
  int64_t wallNow=wallMillis();
  if(!bootReady && millis()>=BOOT_COOLDOWN_MS)bootReady=true;
  if(!bootReady){delay(20);return;}
  const bool quiet=liveDataQuiet();
  QuietAction quietAction=quietDisplay.update(quiet);
  if(quietAction==QuietAction::Sleep) {
    uint8_t *frame=(uint8_t*)heap_caps_malloc(DASH_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!frame)demoAbort("Cannot allocate sleep framebuffer");
    renderSleepScreen(frame);
    Serial.println("NIGHT_SLEEP: one full sleep image; no further screen or data updates while quiet mode is active.");
    runDisplayPhase(frame,nullptr,0,true,true);heap_caps_free(frame);
    clearTransportRepaint();
    Serial.println("NIGHT_SCREEN_DONE: panel asleep, PWR LOW.");
  }
  if(quiet){delay(250);return;}
  if(quietAction==QuietAction::Wake) {
    hasDrawn=false;displayedMinute=-1;lastFullMinute=-1;
    Serial.println("NIGHT_WAKE: restore dashboard with a full redraw; fresh data batch resumes.");
  }
  bool timeReady=wallNow>=1700000000000LL;
  // A cold offline boot gets one honest waiting screen; no invented time.
  if(!timeReady && hasDrawn){delay(20);return;}
  const int64_t upcoming=timeReady?(wallNow/DISPLAY_INTERVAL_MS+1)*DISPLAY_INTERVAL_MS:0;
  bool fullRefresh=!hasDrawn || upcoming-lastFullMinute>=WEATHER_INTERVAL_MS || millis()-lastFullStarted>=WEATHER_INTERVAL_MS+DISPLAY_INTERVAL_MS;
  ClockWindow windows[4];int windowCount=1;
  DashboardData forecast{};
  if(!getLiveDashboard(forecast,time_t(upcoming/1000))){delay(20);return;}
  if(hasDrawn && !fullRefresh && !transportRepaintPending())holdTransportUntilFullRefresh(forecast,displayedData);
  if(hasDrawn && DASH_CLOCK_WINDOW_ENABLED)windowCount=changedDashboardWindows(displayedClock,displayedData,forecast,windows,transportRepaintPending());
  bool colourPartial=hasDrawn && !fullRefresh && (transportStatusChanged(displayedData,forecast) || transportRepaintPending());
  unsigned predictedClockCount=windowCount-(colourPartial?1:0);
  uint32_t lead=fullRefresh?fullCycleEstimate:(predictedClockCount?clockOverheadEstimate+clockWaveEstimate*predictedClockCount:0)+(colourPartial?colourOverheadEstimate+colourWaveEstimate:0);
  if(lead>=DISPLAY_INTERVAL_MS)lead=DISPLAY_INTERVAL_MS-1000;
  int64_t target=timeReady?clockDisplayTarget(wallNow,displayedMinute,lead,DISPLAY_INTERVAL_MS):0;
  if(target<0){delay(20);return;}
  // A late wake or NTP correction may target the current, not upcoming, minute.
  fullRefresh=!hasDrawn || target-lastFullMinute>=WEATHER_INTERVAL_MS || millis()-lastFullStarted>=WEATHER_INTERVAL_MS+DISPLAY_INTERVAL_MS;
  DashboardData data{};if(!getLiveDashboard(data,time_t(target/1000))){delay(20);return;}
  if(hasDrawn && !fullRefresh && !transportRepaintPending())holdTransportUntilFullRefresh(data,displayedData);
  windowCount=0;
  if(!fullRefresh && DASH_CLOCK_WINDOW_ENABLED) {
    windowCount=changedDashboardWindows(displayedClock,displayedData,data,windows,transportRepaintPending());
    if(!windowCount) {displayedMinute=target;delay(20);return;}
    Serial.printf("DISPLAY_DIRTY: %s -> %s, %d window(s)\n",displayedClock,data.clock,windowCount);
  }
  colourPartial=!fullRefresh && (transportStatusChanged(displayedData,data) || transportRepaintPending());
  uint32_t cycleStarted=millis();
  Serial.printf("CLOCK_SCHEDULE: target_epoch_ms=%lld start_epoch_ms=%lld lead_ms=%lu\n",(long long)target,(long long)wallMillis(),(unsigned long)lead);
  uint8_t *frame=(uint8_t*)heap_caps_malloc(DASH_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!frame)demoAbort("Cannot allocate framebuffer in PSRAM");
  demoStage="render dashboard";uint32_t started=millis();
  renderDashboard(frame,data);
  Serial.printf("Native UI rendered: 1360x480, %d bytes, %lu ms\n",DASH_BYTES,(unsigned long)(millis()-started));
  delay(1);
  lastDisplayStarted=millis();
  Serial.printf("WINDOW_CYCLE_START: uptime=%lu ms clock=%s mode=%s\n",(unsigned long)lastDisplayStarted,data.clock,fullRefresh?"FULL":colourPartial?"COLOUR_WINDOW":(DASH_CLOCK_WINDOW_ENABLED?"CLOCK_WINDOW":"FAST_FULL"));
  uint32_t renderElapsed=millis()-cycleStarted;
  ClockWindow clockWindows[4],colourWindows[4];unsigned clockCount=0,colourCount=0;
  for(int i=0;i<windowCount;i++) {
    if(windows[i].y==TRANSPORT_WINDOW.y)colourWindows[colourCount++]=windows[i];
    else clockWindows[clockCount++]=windows[i];
  }
  if(fullRefresh) {
    lastFullStarted=lastDisplayStarted;
    runDisplayPhase(frame,nullptr,0,true,true);
    fullCycleEstimate=millis()-cycleStarted;lastFullMinute=target;
  }else {
    // Colour first, clock last: the minute digits settle near the NTP target,
    // rather than appearing early while the slower colour waveform runs.
    if(colourCount) {
      uint32_t phaseStarted=millis();
      uint32_t wave=runDisplayPhase(frame,colourWindows,colourCount,true,false);
      uint32_t elapsed=millis()-phaseStarted;
      colourWaveEstimate=wave;
      if(elapsed>=wave*colourCount)colourOverheadEstimate=elapsed-wave*colourCount;
      Serial.printf("COLOUR_PHASE_COMPLETE: cycle_ms=%lu wave_ms=%lu\n",(unsigned long)elapsed,(unsigned long)wave);
    }
    if(clockCount || !DASH_CLOCK_WINDOW_ENABLED) {
      uint32_t phaseStarted=millis();
      uint32_t wave=runDisplayPhase(frame,clockWindows,clockCount,false,!DASH_CLOCK_WINDOW_ENABLED);
      uint32_t elapsed=millis()-phaseStarted;
      clockWaveEstimate=wave;
      unsigned passes=clockCount?clockCount:1;
      if(elapsed>=wave*passes)clockOverheadEstimate=renderElapsed+elapsed-wave*passes;
    }
  }
  heap_caps_free(frame);
  uint32_t elapsed=millis()-cycleStarted;
  displayedMinute=timeReady?target:-1;
  displayedData=data;
  clearTransportRepaint();
  Serial.printf("CLOCK_COMPLETE: target_epoch_ms=%lld finish_epoch_ms=%lld offset_ms=%lld cycle_ms=%lu\n",(long long)target,(long long)wallMillis(),(long long)(wallMillis()-target),(unsigned long)elapsed);
  snprintf(displayedClock,sizeof(displayedClock),"%s",data.clock);
  hasDrawn=true;
  Serial.println("LIVE_REFRESH_DONE: refresh completed; panel asleep, PWR LOW.");
  Serial.flush();
}
