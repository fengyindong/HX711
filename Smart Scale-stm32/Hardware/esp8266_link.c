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
                        float display_value, uint8_t alarm,
                        uint8_t stable)
{
    char text[128];
    long grams = ToTenths(weight_g);
    long shown = ToTenths(display_value);

    /* 每条消息以CRLF结束，ESP8266按行接收，便于处理粘包和拆包。 */
    snprintf(text, sizeof(text),
             "{\"weight_g_x10\":%ld,\"value_x10\":%ld,\"unit\":\"%s\","
             "\"alarm\":%u,\"stable\":%u}\r\n",
             grams, shown, unit, (unsigned)alarm, (unsigned)stable);
    ESP8266_Write(text);
}
