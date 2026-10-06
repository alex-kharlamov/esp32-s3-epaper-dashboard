#include "Dashboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=2 && argc!=3)return 1;
 uint8_t *allocation=(uint8_t*)malloc(DASH_BYTES+64);if(!allocation)return 2;
 memset(allocation,0xa7,DASH_BYTES+64);uint8_t *image=allocation+32;
 if(argc==3 && !strcmp(argv[2],"sleep"))renderSleepScreen(image);
 else renderDashboard(image,sampleDashboard());
 FILE *out=fopen(argv[1],"wb");if(!out)return 3;
 size_t n=fwrite(image,1,DASH_BYTES,out);fclose(out);
 // Check renderer clipping with long labels, negative temperature and missing icons.
 DashboardData stress=sampleDashboard();stress.temperature=-99;stress.windSpeed=123;
 stress.date="A very long date label that must fit";
 stress.location="A very long location label that must fit";
 stress.weatherIcon="missing_icon";
 renderDashboard(image,stress);
 renderSleepScreen(image);
 for(int i=0;i<32;i++)if(allocation[i]!=0xa7 || allocation[DASH_BYTES+32+i]!=0xa7)return 5;
 free(allocation);puts("Renderer clipping/buffer guards passed");return n==DASH_BYTES?0:4;
}
