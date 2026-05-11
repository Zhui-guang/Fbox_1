#ifndef FONT_ZH16_H
#define FONT_ZH16_H
#include <stdint.h>
typedef struct { uint16_t code; uint8_t rows[32]; } FontZh16Glyph;
extern const FontZh16Glyph g_font_zh16[];
extern const uint16_t g_font_zh16_count;
const FontZh16Glyph *FontZh16_Find(uint16_t code);
#endif
