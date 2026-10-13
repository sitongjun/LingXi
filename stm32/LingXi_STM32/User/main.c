/*
 * LingXi_STM32 —— 灵犀居家管家 STM32 家居主控
 * M1：通过 USART3 与 DNESP32S3 通信，接收大模型下发的控制命令
 */
#include "system.h"
#include "SysTick.h"
#include "bsp.h"
#include "link.h"
#include "home.h"
#include "lcd.h"
#include <stdio.h>

static link_frame_t rx_frame;   /* 1KB 缓冲，放在全局区避免栈溢出 */

int main(void)
{
	SysTick_Init(72);
	bsp_init();
	link_init(115200);
	home_init();
	{
		/* 点屏测试：四角色块判断方向，中间色条判断 RGB 顺序 */
		u16 id = lcd_init();
		printf("LCD ID = 0x%04X\r\n", id);
		lcd_clear(C_BLACK);
		lcd_fill(0, 0, 60, 60, C_WHITE);                    /* 左上 白 */
		lcd_fill(LCD_W - 60, 0, 60, 60, C_RED);             /* 右上 红 */
		lcd_fill(0, LCD_H - 60, 60, 60, C_GREEN);           /* 左下 绿 */
		lcd_fill(LCD_W - 60, LCD_H - 60, 60, 60, C_BLUE);   /* 右下 蓝 */
		lcd_fill(40, 160, 240, 40, C_RED);
		lcd_fill(40, 220, 240, 40, C_GREEN);
		lcd_fill(40, 280, 240, 40, C_BLUE);
	}

	printf("\r\n=== LingXi_STM32 M1 ===\r\n");
	printf("link: USART3 PB10(TX) PB11(RX) 115200\r\n");
	BEEP = 1; delay_ms(80); BEEP = 0;

	while (1)
	{
		while (link_poll(&rx_frame))
			home_handle_frame(&rx_frame);
		home_tick();
	}
}
