#include "bsp.h"
#include <stdio.h>

static volatile u32 g_ms = 0;

/* USART1(PA9/PA10) -> CH340，仅用于调试打印 */
static void debug_uart_init(u32 baud)
{
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef us;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
	gpio.GPIO_Pin = GPIO_Pin_9;
	gpio.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio);
	gpio.GPIO_Pin = GPIO_Pin_10;
	gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &gpio);

	USART_StructInit(&us);
	us.USART_BaudRate = baud;
	us.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART1, &us);
	USART_Cmd(USART1, ENABLE);
}

/* printf 重定向（工程使用 MicroLIB） */
int fputc(int ch, FILE *f)
{
	(void)f;
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	USART_SendData(USART1, (u16)ch);
	return ch;
}

/* TIM2 1ms 中断，提供 millis() 时基 */
static void tick_timer_init(void)
{
	TIM_TimeBaseInitTypeDef tb;
	NVIC_InitTypeDef nvic;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	TIM_TimeBaseStructInit(&tb);
	tb.TIM_Prescaler = 72 - 1;        /* 72MHz / 72 = 1MHz */
	tb.TIM_Period = 1000 - 1;         /* 1ms */
	TIM_TimeBaseInit(TIM2, &tb);
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

	nvic.NVIC_IRQChannel = TIM2_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 3;
	nvic.NVIC_IRQChannelSubPriority = 0;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);
	TIM_Cmd(TIM2, ENABLE);
}

void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
		g_ms++;
	}
}

u32 millis(void)
{
	return g_ms;
}

void bsp_init(void)
{
	GPIO_InitTypeDef gpio;

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
	                       RCC_APB2Periph_GPIOE, ENABLE);

	/* DS0(PB5) 灭、BEEP(PB8) 不响 */
	GPIO_SetBits(GPIOB, GPIO_Pin_5);
	GPIO_ResetBits(GPIOB, GPIO_Pin_8);
	gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_8;
	gpio.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio);

	/* DS1(PE5) 灭 */
	GPIO_SetBits(GPIOE, GPIO_Pin_5);
	gpio.GPIO_Pin = GPIO_Pin_5;
	GPIO_Init(GPIOE, &gpio);

	/* KEY_UP(PA0) 下拉；K1~K3(PE2~PE4) 上拉 */
	gpio.GPIO_Pin = GPIO_Pin_0;
	gpio.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_Init(GPIOA, &gpio);
	gpio.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
	gpio.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOE, &gpio);

	debug_uart_init(115200);
	tick_timer_init();
}

u8 bsp_key_scan(void)
{
	static u8 last = 0;
	u8 now = 0, pressed;

	if (KEY_UP_IN) now |= KEY_UP;
	if (!KEY1_IN)  now |= KEY_1;
	if (!KEY2_IN)  now |= KEY_2;
	if (!KEY3_IN)  now |= KEY_3;
	pressed = now & (u8)~last;
	last = now;
	return pressed;
}
