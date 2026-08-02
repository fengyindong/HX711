#ifndef __ALARM_H
#define __ALARM_H

#include "stm32f10x.h"

/* PB8驱动红色LED，PB9驱动有源蜂鸣器，均为高电平有效。 */
void Alarm_Init(void);
/* 开启或关闭超重报警；关闭时会立即熄灯并停止蜂鸣。 */
void Alarm_Set(uint8_t enabled);
/* 由1ms SysTick调用，产生250ms间隔的“滴滴”声和LED闪烁。 */
void Alarm_Tick1ms(void);

#endif
