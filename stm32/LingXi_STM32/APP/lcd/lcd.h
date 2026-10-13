#ifndef _LCD_H
#define _LCD_H

#include "system.h"

/*
 * ILI9488 3.5 寸 TFT（320x480），FSMC Bank1 NE4（PG12），RS = FSMC_A10（PG0），背光 PB0
 * 该屏数据按 8 位传输：每个 RGB565 像素分两次写入（高字节、低字节）
 * 16 位总线下 HADDR[11] 对应 A10：命令地址 A10=0，数据地址 A10=1
 */
typedef struct {
	volatile u16 REG;
	volatile u16 RAM;
} LCD_TypeDef;

#define LCD_BASE   ((u32)(0x6C000000 | 0x000007FE))
#define LCD        ((LCD_TypeDef *)LCD_BASE)

#define LCD_W      320
#define LCD_H      480

/* RGB565 颜色 */
#define RGB(r, g, b)  ((u16)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define C_BLACK    0x0000
#define C_WHITE    0xFFFF
#define C_RED      RGB(255, 0, 0)
#define C_GREEN    RGB(0, 255, 0)
#define C_BLUE     RGB(0, 0, 255)

u16  lcd_init(void);                                 /* 返回 0xD3 读到的 ID（本板读出 0x9486） */
void lcd_set_window(u16 x0, u16 y0, u16 x1, u16 y1);
void lcd_fill(u16 x, u16 y, u16 w, u16 h, u16 color);
void lcd_clear(u16 color);
void lcd_pixel(u16 x, u16 y, u16 color);

/* 写入窗口像素流（先 lcd_set_window） */
static __inline void lcd_write_px(u16 color) { LCD->RAM = color >> 8; LCD->RAM = color & 0xFF; }

#endif
