// Native ESP32 port of main.py render_screen() at upstream ceced5eb.
#include "Dashboard.h"
#include "ClockUpdate.h"
#include "DashboardAssets.h"
#include "SleepArtwork.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {
uint8_t *canvas;
constexpr int BLACK=0,WHITE=1,YELLOW=2,RED=3;
void pixel(int x,int y,int c=BLACK) {
  if(x<0 || x>=DASH_WIDTH || y<0 || y>=DASH_HEIGHT)return;
  int i=y*340+x/4,s=6-2*(x%4);
  canvas[i]=(canvas[i]&~(3<<s))|(c<<s);
}
void fill(int x,int y,int w,int h,int c=BLACK) {
  for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)pixel(i,j,c);
}
void line(int x0,int y0,int x1,int y1,int width=2,int c=BLACK) {
  int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
  for(;;){fill(x0,y0,width,width,c);if(x0==x1 && y0==y1)break;int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}
}
void box(int x,int y,int w,int h,int width=2) {
  fill(x,y,w,width);fill(x,y+h-width,w,width);fill(x,y,width,h);fill(x+w-width,y,width,h);
}
uint16_t nextCode(const char *&s) {
  uint8_t a=*s++;if(a<128)return a;
  if((a&0xe0)==0xc0 && *s){uint8_t b=*s++;return ((a&31)<<6)|(b&63);}return '?';
}
const Glyph &glyph(const BitmapFont &f,int code) {
  for(int i=0;i<f.count;i++)if(f.glyphs[i].code==code)return f.glyphs[i];return f.glyphs[0];
}
int textWidth(const char *str,const BitmapFont &font) {
  int w=0;while(*str)w+=glyph(font,nextCode(str)).advance;return w;
}
void text(int x,int y,const char *str,const BitmapFont &font,int c=BLACK,int maxWidth=0) {
  float scale=1.0f;int total=textWidth(str,font);if(maxWidth>0 && total>maxWidth)scale=float(maxWidth)/total;
  float cursor=x;
  while(*str){const Glyph &g=glyph(font,nextCode(str));int stride=(g.w+7)/8;
    int w=int(ceil(g.w*scale)),h=int(ceil(g.h*scale));
    for(int j=0;j<h;j++)for(int i=0;i<w;i++){
      int sx=int(i/scale),sy=int(j/scale);
      if(sx<g.w && sy<g.h && (font.bits[g.offset+sy*stride+sx/8]&(0x80>>(sx%8))))
        pixel(int(cursor+g.x*scale)+i,y+int(g.y*scale)+j,c);
    }
    cursor+=g.advance*scale;
  }
}
void centeredText(int cx,int y,const char *str,const BitmapFont &font,int maxWidth) {
  int width=textWidth(str,font);if(width>maxWidth)width=maxWidth;
  text(cx-width/2,y,str,font,BLACK,maxWidth);
}
void icon(int x,int y,const char *name,int size) {
  for(int k=0;k<dashboardIconCount;k++){
    const auto &a=dashboardIcons[k];if(a.width!=size || strcmp(a.name,name))continue;
    int stride=(a.width+7)/8;for(int j=0;j<a.height;j++)for(int i=0;i<a.width;i++)
      if(a.bits[j*stride+i/8]&(0x80>>(i%8)))pixel(x+i,y+j);return;
  }
  box(x,y,size,size);
}
// Outline icons use solid native pigment values; only the sun is yellow.
void disc(int x,int y,int radius,int c) {
  for(int j=-radius;j<=radius;j++)for(int i=-radius;i<=radius;i++)
    if(i*i+j*j<=radius*radius)pixel(x+i,y+j,c);
}
void sun(int x,int y,int size) {
  int cx=x+size/2,cy=y+size/2,r=size/4;
  disc(cx,cy,r,YELLOW);
  for(int i=0;i<8;i++) {
    float angle=i*3.14159265f/4;
    int a=size*34/100,b=size*46/100;
    line(cx+int(cos(angle)*a),cy+int(sin(angle)*a),cx+int(cos(angle)*b),cy+int(sin(angle)*b),size>90?4:3,YELLOW);
  }
}
bool cloudMask(int u,int v) {
  auto circle=[&](int x,int y,int r){return (u-x)*(u-x)+(v-y)*(v-y)<=r*r;};
  return circle(30,53,20) || circle(50,40,25) || circle(71,54,19) || (u>=29 && u<=73 && v>=50 && v<=73);
}
void weatherIcon(int x,int y,const char *name,int size) {
  bool clear=!strcmp(name,"icon_sun"),partly=!strcmp(name,"icon_partly-cloudy-day");
  bool cloudy=!strcmp(name,"icon_clouds"),rain=!strcmp(name,"icon_rain") || !strcmp(name,"icon_heavy_rain");
  if(clear){sun(x,y,size);return;}
  if(!partly && !cloudy && !rain){icon(x,y,name,size);return;}
  if(partly)sun(x+size*42/100,y-size*13/100,size*68/100);
  for(int j=0;j<size;j++)for(int i=0;i<size;i++) {
    int u=i*100/size,v=j*100/size;
    if(!cloudMask(u,v))continue;
    bool edge=!cloudMask(u-4,v)||!cloudMask(u+4,v)||!cloudMask(u,v-4)||!cloudMask(u,v+4);
    pixel(x+i,y+j,edge?BLACK:WHITE);
  }
  if(rain)for(int i=0;i<3;i++) {
    int cx=x+size*(31+i*21)/100;
    line(cx,y+size*81/100,cx-size*5/100,y+size*92/100,size>90?4:3);
  }
}
void transportIndicator(int x,const char *name,ServiceHealth health,const char *label=nullptr,const char *reason=nullptr) {
  int background=health==ServiceHealth::Issue?RED:health==ServiceHealth::Notice?YELLOW:WHITE;
  int ink=background==RED?WHITE:BLACK;
  fill(x,252,680,44,background);
  if(health==ServiceHealth::Good){line(x+17,272,x+24,279,3,ink);line(x+24,279,x+37,263,3,ink);}
  else text(x+20,258,health==ServiceHealth::Issue || health==ServiceHealth::Notice?"!":"?",font28,ink,22);
  const char *status=health==ServiceHealth::Good?"GOOD":health==ServiceHealth::Notice?"NOTICE":health==ServiceHealth::Issue?"ISSUE":health==ServiceHealth::Stale?"OLD":"UNKNOWN";
  if(label&&*label)status=label;
  // Bound detailed status to leave the station label readable.
  int statusWidth=textWidth(status,font20);if(statusWidth>300){status=health==ServiceHealth::Issue?"DISRUPTION":health==ServiceHealth::Notice?"NOTICE":status;statusWidth=textWidth(status,font20);}
  bool detail=reason&&*reason;
  text(x+49,detail?254:259,name,font20,ink,680-65-statusWidth-20);
  text(x+660-statusWidth,detail?254:259,status,font20,ink);
  if(detail)text(x+49,278,reason,font14,ink,611);
}
struct ClockGeometry {
  int top,bottom,total,base;
  float scale,left;
};
ClockGeometry clockGeometry() {
  // Measure the complete digit repertoire once, not the current time. Both
  // advances and vertical bounds stay stable across every minute transition.
  int top=10000,bottom=-10000;
  for(const char *c="0123456789:";*c;c++) {
    const Glyph &g=glyph(font180,*c);
    if(g.y<top)top=g.y;if(g.y+g.h>bottom)bottom=g.y+g.h;
  }
  int total=textWidth("00:00",font180);
  float scale=float(867)/total;
  if((bottom-top)*scale>236)scale=float(236)/(bottom-top);
  float left=473+(867-total*scale)/2;
  int base=8+int((236-(bottom-top)*scale)/2-top*scale);
  return {top,bottom,total,base,scale,left};
}
void clockRow(const char *value) {
  const auto m=clockGeometry();float cursor=m.left;
  for(const char *c=value;*c;c++) {
    const Glyph &g=glyph(font180,*c);int stride=(g.w+7)/8;
    int w=int(g.w*m.scale),h=int(g.h*m.scale);
    for(int j=0;j<h;j++)for(int i=0;i<w;i++) {
      int sx=int(i/m.scale),sy=int(j/m.scale);
      if(font180.bits[g.offset+sy*stride+sx/8]&(0x80>>(sx%8)))
        pixel(int(cursor+g.x*m.scale)+i,m.base+int(g.y*m.scale)+j);
    }
    cursor+=g.advance*m.scale;
  }
}
void upperCopy(char *out,int length,const char *value) {
  int i=0;for(;value && value[i] && i<length-1;i++) {
    char c=value[i];out[i]=(c>='a' && c<='z')?c-'a'+'A':c;
  }
  out[i]=0;
}
const char *condition(const char *name) {
  if(!strcmp(name,"icon_sun"))return "CLEAR";
  if(!strcmp(name,"icon_night"))return "CLEAR NIGHT";
  if(!strcmp(name,"icon_partly-cloudy-day"))return "PARTLY CLOUDY";
  if(!strcmp(name,"icon_clouds"))return "CLOUDY";
  if(!strcmp(name,"icon_rain"))return "RAIN";
  if(!strcmp(name,"icon_heavy_rain"))return "HEAVY RAIN";
  if(!strcmp(name,"icon_snow"))return "SNOW";
  if(!strcmp(name,"icon_storm"))return "THUNDERSTORM";
  if(!strcmp(name,"icon_windy"))return "WINDY";
  return "WEATHER";
}
const char *healthLabel(ServiceHealth state) {
  switch(state) {
    case ServiceHealth::Good:return "OK";
    case ServiceHealth::Notice:return "NOTICE";
    case ServiceHealth::Issue:return "ISSUE";
    case ServiceHealth::Stale:return "OLD";
    default:return "?";
  }
}

}
void renderSleepScreen(uint8_t *buffer,unsigned wakeHour) {
  canvas=buffer;memset(canvas,0x55,DASH_BYTES);
  for(int y=0;y<SLEEP_ART_HEIGHT;y++)for(int x=0;x<SLEEP_ART_WIDTH;x++) {
    int i=y*(SLEEP_ART_WIDTH/4)+x/4,shift=6-2*(x%4);
    pixel(24+x,y,(sleepArtwork[i]>>shift)&3);
  }
  centeredText(1030,115,"GOOD NIGHT",font60,590);
  centeredText(1030,202,"A LITTLE REST",font35,560);
  char wake[32];snprintf(wake,sizeof(wake),"BACK AT %02u:00",wakeHour);centeredText(1030,267,wake,font35,560);
  centeredText(680,421,"WEATHER AND TFL WILL RESUME IN THE MORNING",font20,1260);
}

void renderDashboard(uint8_t *buffer,const DashboardData &d) {
  canvas=buffer;memset(canvas,0x55,DASH_BYTES);char s[180],label[100];
  upperCopy(label,sizeof(label),d.location);text(20,0,label,font60,BLACK,420);
  snprintf(s,sizeof(s),"%s %s",d.weekday,d.date);upperCopy(label,sizeof(label),s);
  text(20,55,label,font24,BLACK,420);
  if(d.weatherAvailable)snprintf(s,sizeof(s),"%d°%s",d.temperature,d.fahrenheit?"F":"C");else snprintf(s,sizeof(s),"--°C");
  text(180,86,s,textWidth(s,font120)<=260?font120:font80,BLACK,260);
  if(d.weatherAvailable)weatherIcon(28,88,d.weatherIcon,120);
  char weatherLabel[64];
  snprintf(weatherLabel,sizeof(weatherLabel),"%s",d.weatherAvailable?condition(d.weatherIcon):"WEATHER UNAVAILABLE");
  char *secondWord=strchr(weatherLabel,' ');
  if(secondWord) {
    *secondWord++=0;
    text(180,188,weatherLabel,font35,BLACK,260);
    text(180,219,secondWord,font35,BLACK,260);
  }else text(180,203,weatherLabel,font35,BLACK,260);
  static const char *directions[]={"N","NE","E","SE","S","SW","W","NW"};
  int direction=((d.windDirection%360)+360)%360;
  if(d.windSpeed<0)snprintf(s,sizeof(s),"-- km/h");
  else snprintf(s,sizeof(s),"%s %.0f %s",directions[((direction+22)/45)%8],d.windSpeed,d.windMph?"mph":"km/h");
  icon(18,213,"icon_wind",30);text(52,215,s,font24,BLACK,120);
  line(453,8,453,244,1);
  if(!strcmp(d.clock,"--:--"))centeredText(906,95,"TIME WAITING",font60,830);
  else clockRow(d.clock);

  transportIndicator(0,d.dlrName,journeyHealth(d.dlr,d.eastIndia),d.dlrLabel,d.dlrReason);
  transportIndicator(680,d.jubileeName,journeyHealth(d.jubilee,d.canningTown),d.jubileeLabel,d.jubileeReason);
  line(0,252,1359,252,1);line(0,295,1359,295,1);
  for(int i=0;i<8;i++) {
    int x=i*170,center=x+85;
    centeredText(center,304,d.forecast[i].time,font24,158);
    if(i>0)line(x,308,x,454,1);
    if(d.forecast[i].available)weatherIcon(center-40,336,d.forecast[i].icon,80);
    else centeredText(center,365,"--",font28,150);
    int rain=d.forecast[i].rainProbability;if(rain<0)rain=0;if(rain>100)rain=100;
    char temp[24],chance[16];
    if(d.forecast[i].available){snprintf(temp,sizeof(temp),"%d° / ",d.forecast[i].temperature);snprintf(chance,sizeof(chance),"%d%%",rain);}
    else {snprintf(temp,sizeof(temp),"--° / ");snprintf(chance,sizeof(chance),"--%%");}
    const BitmapFont &forecastFont=textWidth(temp,font24)+textWidth(chance,font24)<=158?font24:font20;
    int tw=textWidth(temp,forecastFont),cw=textWidth(chance,forecastFont),left=center-(tw+cw)/2;
    bool highRain=d.forecast[i].available && rain>50;
    if(d.forecast[i].available && rain>=40)fill(left+tw-2,426,cw+4,26,highRain?RED:YELLOW);
    text(left,423,temp,forecastFont);text(left+tw,423,chance,forecastFont,highRain?WHITE:BLACK);
  }
  line(0,459,1359,459,1);
  const char *footer=d.statusText?d.statusText:(d.sampleData?"SAMPLE DATA":"WEATHER UNAVAILABLE");
  if(d.batteryPercent>=0&&d.batteryPercent<=15)fill(14,463,142,17,YELLOW);
  text(20,464,footer,font14,BLACK,1100);
  if(d.transportCheckedAt){time_t checked=d.transportCheckedAt;tm local{};localtime_r(&checked,&local);strftime(s,sizeof(s),"TfL %H:%M",&local);}
  else snprintf(s,sizeof(s),d.sampleData?"TfL 12:33 / SAMPLE":"TfL --:--");
  text(1340-textWidth(s,font14),464,s,font14);
}

int changedClockWindows(const char *previous,const char *current,ClockWindow windows[4]) {
  if(!previous || !current || strlen(previous)!=5 || strlen(current)!=5 || previous[2]!=':' || current[2]!=':') {
    windows[0]=CLOCK_AREA;return 1;
  }
  const auto m=clockGeometry();
  float cursor=m.left;float scale=m.scale;
  int count=0,lastChanged=-2;
  for(int i=0;i<5;i++) {
    const Glyph &g=glyph(font180,current[i]);
    if(i!=2 && previous[i]!=current[i]) {
      int left=int(floor(cursor))&~3,right=(int(ceil(cursor+g.advance*scale))+3)&~3;
      ClockWindow window={uint16_t(left),CLOCK_AREA.y,uint16_t(right-left),CLOCK_AREA.height};
      if(lastChanged==i-1) windows[count-1].width=right-windows[count-1].x;
      else windows[count++]=window;
      lastChanged=i;
    }
    cursor+=g.advance*scale;
  }
  return count;
}
