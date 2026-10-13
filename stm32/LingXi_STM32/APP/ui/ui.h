#ifndef _UI_H
#define _UI_H

#include "system.h"

/* 3.5 寸彩屏居家状态面板（竖屏 320x480） */
void ui_init(void);     /* 绘制静态框架 */
void ui_tick(void);     /* 主循环中调用，只重绘发生变化的部分 */

#endif
