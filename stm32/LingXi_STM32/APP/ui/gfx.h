#ifndef _GFX_H
#define _GFX_H

#include "lcd.h"
#include "font.h"

/* 文字对齐方式 */
enum { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 };

u16  gfx_blend(u16 fg, u16 bg, u8 alpha);    /* alpha 0~255 */
u16  gfx_text_width(const font_t *f, const char *s);

/* 在背景色 bg 上绘制 UTF-8 字符串，返回绘制宽度 */
u16  gfx_text(u16 x, u16 y, const char *s, const font_t *f, u16 fg, u16 bg);

/* 在 [x, x+w) 区域内按对齐方式绘制文字，并用 bg 清除区域内其余部分（适合刷新数值） */
void gfx_text_box(u16 x, u16 y, u16 w, const char *s, const font_t *f, u16 fg, u16 bg, u8 align);

void gfx_round_rect(u16 x, u16 y, u16 w, u16 h, u8 r, u16 color, u16 bg);
void gfx_circle(u16 cx, u16 cy, u8 r, u16 color, u16 bg);
void gfx_hline(u16 x, u16 y, u16 w, u16 color);
void gfx_vline(u16 x, u16 y, u16 h, u16 color);
void gfx_line(int x0, int y0, int x1, int y1, u16 color);
void gfx_vgradient(u16 x, u16 y, u16 w, u16 h, u16 top, u16 bottom);

#endif
