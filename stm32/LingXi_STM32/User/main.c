/*
 * LingXi_STM32 —— 灵犀居家管家 STM32 家居主控
 * M1：通过 USART3 与 DNESP32S3 通信，接收大模型下发的控制命令
 */
#include "system.h"
#include "SysTick.h"
#include "bsp.h"
#include "link.h"
#include "home.h"
#include <stdio.h>

static link_frame_t rx_frame;   /* 1KB 缓冲，放在全局区避免栈溢出 */

int main(void)
{
	SysTick_Init(72);
	bsp_init();
	link_init(115200);
	home_init();

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
