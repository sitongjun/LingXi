#ifndef _HOME_H
#define _HOME_H

#include "link.h"

/* 设备类型：switch 取值 0/1；level 取值 min..max */
enum { DEV_SWITCH = 0, DEV_LEVEL = 1 };

/* SET 应答状态码 */
enum { ST_OK = 0, ST_NO_DEV = 1, ST_RANGE = 2, ST_BAD_LEN = 3 };

/* 事件来源 */
enum { SRC_VOICE = 0, SRC_KEY = 1, SRC_AUTO = 2 };

/* 报警原因（位图） */
enum { OVER_TEMP = 1, OVER_HUMI = 2 };

typedef struct {
	u8  sensor_ok;     /* DHT11 是否在线 */
	s16 temp_x10;      /* 温度 ×10 */
	u8  humi;          /* 湿度 % */
	u8  over;          /* 当前超限原因（OVER_TEMP | OVER_HUMI） */
	u8  silenced;      /* 超限期间用户已手动关闭报警 */
	u32 updated_ms;    /* 最近一次成功采样时间 */
} home_env_t;

typedef struct {
	u32         ms;     /* 发生时间（millis），0 表示尚无事件 */
	u8          src;
	const char *label;  /* 设备中文名 */
	s16         value;
	u16         seq;    /* 每次新事件递增，界面据此判断是否需要刷新 */
} home_event_t;

void home_init(void);
void home_handle_frame(const link_frame_t *f);
void home_tick(void);                          /* 主循环中持续调用 */

/* 供界面读取 */
const home_env_t   *home_env(void);
const home_event_t *home_last_event(void);
s16  home_value(const char *name);
u8   home_link_online(void);                   /* 10 秒内收到过 ESP32 的帧 */

#endif
