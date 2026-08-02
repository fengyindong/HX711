#include "alarm.h"

/* 超重报警模块：用SysTick节拍同时控制有源蜂鸣器和红色LED。 */
#define ALARM_PORT          GPIOB
#define ALARM_LED_PIN       GPIO_Pin_8
#define ALARM_BUZZER_PIN    GPIO_Pin_9
#define ALARM_TOGGLE_MS     250U

static volatile uint8_t g_enabled;
static volatile uint8_t g_output;
static volatile uint16_t g_elapsed;

/* 集中控制两个报警输出，便于以后修改为低电平有效。 */
static void Alarm_Write(uint8_t on)
{
    if (on != 0U) {
        GPIO_SetBits(ALARM_PORT, ALARM_LED_PIN | ALARM_BUZZER_PIN);
    } else {
        GPIO_ResetBits(ALARM_PORT, ALARM_LED_PIN | ALARM_BUZZER_PIN);
    }
}

void Alarm_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = ALARM_LED_PIN | ALARM_BUZZER_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(ALARM_PORT, &gpio);
    g_enabled = 0U;
    g_output = 0U;
    g_elapsed = 0U;
    Alarm_Write(0U);
}

void Alarm_Set(uint8_t enabled)
{
    if (enabled != g_enabled) {
        g_enabled = enabled;
        g_output = 0U;
        g_elapsed = 0U;
        Alarm_Write(0U);
    }
}

void Alarm_Tick1ms(void)
{
    if (g_enabled == 0U) {
        return;
    }
    ++g_elapsed;
    /* 每250ms翻转一次，形成间歇蜂鸣并同步闪烁LED。 */
    if (g_elapsed >= ALARM_TOGGLE_MS) {
        g_elapsed = 0U;
        g_output = !g_output;
        Alarm_Write(g_output);
    }
}
