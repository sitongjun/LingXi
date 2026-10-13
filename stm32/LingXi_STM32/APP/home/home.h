#ifndef _HOME_H
#define _HOME_H

#include "link.h"

/* 设备类型：switch 取值 0/1；level 取值 min..max */
enum { DEV_SWITCH = 0, DEV_LEVEL = 1 };

/* SET 应答状态码 */
enum { ST_OK = 0, ST_NO_DEV = 1, ST_RANGE = 2, ST_BAD_LEN = 3 };

void home_init(void);
void home_handle_frame(const link_frame_t *f);
void home_tick(void);                          /* 主循环中持续调用 */

#endif
