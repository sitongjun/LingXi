#include "link.h"
#include "bsp.h"

#define RX_BUF_SIZE  2048
#define FRAME_TIMEOUT_MS  50          /* 帧内字节间隔超时，超时则丢弃半帧 */

/* 定义 LINK_ON_USART1 时改走 USART1(CH340)，用于在电脑上用脚本模拟 ESP32 测试协议 */
#ifdef LINK_ON_USART1
#define LINK_USART      USART1
#define LINK_IRQn       USART1_IRQn
#define LINK_IRQHandler USART1_IRQHandler
#else
#define LINK_USART      USART3
#define LINK_IRQn       USART3_IRQn
#define LINK_IRQHandler USART3_IRQHandler
#endif

static volatile u8  rx_buf[RX_BUF_SIZE];
static volatile u16 rx_head = 0, rx_tail = 0;
static u32 crc_errors = 0;

void link_init(u32 baud)
{
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef us;
	NVIC_InitTypeDef nvic;

#ifndef LINK_ON_USART1
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
	gpio.GPIO_Pin = GPIO_Pin_10;
	gpio.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio);
	gpio.GPIO_Pin = GPIO_Pin_11;
	gpio.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOB, &gpio);
#else
	(void)gpio;   /* USART1 引脚已在 bsp 中初始化 */
#endif

	USART_StructInit(&us);
	us.USART_BaudRate = baud;
	us.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(LINK_USART, &us);
	USART_ITConfig(LINK_USART, USART_IT_RXNE, ENABLE);

	nvic.NVIC_IRQChannel = LINK_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 1;
	nvic.NVIC_IRQChannelSubPriority = 0;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);
	USART_Cmd(LINK_USART, ENABLE);
}

void LINK_IRQHandler(void)
{
	/* 读 SR 再读 DR 可同时清除 RXNE 与 ORE */
	if (LINK_USART->SR & (USART_FLAG_RXNE | USART_FLAG_ORE))
	{
		u8 c = (u8)LINK_USART->DR;
		u16 next = (u16)((rx_head + 1) % RX_BUF_SIZE);
		if (next != rx_tail)
		{
			rx_buf[rx_head] = c;
			rx_head = next;
		}
	}
}

u16 link_crc16(u16 crc, const u8 *d, u16 n)
{
	u8 i;
	while (n--)
	{
		crc ^= (u16)(*d++) << 8;
		for (i = 0; i < 8; i++)
			crc = (crc & 0x8000) ? (u16)((crc << 1) ^ 0x1021) : (u16)(crc << 1);
	}
	return crc;
}

u32 link_crc_errors(void)
{
	return crc_errors;
}

static void uart_put(u8 c)
{
	while (USART_GetFlagStatus(LINK_USART, USART_FLAG_TXE) == RESET);
	USART_SendData(LINK_USART, c);
}

void link_send(u8 cmd, u8 seq, const u8 *data, u16 len)
{
	u8 hdr[4];
	u16 crc, i;

	hdr[0] = (u8)(len & 0xFF);
	hdr[1] = (u8)(len >> 8);
	hdr[2] = cmd;
	hdr[3] = seq;
	crc = link_crc16(0xFFFF, hdr, 4);
	crc = link_crc16(crc, data, len);

	uart_put(0xAA);
	uart_put(0x55);
	for (i = 0; i < 4; i++) uart_put(hdr[i]);
	for (i = 0; i < len; i++) uart_put(data[i]);
	uart_put((u8)(crc & 0xFF));
	uart_put((u8)(crc >> 8));
}

/* 帧解析状态机 */
enum { S_H0, S_H1, S_LEN0, S_LEN1, S_CMD, S_SEQ, S_DATA, S_CRC0, S_CRC1 };

u8 link_poll(link_frame_t *f)
{
	static u8 state = S_H0;
	static u16 idx = 0, crc_rx = 0;
	static u32 last_ms = 0;
	u8 c, hdr[4];

	if (state != S_H0 && millis() - last_ms > FRAME_TIMEOUT_MS)
		state = S_H0;

	while (rx_tail != rx_head)
	{
		c = rx_buf[rx_tail];
		rx_tail = (u16)((rx_tail + 1) % RX_BUF_SIZE);
		last_ms = millis();

		switch (state)
		{
		case S_H0:   if (c == 0xAA) state = S_H1; break;
		case S_H1:   state = (c == 0x55) ? S_LEN0 : (c == 0xAA ? S_H1 : S_H0); break;
		case S_LEN0: f->len = c; state = S_LEN1; break;
		case S_LEN1:
			f->len |= (u16)c << 8;
			state = (f->len > LINK_MAX_PAYLOAD) ? S_H0 : S_CMD;
			break;
		case S_CMD:  f->cmd = c; state = S_SEQ; break;
		case S_SEQ:
			f->seq = c; idx = 0;
			state = f->len ? S_DATA : S_CRC0;
			break;
		case S_DATA:
			f->data[idx++] = c;
			if (idx >= f->len) state = S_CRC0;
			break;
		case S_CRC0: crc_rx = c; state = S_CRC1; break;
		case S_CRC1:
			crc_rx |= (u16)c << 8;
			state = S_H0;
			hdr[0] = (u8)(f->len & 0xFF); hdr[1] = (u8)(f->len >> 8);
			hdr[2] = f->cmd; hdr[3] = f->seq;
			if (link_crc16(link_crc16(0xFFFF, hdr, 4), f->data, f->len) == crc_rx)
				return 1;
			crc_errors++;
			break;
		}
	}
	return 0;
}
