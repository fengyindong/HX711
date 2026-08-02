#include "bsp_usart.h"

#include <stdarg.h>
#include <stdio.h>

/* 调试串口模块：USART1使用轮询方式收发，不占用额外中断资源。 */
void BSP_USART1_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1,
                          ENABLE);

    /* PA9复用推挽输出为TX，PA10浮空输入为RX。 */
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    USART_StructInit(&usart);
    usart.USART_BaudRate = baudrate;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &usart);
    USART_Cmd(USART1, ENABLE);
}

void BSP_USART1_WriteString(const char *text)
{
    while (*text != '\0') {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
        }
        USART_SendData(USART1, (uint16_t)(uint8_t)*text++);
    }
}

void BSP_USART1_Printf(const char *format, ...)
{
    /* 固定缓冲区避免动态内存；超长字符串会被vsnprintf安全截断。 */
    char buffer[128];
    va_list args;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    BSP_USART1_WriteString(buffer);
}

int BSP_USART1_GetChar(char *ch)
{
    if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET) {
        return 0;
    }

    *ch = (char)(USART_ReceiveData(USART1) & 0xFFU);
    return 1;
}
