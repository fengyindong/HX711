#ifndef __HX711_H
#define __HX711_H

#include "stm32f10x.h"

/* HX711默认接线；更换引脚时只需修改以下宏。 */
#define HX711_GPIO_PORT             GPIOB
#define HX711_GPIO_CLOCK            RCC_APB2Periph_GPIOB
#define HX711_DOUT_PIN              GPIO_Pin_12
#define HX711_SCK_PIN               GPIO_Pin_13

#define HX711_READ_TIMEOUT_MS       1500U

typedef enum {
    HX711_OK = 0,          /* 数据读取成功 */
    HX711_TIMEOUT,         /* DOUT在限定时间内没有变低 */
    HX711_INVALID_DATA,    /* 参数错误、ADC饱和或接线异常 */
    HX711_NOT_STABLE       /* 数据有效，但称重物仍在晃动 */
} HX711_Status;

/* 配置DOUT浮空输入和PD_SCK推挽输出。 */
void HX711_Init(void);
/* PD_SCK保持高电平超过60us，使HX711进入低功耗状态。 */
void HX711_PowerDown(void);
/* 拉低PD_SCK唤醒HX711；唤醒后应等待4个转换周期。 */
void HX711_PowerUp(void);
/* 读取通道A、增益128的一次24位有符号原始数据。 */
HX711_Status HX711_ReadRaw(int32_t *value, uint32_t timeout_ms);
/* 直接读取DOUT引脚当前电平：1=高电平等待转换，0=数据就绪。 */
uint8_t HX711_GetDoutLevel(void);

#endif