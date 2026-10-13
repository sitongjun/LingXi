#ifndef _BSP_H
#define _BSP_H

#include "system.h"

/* 板载资源（实测确认，见 docs/接口设计.md） */
#define DS0          PBout(5)     /* 低电平亮 */
#define DS1          PEout(5)     /* 低电平亮 */
#define BEEP         PBout(8)     /* 高电平响 */

#define KEY_UP_IN    PAin(0)      /* 按下为 1 */
#define KEY1_IN      PEin(2)      /* 按下为 0 */
#define KEY2_IN      PEin(3)
#define KEY3_IN      PEin(4)

enum { KEY_NONE = 0, KEY_UP = 1, KEY_1 = 2, KEY_2 = 4, KEY_3 = 8 };

void bsp_init(void);
u32  millis(void);
u8   bsp_key_scan(void);          /* 返回本次新按下的按键位图，需每 10ms 左右调用 */

#endif
