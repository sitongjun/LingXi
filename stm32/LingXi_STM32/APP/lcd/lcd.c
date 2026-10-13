#include "lcd.h"
#include "SysTick.h"

/* 背光 PB0，高电平亮 */
#define LCD_BL  PBout(0)

static void wr_cmd(u16 c)  { LCD->REG = c; }
static void wr_data(u16 d) { LCD->RAM = d; }
static u16  rd_data(void)  { return LCD->RAM; }

static void wr_seq(u8 cmd, const u8 *d, u8 n)
{
	wr_cmd(cmd);
	while (n--) wr_data(*d++);
}

static void fsmc_init(void)
{
	GPIO_InitTypeDef gpio;
	FSMC_NORSRAMInitTypeDef fsmc;
	FSMC_NORSRAMTimingInitTypeDef rd, wr;

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOD |
	                       RCC_APB2Periph_GPIOE | RCC_APB2Periph_GPIOG, ENABLE);

	gpio.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;

	/* PD0,1,8,9,10,14,15 = D2,D3,D13,D14,D15,D0,D1；PD4 = NOE；PD5 = NWE */
	gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_8 |
	                GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOD, &gpio);

	/* PE7~PE15 = D4~D12 */
	gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 |
	                GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOE, &gpio);

	/* PG0 = A10(RS)，PG12 = NE4(CS) */
	gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_12;
	GPIO_Init(GPIOG, &gpio);

	/* 背光 */
	gpio.GPIO_Pin = GPIO_Pin_0;
	gpio.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOB, &gpio);
	LCD_BL = 0;

	/* 时序参考普中官方驱动：读慢、写也偏慢以保证本屏稳定 */
	rd.FSMC_AddressSetupTime = 0x01;
	rd.FSMC_AddressHoldTime = 0x00;
	rd.FSMC_DataSetupTime = 0x0F;
	rd.FSMC_BusTurnAroundDuration = 0x00;
	rd.FSMC_CLKDivision = 0x00;
	rd.FSMC_DataLatency = 0x00;
	rd.FSMC_AccessMode = FSMC_AccessMode_A;

	wr.FSMC_AddressSetupTime = 0x15;
	wr.FSMC_AddressHoldTime = 0x15;
	wr.FSMC_DataSetupTime = 0x05;
	wr.FSMC_BusTurnAroundDuration = 0x00;
	wr.FSMC_CLKDivision = 0x00;
	wr.FSMC_DataLatency = 0x00;
	wr.FSMC_AccessMode = FSMC_AccessMode_A;

	fsmc.FSMC_Bank = FSMC_Bank1_NORSRAM4;
	fsmc.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
	fsmc.FSMC_MemoryType = FSMC_MemoryType_SRAM;
	fsmc.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
	fsmc.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
	fsmc.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	fsmc.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;
	fsmc.FSMC_WrapMode = FSMC_WrapMode_Disable;
	fsmc.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
	fsmc.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
	fsmc.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
	fsmc.FSMC_ExtendedMode = FSMC_ExtendedMode_Enable;
	fsmc.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
	fsmc.FSMC_ReadWriteTimingStruct = &rd;
	fsmc.FSMC_WriteTimingStruct = &wr;
	FSMC_NORSRAMInit(&fsmc);
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4, ENABLE);
}

static u16 read_id(void)
{
	u16 id;
	wr_cmd(0xD3);
	(void)rd_data();               /* dummy */
	(void)rd_data();               /* 0x00 */
	id = (u16)((rd_data() & 0xFF) << 8);
	id |= rd_data() & 0xFF;
	return id;
}

/* ILI9488 初始化序列（来自普中官方驱动）。该屏每个像素分两次写入：高 8 位、低 8 位 */
static const u8 s_e0[] = { 0x00, 0x13, 0x18, 0x04, 0x0F, 0x06, 0x3A, 0x56, 0x4D, 0x03,
                           0x0A, 0x06, 0x30, 0x3E, 0x0F };
static const u8 s_e1[] = { 0x00, 0x13, 0x18, 0x01, 0x11, 0x06, 0x38, 0x34, 0x4D, 0x06,
                           0x0D, 0x0B, 0x31, 0x37, 0x0F };
static const u8 s_c0[] = { 0x18, 0x17 };
static const u8 s_c5[] = { 0x00, 0x1A, 0x80 };
static const u8 s_b6[] = { 0x00, 0x22, 0x3B };
static const u8 s_f7[] = { 0xA9, 0x51, 0x2C, 0x82 };

u16 lcd_init(void)
{
	u16 id;
	u8 v;

	fsmc_init();
	delay_ms(50);
	id = read_id();

	wr_seq(0xE0, s_e0, sizeof(s_e0));   /* 正伽马 */
	wr_seq(0xE1, s_e1, sizeof(s_e1));   /* 负伽马 */
	wr_seq(0xC0, s_c0, sizeof(s_c0));
	v = 0x41; wr_seq(0xC1, &v, 1);
	wr_seq(0xC5, s_c5, sizeof(s_c5));
	v = 0x08; wr_seq(0x36, &v, 1);      /* 竖屏 320x480，BGR */
	v = 0x55; wr_seq(0x3A, &v, 1);      /* 16 位像素 */
	v = 0x00; wr_seq(0xB0, &v, 1);
	v = 0xA0; wr_seq(0xB1, &v, 1);      /* 60Hz */
	v = 0x02; wr_seq(0xB4, &v, 1);
	wr_seq(0xB6, s_b6, sizeof(s_b6));
	v = 0x00; wr_seq(0xE9, &v, 1);
	wr_seq(0xF7, s_f7, sizeof(s_f7));
	wr_cmd(0x11);                       /* 退出睡眠 */
	delay_ms(120);
	wr_cmd(0x29);                       /* 开显示 */

	lcd_clear(C_BLACK);
	LCD_BL = 1;
	return id;
}

void lcd_set_window(u16 x0, u16 y0, u16 x1, u16 y1)
{
	wr_cmd(0x2A);
	wr_data(x0 >> 8); wr_data(x0 & 0xFF); wr_data(x1 >> 8); wr_data(x1 & 0xFF);
	wr_cmd(0x2B);
	wr_data(y0 >> 8); wr_data(y0 & 0xFF); wr_data(y1 >> 8); wr_data(y1 & 0xFF);
	wr_cmd(0x2C);
}

void lcd_fill(u16 x, u16 y, u16 w, u16 h, u16 color)
{
	u32 n = (u32)w * h;
	if (!w || !h) return;
	lcd_set_window(x, y, x + w - 1, y + h - 1);
	while (n--)
	{
		LCD->RAM = color >> 8;
		LCD->RAM = color & 0xFF;
	}
}

void lcd_clear(u16 color)
{
	lcd_fill(0, 0, LCD_W, LCD_H, color);
}

void lcd_pixel(u16 x, u16 y, u16 color)
{
	lcd_set_window(x, y, x, y);
	lcd_write_px(color);
}
