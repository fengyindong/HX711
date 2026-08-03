#include "esp8266_link.h"

#include <stdio.h>

/* STM32通信模块：只负责USART2数据封装，Wi-Fi和MQTT由ESP8266处理。 */
static void ESP8266_Write(const char *text)
{
    while (*text != '\0') {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) {
        }
        USART_SendData(USART2, (uint16_t)(uint8_t)*text++);
    }
}

static long ToTenths(float value)
{
    /* 先四舍五入为0.1单位，再转整数，避免JSON中传输浮点。 */
    float scaled = value * 10.0f;
    return (long)((scaled >= 0.0f) ? scaled + 0.5f : scaled - 0.5f);
}

typedef struct {
    const char *sign;
    unsigned long whole;
    unsigned long fraction;
} Fixed1;

/* 把x10整数拆成符号、整数和一位小数，供JSON数字直接可读显示。 */
static Fixed1 SplitTenths(long tenths)
{
    Fixed1 value;
    unsigned long magnitude;

    value.sign = (tenths < 0L) ? "-" : "";
    magnitude = (tenths < 0L) ? (unsigned long)(-tenths) :
                                (unsigned long)tenths;
    value.whole = magnitude / 10UL;
    value.fraction = magnitude % 10UL;
    return value;
}

void ESP8266_LinkInit(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_2;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_3;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    USART_StructInit(&usart);
    usart.USART_BaudRate = baudrate;
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &usart);
    USART_Cmd(USART2, ENABLE);
}

void ESP8266_SendWeight(float weight_g, const char *unit,
                        float display_value, float alarm_limit_g,
                        float capacity_g, uint8_t alarm,
                        uint8_t over_limit, uint8_t over_range,
                        uint8_t stable)
{
    char text[256];
    Fixed1 grams = SplitTenths(ToTenths(weight_g));
    Fixed1 shown = SplitTenths(ToTenths(display_value));
    Fixed1 alarm_limit = SplitTenths(ToTenths(alarm_limit_g));
    Fixed1 capacity = SplitTenths(ToTenths(capacity_g));
    const char *state;

    if (over_range != 0U) state = "over_range";
    else if (over_limit != 0U) state = "over_limit";
    else if (stable == 0U) state = "moving";
    else state = "stable";

    /*
     * JSON直接输出一位小数，订阅端无需再除以10。数值先拆成整数部分
     * 和小数部分，因此不需要开启STM32 printf的浮点格式支持。
     */
    snprintf(text, sizeof(text),
             "{\"device\":\"hx711-scale\",\"weight_g\":%s%lu.%lu,"
             "\"display_value\":%s%lu.%lu,\"unit\":\"%s\","
             "\"alarm_limit_g\":%s%lu.%lu,\"capacity_g\":%s%lu.%lu,"
             "\"stable\":%u,\"alarm\":%u,\"over_limit\":%u,"
             "\"over_range\":%u,\"state\":\"%s\"}\r\n",
             grams.sign, grams.whole, grams.fraction,
             shown.sign, shown.whole, shown.fraction, unit,
             alarm_limit.sign, alarm_limit.whole, alarm_limit.fraction,
             capacity.sign, capacity.whole, capacity.fraction,
             (unsigned)stable, (unsigned)alarm, (unsigned)over_limit,
             (unsigned)over_range, state);
    ESP8266_Write(text);
}
