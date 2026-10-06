// Native ESP32 port of main.py render_screen() at upstream ceced5eb.
#include "Dashboard.h"
#include "ClockUpdate.h"
#include "DashboardAssets.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
uint8_t *canvas;
constexpr int BLACK=0,WHITE=1;
void pixel(int x,int y,int c=BLACK) {
  if(x<0 || x>=DASH_WIDTH || y<0 || y>=DASH_HEIGHT)return;
  int i=y*340+x/4,s=6-2*(x%4);
  canvas[i]=(canvas[i]&~(3<<s))|(c<<s);
}
void fill(int x,int y,int w,int h,int c=BLACK) {
  for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)pixel(i,j,c);
}
void line(int x0,int y0,int x1,int y1,int width=2) {
  int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
  for(;;){fill(x0,y0,width,width);if(x0==x1 && y0==y1)break;int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}
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
void clockRow(int x,int y,int width,int height,const char *value) {
  // Fit and centre the complete HH:MM string, preserving the LED dot aspect.
  int top=10000,bottom=-10000,total=textWidth(value,font180);
  for(const char *c=value;*c;c++){const Glyph &g=glyph(font180,*c);if(g.y<top)top=g.y;if(g.y+g.h>bottom)bottom=g.y+g.h;}
  if(total<=0 || bottom<=top)return;
  float scale=float(width)/total;if((bottom-top)*scale>height)scale=float(height)/(bottom-top);
  float cursor=x+(width-total*scale)/2;int base=y+int((height-(bottom-top)*scale)/2-top*scale);
  for(const char *c=value;*c;c++){
    const Glyph &g=glyph(font180,*c);int stride=(g.w+7)/8;
    int w=int(g.w*scale),h=int(g.h*scale);
    for(int j=0;j<h;j++)for(int i=0;i<w;i++){
      int sx=int(i/scale),sy=int(j/scale);
      if(font180.bits[g.offset+sy*stride+sx/8]&(0x80>>(sx%8)))pixel(int(cursor+g.x*scale)+i,base+int(g.y*scale)+j);
    }
    cursor+=g.advance*scale;
  }
}
void compass(int cx,int cy,int direction,float speed) {
  const float rad=3.14159265359f/180;
  for(int a=0;a<360;a++){int x=cx+int(round(60*cos(a*rad))),y=cy+int(round(60*sin(a*rad)));fill(x,y,2,2);}
  for(int a=0;a<360;a+=45){int r=a%90==0?52:56;line(cx+int(r*cos(a*rad)),cy+int(r*sin(a*rad)),cx+int(60*cos(a*rad)),cy+int(60*sin(a*rad)));}
  text(cx-8,cy-82,"N",font20);text(cx-8,cy+64,"S",font20);text(cx+66,cy-10,"E",font20);text(cx-84,cy-10,"W",font20);
  float a=(direction-90)*rad;
  int tx=cx+int(48*cos(a)),ty=cy+int(48*sin(a));
  int lx=cx+int(20*cos(a+150*rad)),ly=cy+int(20*sin(a+150*rad));
  int rx=cx+int(20*cos(a-150*rad)),ry=cy+int(20*sin(a-150*rad));
  // Fill the triangular arrow with scanline interpolation.
  for(int y=cy-60;y<=cy+60;y++){
    int xs[3],n=0;int vx[]={tx,lx,rx},vy[]={ty,ly,ry};
    for(int i=0;i<3;i++){int j=(i+1)%3;if((vy[i]<=y && y<vy[j])||(vy[j]<=y && y<vy[i]))xs[n++]=vx[i]+(y-vy[i])*(vx[j]-vx[i])/(vy[j]-vy[i]);}
    if(n>=2){if(xs[0]>xs[1]){int t=xs[0];xs[0]=xs[1];xs[1]=t;}fill(xs[0],y,xs[1]-xs[0]+1,1);}
  }
  char s[24];snprintf(s,sizeof(s),"%.1f km/h",speed);text(cx-textWidth(s,font20)/2,cy+25,s,font20);
}
}
void renderDashboard(uint8_t *buffer,const DashboardData &d) {
  canvas=buffer;memset(canvas,0x55,DASH_BYTES);char s[100];
  // Current weather occupies one third; a single-row clock occupies two thirds.
  icon(20,20,d.weatherIcon,90);
  snprintf(s,sizeof(s),"%d°C",d.temperature);text(120,10,s,font80,BLACK,210);
  centeredText(386,8,"WIND",font20,92);
  if(d.windSpeed<0)snprintf(s,sizeof(s),"--");else snprintf(s,sizeof(s),"%.1f",d.windSpeed);
  centeredText(386,27,s,font60,92);centeredText(386,95,"km/h",font14,92);
  snprintf(s,sizeof(s),"Humidity: %d%%",d.humidity);text(120,95,s,font20);
  snprintf(s,sizeof(s),"Press: %d hPa",d.pressure);text(120,120,s,font20);line(20,150,433,150);
  icon(25,165,"icon_wind",30);compass(100,240,d.windDirection,d.windSpeed);
  text(200,170,"AIR QUALITY",font20);text(200,215,"AQI:",font28);
  if(d.aqi<0)snprintf(s,sizeof(s),"--");else snprintf(s,sizeof(s),"%d",d.aqi);int aw=textWidth(s,font80);
  if(d.aqi>=50){fill(265,240,(aw>140?140:aw)+30,68);text(280,225,s,font80,WHITE,140);}else text(280,225,s,font80,BLACK,140);
  line(453,10,453,322);

  snprintf(s,sizeof(s),"%s  /  %s",d.date,d.weekday);
  int dateWidth=textWidth(s,font24);if(dateWidth>850)dateWidth=850;
  text(473+(867-dateWidth)/2,6,s,font24,BLACK,850);
  clockRow(473,43,867,279,d.clock);

  // Forecast replaces the progress bars and runs across both top regions.
  line(20,326,1340,326);
  for(int i=0;i<8;i++){
    int x=20+i*165,center=x+82;
    centeredText(center,330,d.forecast[i].time,font24,149);
    icon(center-40,359,d.forecast[i].icon,80);
    int rain=d.forecast[i].rainProbability;if(rain<0)rain=0;if(rain>100)rain=100;
    snprintf(s,sizeof(s),"%d°C / %d%%",d.forecast[i].temperature,rain);
    centeredText(center,441,s,font20,149);
  }
  if(d.statusText)text(20,463,d.statusText,font14,BLACK,1320);
  else if(d.sampleData)text(20,463,"SAMPLE DATA  /  ESP32-S3  /  FORECAST: °C / RAIN CHANCE",font14);
}

int changedClockWindows(const char *previous,const char *current,ClockWindow windows[4]) {
  if(!previous || !current || strlen(previous)!=5 || strlen(current)!=5 || previous[2]!=':' || current[2]!=':') {
    windows[0]={472,40,868,284};return 1;
  }
  // Same metrics as clockRow. These LED digits have equal advances and bounds.
  int top=10000,bottom=-10000,total=textWidth(current,font180);
  for(int i=0;i<5;i++) {
    const Glyph &g=glyph(font180,current[i]);
    if(g.y<top)top=g.y;if(g.y+g.h>bottom)bottom=g.y+g.h;
  }
  float scale=float(867)/total;if((bottom-top)*scale>279)scale=float(279)/(bottom-top);
  float cursor=473+(867-total*scale)/2;
  int count=0,lastChanged=-2;
  for(int i=0;i<5;i++) {
    const Glyph &g=glyph(font180,current[i]);
    if(i!=2 && previous[i]!=current[i]) {
      int left=int(floor(cursor))&~3,right=(int(ceil(cursor+g.advance*scale))+3)&~3;
      ClockWindow window={uint16_t(left),40,uint16_t(right-left),284};
      if(lastChanged==i-1) windows[count-1].width=right-windows[count-1].x;
      else windows[count++]=window;
      lastChanged=i;
    }
    cursor+=g.advance*scale;
  }
  return count;
}
