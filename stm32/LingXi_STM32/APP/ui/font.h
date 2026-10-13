#ifndef _FONT_H
#define _FONT_H

#include "system.h"

/* 4bpp 抗锯齿字模：每行按字节对齐，高半字节在左 */
typedef struct {
	u16 code;      /* Unicode */
	u8  w;         /* 字宽（像素） */
	u32 off;       /* 在位图数组中的偏移 */
} glyph_t;

typedef struct {
	u8             h;        /* 行高（像素） */
	u16            count;
	const glyph_t *glyphs;   /* 按 code 升序 */
	const u8      *bits;
} font_t;

extern const font_t font_16;      /* 微软雅黑 16px：ASCII + 界面用到的汉字 */
extern const font_t font_24;      /* 微软雅黑粗体 24px */
extern const font_t font_num48;   /* Bahnschrift 大号数字 */

#endif
