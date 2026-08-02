#ifndef __BSP_TIME_H
#define __BSP_TIME_H

#include "stm32f10x.h"

/* 使用SysTick建立1ms系统时基。 */
void BSP_Time_Init(void);
/* 返回从系统启动开始累计的毫秒数，溢出处理由无符号减法保证。 */
uint32_t BSP_Millis(void);
/* DWT硬件计数延时，不依赖SysTick中断，适合启动和HX711时序。 */
void BSP_DelayUs(uint32_t us);
/* DWT硬件计数毫秒延时，即使SysTick未进入也不会永久卡死。 */
void BSP_DelayMs(uint32_t ms);
/* 检查SysTick毫秒计数是否真正运行，成功返回1。 */
uint8_t BSP_TimebaseCheck(void);

#endif
