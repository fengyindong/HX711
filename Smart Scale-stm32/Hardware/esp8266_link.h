#ifndef __ESP8266_LINK_H
#define __ESP8266_LINK_H

#include "stm32f10x.h"

/* 初始化USART2：PA2=TX、PA3=RX，用于连接ESP8266。 */
void ESP8266_LinkInit(uint32_t baudrate);
/* 发送一行JSON状态；重量使用x10整数传输以避免接收端浮点解析。 */
void ESP8266_SendWeight(float weight_g, const char *unit,
                        float display_value, uint8_t alarm,
                        uint8_t stable);

#endif
