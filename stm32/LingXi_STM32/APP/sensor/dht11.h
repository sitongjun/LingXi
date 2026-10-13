#ifndef _DHT11_H
#define _DHT11_H

#include "system.h"

/* DHT11 温湿度传感器，接板载 DS18B20/DHT11 插座（PG11，单总线） */

void dht11_init(void);

/* 读取一次（耗时约 25ms，两次读取需间隔 1s 以上）
 * 成功返回 1：temp_x10 为温度×10，humi 为相对湿度 %；未接或校验失败返回 0 */
u8 dht11_read(s16 *temp_x10, u8 *humi);

#endif
