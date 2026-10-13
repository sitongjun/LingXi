/*
 * LingXi 板级引脚验证程序 第 3 版
 * 第 2 版实测：BEEP=PB8（高电平响），DS0=PB5、DS1=PE5（低电平亮）
 * 第 3 版新增：USART3(PB10 TX / PB11 RX) 自环测试，PB10 与 PB11 短接后应打印 OK
 * 第 1 版实测：PB5 是 DS0 指示灯（不是蜂鸣器），按键 PA0/PE2/PE3/PE4 正确
 * 1. 上电蜂鸣器短响两声      -> 验证 BEEP = PB8（高电平响）
 * 2. DS0/DS1 交替闪烁         -> 验证 DS0 = PB5、DS1 = PE5（低电平亮）
 * 3. USART1 115200 打印信息   -> 验证 PA9/PA10 经 CH340 输出
 * 4. 按键按下时打印           -> 验证 KEY_UP=PA0(高有效)、K1~K3=PE2~PE4(低有效)
 */
#include "system.h"
#include "SysTick.h"
#include <stdio.h>

static void usart1_init(u32 baud)
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

	us.USART_BaudRate = baud;
	us.USART_WordLength = USART_WordLength_8b;
	us.USART_StopBits = USART_StopBits_1;
	us.USART_Parity = USART_Parity_No;
	us.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	us.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART1, &us);
	USART_Cmd(USART1, ENABLE);
}

/* printf 重定向到 USART1（不使用 MicroLIB 时需要关闭半主机） */
#pragma import(__use_no_semihosting)
struct __FILE { int handle; };
FILE __stdout;
void _sys_exit(int x) { (void)x; }
int fputc(int ch, FILE *f)
{
	(void)f;
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
	USART_SendData(USART1, (u16)ch);
	return ch;
}

static void usart3_init(u32 baud)
{
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef us;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
	gpio.GPIO_Pin = GPIO_Pin_10;
	gpio.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio);
	gpio.GPIO_Pin = GPIO_Pin_11;
	gpio.GPIO_Mode = GPIO_Mode_IPU;		/* 未接线时上拉，避免噪声 */
	GPIO_Init(GPIOB, &gpio);

	us.USART_BaudRate = baud;
	us.USART_WordLength = USART_WordLength_8b;
	us.USART_StopBits = USART_StopBits_1;
	us.USART_Parity = USART_Parity_No;
	us.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	us.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART3, &us);
	USART_Cmd(USART3, ENABLE);
}

/* 发送 4 字节并在约 5ms 内读回，全部一致返回 1 */
static u8 usart3_loopback(u8 seq)
{
	u8 tx[4], i, ok = 1;
	u16 wait;

	tx[0] = 'L'; tx[1] = 'X'; tx[2] = seq; tx[3] = (u8)~seq;
	(void)USART_ReceiveData(USART3);	/* 清空残留 */
	USART_ClearFlag(USART3, USART_FLAG_ORE);
	for (i = 0; i < 4; i++)
	{
		while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
		USART_SendData(USART3, tx[i]);
		for (wait = 0; wait < 500 && USART_GetFlagStatus(USART3, USART_FLAG_RXNE) == RESET; wait++)
			delay_us(10);
		if (USART_GetFlagStatus(USART3, USART_FLAG_RXNE) == RESET ||
		    (u8)USART_ReceiveData(USART3) != tx[i])
			ok = 0;
	}
	return ok;
}

static void board_gpio_init(void)
{
	GPIO_InitTypeDef gpio;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
	                       RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOE, ENABLE);

	/* 蜂鸣器 PB8：先置低（不响）；DS0 PB5：先置高（灭） */
	GPIO_ResetBits(GPIOB, GPIO_Pin_8);
	GPIO_SetBits(GPIOB, GPIO_Pin_5);
	gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_8;
	gpio.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio);

	/* DS1 PE5：先置高（灭） */
	GPIO_SetBits(GPIOE, GPIO_Pin_5);
	gpio.GPIO_Pin = GPIO_Pin_5;
	GPIO_Init(GPIOE, &gpio);

	/* KEY_UP PA0：下拉输入，按下为高 */
	gpio.GPIO_Pin = GPIO_Pin_0;
	gpio.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_Init(GPIOA, &gpio);

	/* K1~K3 PE2~PE4：上拉输入，按下为低 */
	gpio.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
	gpio.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOE, &gpio);
}

static void beep(u16 ms)
{
	GPIO_SetBits(GPIOB, GPIO_Pin_8);
	delay_ms(ms);
	GPIO_ResetBits(GPIOB, GPIO_Pin_8);
}

static void check_keys(void)
{
	static u8 last = 0;
	u8 now = 0;

	if (PAin(0)) now |= 0x01;
	if (!PEin(2)) now |= 0x02;
	if (!PEin(3)) now |= 0x04;
	if (!PEin(4)) now |= 0x08;

	if ((now & ~last) & 0x01) printf("[KEY] KEY_UP (PA0) pressed\r\n");
	if ((now & ~last) & 0x02) printf("[KEY] K1 (PE2) pressed\r\n");
	if ((now & ~last) & 0x04) printf("[KEY] K2 (PE3) pressed\r\n");
	if ((now & ~last) & 0x08) printf("[KEY] K3 (PE4) pressed\r\n");
	if (now & ~last) beep(30);
	last = now;
}

int main(void)
{
	u8 led = 0;
	u16 tick = 0;
	u32 loops = 0;

	SysTick_Init(72);
	board_gpio_init();
	usart1_init(115200);
	usart3_init(115200);

	printf("\r\n=== LingXi board test v3 ===\r\n");
	printf("BEEP=PB8  DS0=PB5  DS1=PE5  KEY_UP=PA0  K1..K3=PE2..PE4\r\n");
	beep(100); delay_ms(100); beep(100);

	while (1)
	{
		/* DS0/DS1 每 500ms 交替闪烁 */
		if (tick % 50 == 0)
		{
			PBout(5) = led;
			PEout(5) = !led;
			led = !led;
		}
		if (tick % 100 == 0)
		{
			static u8 last_ok = 0xFF;
			u8 ok = usart3_loopback((u8)loops);
			printf("[ALIVE] loop=%lu  USART3 loopback: %s\r\n",
			       (unsigned long)loops++, ok ? "OK" : "FAIL");
			if (ok && last_ok != 1) beep(300);	/* 首次连通长响一声 */
			last_ok = ok;
		}

		check_keys();
		delay_ms(10);
		tick = (tick + 1) % 1000;
	}
}
