#ifndef __ESP8266_LINK_H
#define __ESP8266_LINK_H

#include "stm32f10x.h"

/* 初始化USART2：PA2=TX、PA3=RX，用于连接ESP8266。 */
void ESP8266_LinkInit(uint32_t baudrate);
/* 发送一行带一位小数的可读JSON状态，不依赖printf浮点支持。 */
void ESP8266_SendWeight(float weight_g, const char *unit,
                        float display_value, float alarm_limit_g,
                        float capacity_g, uint8_t alarm,
                        uint8_t over_limit, uint8_t over_range,
                        uint8_t stable);

#endif