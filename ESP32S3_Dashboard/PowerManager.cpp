#include "PowerManager.h"
#include "PowerPolicy.h"
#include "Configuration.h"
#include "DEV_Config.h"
#include "RuntimeSettings.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <esp_sntp.h>
#include <driver/gpio.h>
#include <driver/usb_serial_jtag.h>
#include <freertos/semphr.h>
#include <atomic>
#include <sys/time.h>
namespace {
SemaphoreHandle_t workMutex=nullptr,sleepMutex=nullptr;
unsigned workCount=0;
std::atomic<bool> batteryTest{false},deepTest{false},usbHostSeen{false};
std::atomic<bool> displayActive{false};
std::atomic<uint32_t> batteryTestUntil{0};
bool timerWake=false;
std::atomic<bool> warmup{false};
uint32_t cooldown=0;
int64_t bootUs=0,networkStartUs=0;
uint64_t networkUs=0,lightUs=0,busySleepUs=0,lowCpuUs=0;
uint32_t sleepCount=0,sleepErrors=0;
int64_t lowStartUs=0;
constexpr uint32_t RTC_MAGIC=0x50575231;
struct SavedPower {uint32_t magic;int64_t panelEndMs;uint8_t purpose;};
RTC_DATA_ATTR SavedPower saved{};
struct TestResult {uint32_t magic=0x50575431,elapsedMs=0;uint64_t lightMs=0,busyMs=0;uint32_t sleeps=0,errors=0,cycles=0;int32_t worstOffsetMs=0;uint8_t complete=0;};
TestResult testResult{};
uint64_t testLightBase=0,testBusyBase=0;
uint32_t testSleepBase=0,testErrorBase=0,testStart=0;
void saveTest(bool complete){
  testResult.elapsedMs=millis()-testStart;testResult.lightMs=(lightUs-testLightBase)/1000;testResult.busyMs=(busySleepUs-testBusyBase)/1000;
  testResult.sleeps=sleepCount-testSleepBase;testResult.errors=sleepErrors-testErrorBase;testResult.complete=complete;
  Preferences p;if(p.begin("dashpower",false)){p.putBytes("lasttest",&testResult,sizeof(testResult));p.end();}
}
void reportSavedTest(){
  TestResult result{};Preferences p;if(!p.begin("dashpower",true))return;
  bool valid=p.getBytesLength("lasttest")==sizeof(result) && p.getBytes("lasttest",&result,sizeof(result))==sizeof(result) && result.magic==0x50575431;p.end();
  if(valid)Serial.printf("POWER_LAST_BATTERY_TEST: complete=%u elapsed_ms=%lu light_sleep_ms=%llu busy_sleep_ms=%llu sleeps=%lu errors=%lu cycles=%lu worst_clock_offset_ms=%ld\n",result.complete,(unsigned long)result.elapsedMs,(unsigned long long)result.lightMs,(unsigned long long)result.busyMs,(unsigned long)result.sleeps,(unsigned long)result.errors,(unsigned long)result.cycles,(long)result.worstOffsetMs);
}
const int outputs[]={EPD_PWR_PIN,EPD_RST_PIN,EPD_DC_PIN,EPD_SCK_PIN,EPD_MOSI_PIN,EPD_CS_M_PIN,EPD_CS_S_PIN};
int64_t wallMs(){timeval t{};gettimeofday(&t,nullptr);return int64_t(t.tv_sec)*1000+t.tv_usec/1000;}
bool usbDebug(){
  if(usb_serial_jtag_is_connected())usbHostSeen.store(true);
  return DASH_POWER_USB_DEBUG && usbHostSeen.load() && !batteryTest.load();
}
void holdPins(bool hold) {
  for(int pin:outputs)if(hold)gpio_hold_en(gpio_num_t(pin));else gpio_hold_dis(gpio_num_t(pin));
}
void nap(uint32_t ms,int busyPin) {
  uint32_t until=batteryTestUntil.load();
  // Finish only after a display phase, so its final offset is saved too.
  if(busyPin<0 && until && int32_t(until-millis())<=0){batteryTest.store(false);batteryTestUntil.store(0);saveTest(true);reportSavedTest();}
  if(ms<2){delay(ms);return;}
  if(!DASH_POWER_LIGHT_SLEEP || usbDebug() || !sleepMutex || xSemaphoreTake(sleepMutex,0)!=pdTRUE) {delay(busyPin>=0?10:(ms>200?200:ms));return;}
  if(WiFi.getMode()!=WIFI_OFF){xSemaphoreGive(sleepMutex);delay(ms>100?100:ms);return;}
  if(busyPin>=0 && digitalRead(busyPin)==HIGH){xSemaphoreGive(sleepMutex);return;}
  holdPins(true);
  esp_sleep_enable_timer_wakeup(uint64_t(ms)*1000);
  if(busyPin>=0){gpio_wakeup_enable(gpio_num_t(busyPin),GPIO_INTR_HIGH_LEVEL);esp_sleep_enable_gpio_wakeup();}
  int64_t start=esp_timer_get_time();
  esp_err_t error=esp_light_sleep_start();
  uint64_t elapsed=esp_timer_get_time()-start;
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  if(busyPin>=0){esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);gpio_wakeup_disable(gpio_num_t(busyPin));}
  holdPins(false);
  if(error==ESP_OK){++sleepCount;lightUs+=elapsed;if(busyPin>=0)busySleepUs+=elapsed;}
  else {++sleepErrors;delay(20);}
  xSemaphoreGive(sleepMutex);
}
}
void powerBegin() {
  timerWake=esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_TIMER;
  bool valid=saved.magic==RTC_MAGIC;
  warmup=timerWake && valid && saved.purpose==1;
  cooldown=timerWake && valid?millis()+powerCooldownRemaining(true,wallMs(),saved.panelEndMs,DASH_BOOT_COOLDOWN_MS):DASH_BOOT_COOLDOWN_MS;
  if(!valid || !timerWake)saved={RTC_MAGIC,0,0};
  // Configure LOW before releasing a retained pad to avoid a power-on glitch.
  for(int pin:outputs){pinMode(pin,OUTPUT);digitalWrite(pin,LOW);}
  gpio_deep_sleep_hold_dis();holdPins(false);
  workMutex=xSemaphoreCreateMutex();sleepMutex=xSemaphoreCreateMutex();
  if(!workMutex || !sleepMutex){Serial.println("POWER_FATAL: mutex allocation failed");delay(20);abort();}
  bootUs=lowStartUs=esp_timer_get_time();
  setCpuFrequencyMhz(DASH_POWER_IDLE_CPU_MHZ);
  Serial.printf("POWER_BOOT: timer_wake=%u warmup=%u cooldown_ms=%lu idle_cpu=%u USB_debug=%u\n",timerWake,warmup.load(),(unsigned long)cooldown,getCpuFrequencyMhz(),usbDebug());
  reportSavedTest();
}
void powerWorkBegin(){
  xSemaphoreTake(workMutex,portMAX_DELAY);
  if(!workCount++){lowCpuUs+=esp_timer_get_time()-lowStartUs;setCpuFrequencyMhz(240);}
  xSemaphoreGive(workMutex);
}
void powerWorkEnd(){
  xSemaphoreTake(workMutex,portMAX_DELAY);
  if(workCount && !--workCount){setCpuFrequencyMhz(DASH_POWER_IDLE_CPU_MHZ);lowStartUs=esp_timer_get_time();}
  xSemaphoreGive(workMutex);
}
void powerNetworkBegin(){xSemaphoreTake(sleepMutex,portMAX_DELAY);powerWorkBegin();networkStartUs=esp_timer_get_time();}
void powerNetworkEnd(){xSemaphoreTake(workMutex,portMAX_DELAY);networkUs+=esp_timer_get_time()-networkStartUs;xSemaphoreGive(workMutex);powerWorkEnd();xSemaphoreGive(sleepMutex);}
void powerIdleWait(uint32_t ms){nap(ms,-1);}
void powerBusyWait(int pin,uint32_t ms){nap(ms,pin);}
void powerPanelFinished(){saved.panelEndMs=wallMs();}
uint32_t powerBootCooldown(){return cooldown;}
bool powerMorningWarmup(){return warmup;}
bool powerNightImageHeld(){return timerWake && saved.magic==RTC_MAGIC && saved.purpose!=0;}
void powerSetBatteryTest(bool enabled){
  if(!enabled && batteryTestUntil.load())saveTest(false);
  if(enabled){testResult=TestResult{};testStart=millis();testLightBase=lightUs;testBusyBase=busySleepUs;testSleepBase=sleepCount;testErrorBase=sleepErrors;saveTest(false);}
  batteryTest.store(enabled);batteryTestUntil.store(enabled?millis()+120000:0);Serial.printf("POWER_BATTERY_TEST: %u; bounded to 120 seconds; USB may disappear during chip sleep.\n",enabled);}
void powerRequestDeepTest(){deepTest.store(true);}
bool powerDeepTestPending(){return deepTest.load();}
void powerNightSleep() {
  bool test=deepTest.load();
  if(!test && (!DASH_POWER_NIGHT_DEEP_SLEEP || usbDebug() || warmup))return;
  time_t now=time(nullptr),wake=test?now+90:powerMorningWake(now,DASH_POWER_MORNING_EARLY_SECONDS,deviceSettings().quietStart,deviceSettings().quietEnd);
  if(wake<=now || now<1700000000)return;
  // A network session already in progress must finish/abort before power-down.
  if(xSemaphoreTake(sleepMutex,pdMS_TO_TICKS(100))!=pdTRUE)return;
  esp_sntp_stop();WiFi.disconnect(true,false);WiFi.mode(WIFI_OFF);
  saved.purpose=test?2:1;
  for(int pin:outputs)digitalWrite(pin,LOW);
  holdPins(true);gpio_deep_sleep_hold_en();
  Serial.printf("POWER_DEEP_SLEEP: seconds=%lld purpose=%s panel_end_ms=%lld\n",(long long)(wake-now),test?"TEST":"NIGHT",(long long)saved.panelEndMs);
  if(usbDebug())delay(20);esp_sleep_enable_timer_wakeup(uint64_t(wake-now)*1000000);
  esp_deep_sleep_start();
}
void powerReport(){
  xSemaphoreTake(workMutex,portMAX_DELAY);
  uint64_t radio=networkUs,low=lowCpuUs+(!workCount?esp_timer_get_time()-lowStartUs:0);
  xSemaphoreGive(workMutex);
  Serial.printf("POWER_STATS: uptime_ms=%llu radio_session_ms=%llu low_cpu_ms=%llu light_sleep_ms=%llu busy_sleep_ms=%llu sleep_count=%lu sleep_errors=%lu cpu=%u USB_debug=%u\n",(unsigned long long)((esp_timer_get_time()-bootUs)/1000),(unsigned long long)(radio/1000),(unsigned long long)(low/1000),(unsigned long long)(lightUs/1000),(unsigned long long)(busySleepUs/1000),(unsigned long)sleepCount,(unsigned long)sleepErrors,getCpuFrequencyMhz(),usbDebug());
  reportSavedTest();
}
void powerEmergencyStop(){
  esp_sntp_stop();WiFi.disconnect(true,false);WiFi.mode(WIFI_OFF);
  for(int pin:outputs)digitalWrite(pin,LOW);
  setCpuFrequencyMhz(DASH_POWER_IDLE_CPU_MHZ);
}

void powerWakeComplete(){warmup=false;saved.purpose=0;}

void powerLightTest(){
  Serial.println("POWER_LIGHT_TEST: one-second bounded probe; USB may reconnect.");delay(20);
  uint32_t before=sleepCount,errors=sleepErrors;uint64_t start=lightUs;
  batteryTest.store(true);nap(1000,-1);batteryTest.store(false);
  Serial.printf("POWER_LIGHT_RESULT: slept_ms=%llu count=%lu errors=%lu\n",(unsigned long long)((lightUs-start)/1000),(unsigned long)(sleepCount-before),(unsigned long)(sleepErrors-errors));
  powerReport();
}

bool powerDisplayActive(){return displayActive.load();}
void powerDisplayBegin(){displayActive.store(true);powerWorkBegin();}
void powerDisplayEnd(){powerWorkEnd();displayActive.store(false);}

void powerClockFinished(int64_t offset){
  if(!batteryTestUntil.load())return;
  ++testResult.cycles;int64_t absolute=offset<0?-offset:offset;
  if(absolute>testResult.worstOffsetMs)testResult.worstOffsetMs=absolute>2147483647?2147483647:int32_t(absolute);
  saveTest(false);
}
