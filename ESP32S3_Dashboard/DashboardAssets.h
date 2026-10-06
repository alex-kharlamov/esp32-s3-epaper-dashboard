// Generated from upstream font/icon assets by tools/generate_assets.py.
#pragma once
#include <stdint.h>
struct Glyph { uint16_t code; int16_t x,y,w,h,advance; uint32_t offset; };
struct BitmapFont { const uint8_t *bits; const Glyph *glyphs; int count; };
struct BitmapIcon { const char *name; int width,height; const uint8_t *bits; };
extern const BitmapFont font14;
extern const BitmapFont font20;
extern const BitmapFont font24;
extern const BitmapFont font28;
extern const BitmapFont font32;
extern const BitmapFont font35;
extern const BitmapFont font60;
extern const BitmapFont font80;
extern const BitmapFont font180;
extern const BitmapIcon dashboardIcons[];
extern const int dashboardIconCount;
