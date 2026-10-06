#include "Dashboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=2)return 1;
 uint8_t *allocation=(uint8_t*)malloc(DASH_BYTES+64);if(!allocation)return 2;
 memset(allocation,0xa7,DASH_BYTES+64);uint8_t *image=allocation+32;
 renderDashboard(image,sampleDashboard());
 FILE *out=fopen(argv[1],"wb");if(!out)return 3;
 size_t n=fwrite(image,1,DASH_BYTES,out);fclose(out);
 // Check clipping across high UV/AQI inversion, empty histories and long text.
 DashboardData stress=sampleDashboard();stress.uv=12;stress.aqi=999;
 stress.date="A very long date label that must fit";stress.unread=99999;
 stress.dayProgress=-1;stress.monthProgress=2;stress.cpuHistory=nullptr;
 stress.cpuCount=0;stress.weatherIcon="missing_icon";
 renderDashboard(image,stress);
 for(int i=0;i<32;i++)if(allocation[i]!=0xa7 || allocation[DASH_BYTES+32+i]!=0xa7)return 5;
 free(allocation);puts("Renderer clipping/buffer guards passed");return n==DASH_BYTES?0:4;
}
