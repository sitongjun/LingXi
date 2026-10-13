#include "dht11.h"
#include "bsp.h"
#include "SysTick.h"

#define DHT_OUT  PGout(11)
#define DHT_IN   PGin(11)

void dht11_init(void)
{
	GPIO_InitTypeDef gpio;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, ENABLE);
	gpio.GPIO_Pin = GPIO_Pin_11;
	gpio.GPIO_Mode = GPIO_Mode_Out_OD;      /* 开漏：输出 1 时释放总线，仍可读输入 */
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOG, &gpio);
	DHT_OUT = 1;
}

/* 等待总线变为 level，超时返回 0，否则返回等待的微秒数（至少 1） */
static u32 wait_level(u8 level, u32 timeout_us)
{
	u32 t0 = micros();
	u32 dt;
	while (DHT_IN != level)
	{
		dt = micros() - t0;
		if (dt > timeout_us) return 0;
	}
	dt = micros() - t0;
	return dt ? dt : 1;
}

u8 dht11_read(s16 *temp_x10, u8 *humi)
{
	u8 buf[5] = { 0 };
	u8 i;

	/* 起始信号：拉低 ≥18ms 后释放 */
	DHT_OUT = 0;
	delay_ms(20);
	DHT_OUT = 1;

	/* 响应：约 20~40us 后传感器拉低 80us，再拉高 80us */
	if (!wait_level(0, 100)) return 0;
	if (!wait_level(1, 120)) return 0;
	if (!wait_level(0, 120)) return 0;

	/* 40 位数据：每位先低 50us，再高 26~28us 表示 0、70us 表示 1 */
	for (i = 0; i < 40; i++)
	{
		u32 t_high;
		if (!wait_level(1, 80)) return 0;
		t_high = wait_level(0, 120);
		if (!t_high) return 0;
		buf[i / 8] <<= 1;
		if (t_high > 45) buf[i / 8] |= 1;
	}

	if ((u8)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4]) return 0;

	*humi = buf[0];
	*temp_x10 = (s16)(buf[2] * 10 + (buf[3] & 0x7F) % 10);
	if (buf[3] & 0x80) *temp_x10 = (s16)-*temp_x10;    /* 部分新版 DHT11 支持零下 */
	return 1;
}
