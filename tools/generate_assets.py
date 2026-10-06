#!/usr/bin/env python3
"""Bake Oxanium typography and existing weather icons for the native renderer."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageOps
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'ESP32S3_Dashboard'

def packed(img):
 pixels=list(img.convert('L').getdata());out=[]
 for y in range(img.height):
  for x in range(0,img.width,8):
   n=0
   for i in range(8):
    if x+i<img.width and pixels[y*img.width+x+i]>=128:n|=0x80>>i
   out.append(n)
 return out

def array(name,values):
 lines=['const uint8_t '+name+'[] = {']
 for i in range(0,len(values),24):lines.append('  '+','.join(f'0x{v:02x}' for v in values[i:i+24])+',')
 return '\n'.join(lines+['};'])
header='''// Generated from Oxanium (SIL OFL) and upstream icons by tools/generate_assets.py.
#pragma once
#include <stdint.h>
struct Glyph { uint16_t code; int16_t x,y,w,h,advance; uint32_t offset; };
struct BitmapFont { const uint8_t *bits; const Glyph *glyphs; int count; };
struct BitmapIcon { const char *name; int width,height; const uint8_t *bits; };
'''
cpp=['#include "DashboardAssets.h"']
for size in [14,20,24,28,32,35,60,80,120,180]:
 chars='0123456789:' if size==180 else ''.join(chr(i) for i in range(32,127))+'°'
 font=ImageFont.truetype(str(ROOT/'assets'/'Oxanium-Variable.ttf'),size)
 font.set_variation_by_axes([700 if size>=60 else 600])
 # Equal digit cells are essential: a changed minute cannot move other digits.
 digit_advance=max(round(font.getlength(ch)) for ch in '0123456789')
 bits=[];glyphs=[]
 for ch in chars:
  x,y,r,b=font.getbbox(ch);w,h=r-x,b-y
  img=Image.new('L',(max(1,w),max(1,h)));ImageDraw.Draw(img).text((-x,-y),ch,font=font,fill=255)
  off=len(bits);bits+=packed(img)
  advance=digit_advance if size==180 and ch.isdigit() else round(font.getlength(ch))
  glyphs.append((ord(ch),x,y,max(1,w),max(1,h),advance,off))
 cpp.append(array(f'font{size}Bits',bits))
 cpp.append(f'const Glyph font{size}Glyphs[] = {{'+','.join('{'+','.join(map(str,g))+'}' for g in glyphs)+'};')
 cpp.append(f'const BitmapFont font{size}={{font{size}Bits,font{size}Glyphs,{len(glyphs)}}};')
 header+=f'extern const BitmapFont font{size};\n'
icons=[('icon_cpu',50),('icon_btc',50),('icon_eth',50),('icon_wifi',50),('icon_wind',30),('icon_mail',60)]
for name in ['icon_sun','icon_night','icon_clouds','icon_partly-cloudy-day','icon_rain','icon_heavy_rain','icon_snow','icon_storm','icon_windy']:
 icons += [(name,60),(name,80),(name,90),(name,120)]
for n,size in icons:
 img=Image.open(ROOT/'assets/icons'/f'{n}.bmp').convert('L').resize((size,size),Image.Resampling.LANCZOS)
 # Binary threshold, no dithering: clean shapes survive e-paper refresh better.
 img=ImageOps.invert(img).point(lambda x:255 if x>=128 else 0,mode='1')
 cpp.append(array(f'{n.replace("-","_")}_{size}',packed(img)))
cpp.append('const BitmapIcon dashboardIcons[]={'+','.join('{"'+n+'",'+str(sz)+','+str(sz)+','+n.replace('-','_')+'_'+str(sz)+'}' for n,sz in icons)+'};')
cpp.append(f'const int dashboardIconCount={len(icons)};')
header+='extern const BitmapIcon dashboardIcons[];\nextern const int dashboardIconCount;\n'
(OUT/'DashboardAssets.h').write_text(header)
(OUT/'DashboardAssets.cpp').write_text('\n'.join(cpp)+'\n')
print('Generated fonts/icons:',sum(len(x) for x in cpp),'source bytes')
