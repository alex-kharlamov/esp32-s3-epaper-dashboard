#include "DashboardState.h"
#include "WeatherParsing.h"
#include "ClockUpdate.h"
#include "ProviderPolicy.h"
#include "SettingsModel.h"
#include "HttpBodyReader.h"
#include "TransportDetails.h"
#include "PowerPolicy.h"
#include "HardwareModel.h"
#include "TransportCache.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
using H=ServiceHealth;
JsonDocument json(const char *value) {JsonDocument d;assert(!deserializeJson(d,value));return d;}
JsonDocument file(const char *path) {FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);char *s=(char*)malloc(n+1);assert(s);assert(fread(s,1,n,f)==size_t(n));s[n]=0;fclose(f);auto d=json(s);free(s);return d;}
int main(int argc,char **argv) {
  assert(argc==4);
  setenv("TZ","GMT0BST,M3.5.0/1,M10.5.0",1);tzset();
  auto response=file(argv[1]);int64_t now=response["current"]["time"].as<int64_t>()+60;
  WeatherSnapshot w{};assert(parseWeatherResponse(response.as<JsonVariantConst>(),now,51.5074,-0.1278,w));
  assert(w.hourCount==48 && w.fetchedAt==now);
  auto record=makeWeatherRecord(w);WeatherSnapshot restored{};
  assert(loadWeatherRecord(&record,sizeof(record),51.5074,-0.1278,restored));
  assert(restored.fetchedAt==w.fetchedAt && restored.hourly[47].epoch==w.hourly[47].epoch);
  assert(!loadWeatherRecord(&record,sizeof(record)-1,51.5074,-0.1278,restored));
  assert(!loadWeatherRecord(&record,sizeof(record),51.508881,0.008272,restored));
  record.version=99;assert(!loadWeatherRecord(&record,sizeof(record),51.5074,-0.1278,restored));
  record=makeWeatherRecord(w);record.weather.hourly[3].temperature++;
  assert(!loadWeatherRecord(&record,sizeof(record),51.5074,-0.1278,restored));
  record=makeWeatherRecord(w);record.weather.hourCount=255;record.checksum=weatherChecksum(record.weather);
  assert(!loadWeatherRecord(&record,sizeof(record),51.5074,-0.1278,restored));
  auto malformed=file(argv[1]);malformed["hourly"]["precipitation_probability"][0]=nullptr;
  assert(!parseWeatherResponse(malformed.as<JsonVariantConst>(),now,51.5074,-0.1278,restored));
  malformed=file(argv[1]);malformed["current"]["wind_speed_10m"]=1e30;
  assert(!parseWeatherResponse(malformed.as<JsonVariantConst>(),now,51.5074,-0.1278,restored));
  assert(validWeatherCode(56) && validWeatherCode(66));
  puts("Weather: real response, version/checksum/length/location validation, missing values and invalid ranges passed.");
  LegacyWeatherRecord legacy{};legacy.magic=0x57584348;legacy.version=1;legacy.size=sizeof(legacy.weather);
  auto &v=legacy.weather;v.fetchedAt=w.fetchedAt;v.observedAt=w.observedAt;v.latitude=w.latitude;v.longitude=w.longitude;v.windSpeed=w.windSpeed;v.temperature=w.temperature;v.windDirection=w.windDirection;v.code=w.code;v.isDay=w.isDay;v.hourCount=w.hourCount;
  for(int i=0;i<w.hourCount;i++){auto &h=w.hourly[i];v.hourly[i]={h.epoch,h.temperature,h.code,h.rainProbability,h.isDay};}
  legacy.checksum=2166136261u;for(size_t i=0;i<sizeof(v);i++)legacy.checksum=(legacy.checksum^((uint8_t*)&v)[i])*16777619u;
  assert(loadWeatherRecord(&legacy,sizeof(legacy),w.latitude,w.longitude,restored));assert(!restored.extrasAvailable&&restored.fetchedAt==w.fetchedAt);legacy.checksum++;assert(!loadWeatherRecord(&legacy,sizeof(legacy),w.latitude,w.longitude,restored));
  auto enriched=file(argv[1]);enriched["current"]["apparent_temperature"]=11.5;enriched["current"]["wind_gusts_10m"]=42.0;for(int i=0;i<48;i++)enriched["hourly"]["precipitation"][i]=.8;
  assert(parseWeatherResponse(enriched.as<JsonVariantConst>(),now,w.latitude,w.longitude,restored));assert(restored.extrasAvailable&&restored.gustSpeed==42&&restored.hourly[0].precipitationAvailable);
  ProviderState weatherHealth,tflHealth;weatherHealth.finish(false,now,100);tflHealth.finish(true,now,100);assert(!weatherHealth.latestOk&&tflHealth.latestOk);assert(weatherHealth.due(30100)&&!weatherHealth.due(30099));for(int i=0;i<20;i++)weatherHealth.finish(false,now,100);assert(weatherHealth.retryMs==600000);weatherHealth.finish(true,now,100);assert(weatherHealth.failures==0&&weatherHealth.due(600100));assert(sourceExpired(now-7201,now,7200));
  BatchSchedule batches;assert(batches.due(0));batches.begin(1000);assert(!batches.due(600999)&&batches.due(601000));
  // A provider finishing later, or a failed-provider retry, cannot split/move the normal batch deadline.
  tflHealth.finish(true,now,4000);assert(!tflHealth.due(601000)&&batches.due(601000));weatherHealth.finish(false,now,30000);assert(weatherHealth.due(60000)&&batches.startedMs==1000);
  batches.begin(UINT32_MAX-100);assert(!batches.due(599898)&&batches.due(599899));
  DeviceSettings cfg{};cfg.latitude=51.5;cfg.longitude=0;strcpy(cfg.location,"London");strcpy(cfg.timezone,"GMT0BST,M3.5.0/1,M10.5.0");strcpy(cfg.dlrStation,"940GZZDLEIN");strcpy(cfg.jubileeStation,"940GZZLUCGT");assert(validSettings(cfg));assert(!posixTimezone("Europe/London")&&!posixTimezone("GMT99")&&posixTimezone("UTC0")&&posixTimezone("EST5EDT,M3.2.0,M11.1.0"));cfg.latitude=91;assert(!validSettings(cfg));cfg.latitude=51.5;strcpy(cfg.haUrl,"http://example.com");assert(!validSettings(cfg));strcpy(cfg.haUrl,"https://example.com");strcpy(cfg.haTemperature,"sensor.room_temperature");assert(validSettings(cfg));strcpy(cfg.haTemperature,"../../secrets");assert(!validSettings(cfg));assert(quietAtHour(23,22,6)&&quietAtHour(5,22,6)&&!quietAtHour(12,22,6)&&!quietAtHour(0,0,0));
  char body[32];size_t used=0;auto bodyTest=[&](const char *input,int64_t length,bool chunked){size_t pos=0;return readHttpBody([&](){return input[pos]?int((unsigned char)input[pos++]):-1;},length,chunked,body,sizeof(body),used);};
  assert(bodyTest("3\r\nabc\r\n2;extension=value\r\nde\r\n0\r\n\r\n",-1,true)&&used==5&&!strcmp(body,"abcde"));assert(!bodyTest("0\r\nx\n",-1,true));assert(!bodyTest("0\r\nHeader: x\n\r\n",-1,true));assert(bodyTest("0\r\nHeader: x\r\n\r\n",-1,true));assert(!bodyTest("40\r\n",-1,true));assert(!bodyTest("3\r\nab",-1,true));assert(!bodyTest("zz\r\n",-1,true));assert(!bodyTest("ab",3,false));assert(bodyTest("abc",3,false));assert(bodyTest("abc",-1,false));assert(!readHttpBody([](){return -2;},-1,false,body,sizeof(body),used));
  puts("v21: legacy cache migration, weather extras, independent provider retries, settings validation and bounded/chunked HTTP parsing passed.");
  uint8_t rtc[]={0x56,0x34,0x12,0x03,0x07,0x10,0x26};assert(decodeRtcEpoch(rtc,0)==transportEpoch("2026-10-07T12:34:56Z"));assert(decodeRtcEpoch(rtc,0x80)==-1);rtc[0]=0x6a;assert(decodeRtcEpoch(rtc,0)==-1);rtc[0]=0x56;rtc[2]=0x72;assert(decodeRtcEpoch(rtc,0)==transportEpoch("2026-10-07T12:34:56Z"));
  uint8_t version[]={0,0x12},battery[]={0xb9,0,50,128};float volts,percent;assert(decodeBattery(version,battery,volts,percent)&&fabs(volts-3.7)<.001&&percent==50.5);version[0]=255;assert(!decodeBattery(version,battery,volts,percent));
  puts("Hardware adapters: DS3231 UTC/12-hour/oscillator-stop checks and MAX17048 voltage/SOC decoding passed; physical devices not present.");



  auto lines=file(argv[2]),stations=file(argv[3]);
  assert(parseLineStatus(lines.as<JsonVariantConst>(),"dlr",now)==H::Good);
  assert(parseLineStatus(lines.as<JsonVariantConst>(),"jubilee",now)==H::Good);
  assert(parseLineStatus(lines.as<JsonVariantConst>(),"missing",now)==H::Unknown);
  // Captured fixture includes Canning Town's current escalator fault.
  assert(parseStationStatus(stations.as<JsonVariantConst>(),"940GZZLUCGT",now)==H::Notice);
  assert(parseStationStatus(stations.as<JsonVariantConst>(),"940GZZDLCGT",now)==H::Good);
  assert(parseStationStatus(stations.as<JsonVariantConst>(),"940GZZDLEIN",now)==H::Good);
  auto mixed=json(R"([{"id":"dlr","lineStatuses":[{"statusSeverity":10},{"statusSeverity":6}]}])");
  assert(parseLineStatus(mixed.as<JsonVariantConst>(),"dlr",now)==H::Issue);
  mixed=json(R"([{"id":"dlr","lineStatuses":[{"statusSeverity":10},{"statusSeverity":6,"validityPeriods":[{"fromDate":"2099-01-01T00:00:00Z","toDate":"2099-01-02T00:00:00Z"}]}]}])");
  assert(parseLineStatus(mixed.as<JsonVariantConst>(),"dlr",now)==H::Good);
  mixed=json(R"([{"id":"dlr","lineStatuses":[{"statusSeverity":10,"validityPeriods":[123]}]}])");
  assert(parseLineStatus(mixed.as<JsonVariantConst>(),"dlr",now)==H::Unknown);
  mixed=json(R"({"error":"unauthorized"})");
  assert(parseLineStatus(mixed.as<JsonVariantConst>(),"dlr",now)==H::Unknown);
  assert(parseStationStatus(mixed.as<JsonVariantConst>(),"940GZZDLEIN",now)==H::Unknown);
  mixed=json(R"([{"stationAtcoCode":"940GZZDLEIN","type":"Closure","fromDate":"2020-01-01T00:00:00Z","toDate":"2099-01-01T00:00:00Z"}])");
  assert(parseStationStatus(mixed.as<JsonVariantConst>(),"940GZZDLEIN",now)==H::Issue);
  assert(parseStationStatus(mixed.as<JsonVariantConst>(),"940GZZDLCGT",now)==H::Good);
  mixed=json(R"([{"stationAtcoCode":"940GZZDLEIN","type":"Closure","toDate":"2020-01-01T00:00:00Z"}])");
  assert(parseStationStatus(mixed.as<JsonVariantConst>(),"940GZZDLEIN",now)==H::Good);
  assert(transportEpoch("2026-10-06T12:34:00+01:00")==transportEpoch("2026-10-06T11:34:00.000Z"));
  assert(transportEpoch("2026-02-30T12:34:00Z")==-1);
  assert(combineStationHealth(H::Good,H::Unknown)==H::Unknown);
  assert(combineStationHealth(H::Good,H::Notice)==H::Notice);
  TransportCheck check{H::Good,now,true};
  assert(currentTransportHealth(check,now+180,true,180)==H::Good);
  assert(currentTransportHealth(check,now+181,true,180)==H::Stale);
  assert(currentTransportHealth(check,now,false,180)==H::Stale);
  check.latestRequestOk=false;assert(currentTransportHealth(check,now,true,180)==H::Stale);
  TransportRecord tc{};strcpy(tc.dlrStation,"940GZZDLEIN");strcpy(tc.jubileeStation,"940GZZLUCGT");tc.snapshot.dlr={H::Good,now,true};tc.checksum=transportChecksum(tc);TransportSnapshot savedTransport;assert(loadTransportRecord(tc,tc.dlrStation,tc.jubileeStation,savedTransport)&&!savedTransport.dlr.latestRequestOk);assert(!loadTransportRecord(tc,"940GZZDLCGT",tc.jubileeStation,savedTransport));tc.snapshot.dlr.checkedAt++;assert(!loadTransportRecord(tc,tc.dlrStation,tc.jubileeStation,savedTransport));
  puts("TfL: live fixtures, station notices/closures, future/expired warnings, malformed responses and stale/offline states passed.");
  TransportCheck details{};details.health=H::Issue;auto disruption=json(R"([{"id":"jubilee","lineStatuses":[{"statusSeverity":6,"statusSeverityDescription":"Severe Delays","reason":"Signal failure at Canning Town"}]}])");lineDetails(disruption.as<JsonVariantConst>(),"jubilee",now,details);assert(!strcmp(details.label,"Severe Delays")&&strstr(details.reason,"Signal failure"));
  details={};details.health=H::Notice;stationDetails(stations.as<JsonVariantConst>(),"940GZZLUCGT",now,details);assert(*details.label);
  auto multipleNotices=json(R"([{"stationAtcoCode":"940GZZLUCGT","type":"Information","description":"Lift notice"},{"stationAtcoCode":"940GZZLUCGT","type":"Closure","description":"Station closed"}])");
  TransportCheck stationIssue{};stationIssue.health=H::Issue;stationDetails(multipleNotices.as<JsonVariantConst>(),"940GZZLUCGT",now,stationIssue);assert(!strcmp(stationIssue.reason,"Station closed"));
  TransportCheck lineIssue{};lineIssue.health=H::Issue;TransportCheck stationNotice{};stationNotice.health=H::Notice;assert(&journeyDetail(lineIssue,stationNotice)==&lineIssue);assert(&journeyDetail(lineIssue,stationIssue)==&stationIssue);
  char safe[12];displayText(safe,sizeof(safe),"<b> fault\n message");assert(strlen(safe)<sizeof(safe)&&!strchr(safe,'<')&&!strchr(safe,'\n'));


  DashboardText text{};DashboardData data{};TransportSnapshot t{};
  composeDashboard(data,text,nullptr,false,t,now,now,true,180,"London");
  assert(!data.weatherAvailable && strcmp(data.clock,"--:--") && strstr(data.statusText,"UNAVAILABLE"));
  for(auto &h:data.forecast)assert(!h.available);
  char clock[6];snprintf(clock,sizeof(clock),"%s",data.clock);
  composeDashboard(data,text,&w,true,t,now,now,false,180,"London");
  assert(data.weatherAvailable && !strcmp(clock,data.clock) && strstr(data.statusText,"CACHED"));
  assert(data.forecast[0].available);
  composeDashboard(data,text,&w,true,t,0,0,false,180,"London");
  assert(!strcmp(data.clock,"--:--") && data.weatherAvailable && !data.forecast[0].available);
  composeDashboard(data,text,&w,true,t,w.hourly[w.hourCount-1].epoch+3600,0,false,180,"London");
  for(auto &h:data.forecast)assert(!h.available);
  puts("Dashboard: NTP clock without weather, cold offline placeholder, restored cache and expired forecast slots passed.");
  t.dlr=t.jubilee=t.canningTube=t.eastIndia={H::Good,now,true};composeDashboard(data,text,&w,false,t,now,now,false,780,"London");assert(data.dlr==H::Good&&data.jubilee==H::Good);t.eastIndia.latestRequestOk=false;composeDashboard(data,text,&w,false,t,now,now,true,780,"London");assert(data.eastIndia==H::Stale);


  static uint8_t oldFrame[DASH_BYTES+64],newFrame[DASH_BYTES+64];
  memset(oldFrame,0xa7,sizeof(oldFrame));memset(newFrame,0xa7,sizeof(newFrame));
  DashboardData before=sampleDashboard(),after=before;after.canningTown=H::Issue;
  renderDashboard(oldFrame+32,before);renderDashboard(newFrame+32,after);
  ClockWindow windows[4];int count=changedDashboardWindows(before.clock,before,after,windows);assert(count==1);
  for(int y=0;y<DASH_HEIGHT;y++)for(int x=0;x<DASH_WIDTH;x++) {
    int index=32+y*340+x/4,shift=6-2*(x%4);
    if(((oldFrame[index]^newFrame[index])>>shift)&3)assert(x>=windows[0].x && x<windows[0].x+windows[0].width && y>=windows[0].y && y<windows[0].y+windows[0].height);
  }
  after.clock="12:35";assert(changedDashboardWindows(before.clock,before,after,windows)==2);
  const H states[]={H::Unknown,H::Good,H::Notice,H::Issue,H::Stale};
  for(H a:states)for(H b:states)for(int which=0;which<2;which++) {
    before=sampleDashboard();before.dlr=before.eastIndia=before.jubilee=before.canningTown=H::Good;
    after=before;
    if(which==0){before.eastIndia=a;after.eastIndia=b;}else {before.canningTown=a;after.canningTown=b;}
    renderDashboard(oldFrame+32,before);renderDashboard(newFrame+32,after);
    count=changedDashboardWindows(before.clock,before,after,windows);
    assert(count==(a==b?0:1));
    for(int y=0;y<DASH_HEIGHT;y++)for(int x=0;x<DASH_WIDTH;x++) {
      int index=32+y*340+x/4,shift=6-2*(x%4);
      if(((oldFrame[index]^newFrame[index])>>shift)&3) {
        assert(count==1);auto region=windows[0];
        assert(x>=region.x && x<region.x+region.width && y>=region.y && y<region.y+region.height);
      }
    }
    for(int y=CLOCK_AREA.y;y<CLOCK_AREA.y+CLOCK_AREA.height;y++)for(int x=CLOCK_AREA.x;x<CLOCK_AREA.x+CLOCK_AREA.width;x++) {
      int colour=(newFrame[32+y*340+x/4]>>(6-2*(x%4)))&3;assert(colour<=1);
    }
    assert(newFrame[32+472/4]==0x55 && newFrame[32+680/4]==0x55);
  }
  before=sampleDashboard();before.dlr=before.eastIndia=before.jubilee=before.canningTown=H::Good;
  after=before;after.eastIndia=after.canningTown=H::Issue;
  count=changedDashboardWindows(before.clock,before,after,windows);assert(count==1 && windows[0].width==1360);
  assert(journeyHealth(H::Good,H::Stale)==H::Stale && journeyHealth(H::Issue,H::Good)==H::Issue);
  after=before;after.clock="10:00";
  assert(changedDashboardWindows("09:59",before,after,windows,true)==3);
  puts("Colour: all 50 journey transitions are confined to their badge; both badges merge, clock stays monochrome, padding stays white.");
  composeDashboard(data,text,nullptr,false,t,now,now,false,180,"London");renderDashboard(newFrame+32,data);
  composeDashboard(data,text,&w,true,t,0,0,false,180,"London");renderDashboard(newFrame+32,data);
  for(int i=0;i<32;i++)assert(newFrame[i]==0xa7 && newFrame[DASH_BYTES+32+i]==0xa7);
  puts("Rendering: transport-only window covers every changed pixel; combined clock/status windows and unavailable-state guards passed.");
}
