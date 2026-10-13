#ifndef _led_H
#define _led_H

#include "system.h"

/*  普中STM32F103ZET6开发板 用户LED：D1 -> PB5，D2 -> PE5（低电平点亮） */
#define LED1_PORT 			GPIOB
#define LED1_PIN 			GPIO_Pin_5
#define LED1_PORT_RCC		RCC_APB2Periph_GPIOB

#define LED2_PORT 			GPIOE
#define LED2_PIN 			GPIO_Pin_5
#define LED2_PORT_RCC		RCC_APB2Periph_GPIOE

#define LED1 PBout(5)
#define LED2 PEout(5)


void LED_Init(void);


#endif
