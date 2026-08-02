#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "stm32f10x.h"

/* 初始化USART1：PA9=TX、PA10=RX，默认用于调试信息。 */
void BSP_USART1_Init(uint32_t baudrate);
/* 阻塞发送以\0结束的字符串。 */
void BSP_USART1_WriteString(const char *text);
/* 小型格式化输出；内部缓冲区为128字节。 */
void BSP_USART1_Printf(const char *format, ...);
/* 非阻塞读取一个字符：收到返回1，否则返回0。 */
int BSP_USART1_GetChar(char *ch);

#endif
