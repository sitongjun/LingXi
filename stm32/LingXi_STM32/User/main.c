/*
 * LingXi_STM32 —— 灵犀居家管家 STM32 家居主控
 * M1：通过 USART3 与 DNESP32S3 通信，接收大模型下发的控制命令
 * M2：DHT11 温湿度采集、超阈值自动报警、3.5 寸彩屏状态面板
 */
#include "system.h"
#include "SysTick.h"
#include "bsp.h"
#include "link.h"
#include "home.h"
#include "lcd.h"
#include "ui.h"
#include <stdio.h>

static link_frame_t rx_frame;   /* 2KB 缓冲，放在全局区避免栈溢出 */

int main(void)
{
	u16 lcd_id;

	SysTick_Init(72);
	bsp_init();
	link_init(115200);
	lcd_id = lcd_init();
	home_init();
	ui_init();

	printf("\r\n=== LingXi_STM32 M2 ===\r\n");
	printf("link: USART3 PB10(TX) PB11(RX) 115200, LCD ID 0x%04X\r\n", lcd_id);
	BEEP = 1; delay_ms(80); BEEP = 0;

	while (1)
	{
		while (link_poll(&rx_frame))
			home_handle_frame(&rx_frame);
		home_tick();
		ui_tick();
	}
}
