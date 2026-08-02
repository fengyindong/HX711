#include "keys.h"

/* 按键模块：将GPIO电平转换为与硬件无关的短按/长按事件。 */
#define KEY_PORT            GPIOB
#define KEY_FUNC_PIN        GPIO_Pin_0
#define KEY_PLUS_PIN        GPIO_Pin_1
#define KEY_MINUS_PIN       GPIO_Pin_10
#define KEY_OK_PIN          GPIO_Pin_11
#define KEY_DEBOUNCE_MS     30U                   //30ms去抖
#define KEY_LONG_MS         1200U

typedef struct {
    uint16_t pin;
    uint8_t stable_down;
    uint8_t last_raw;
    uint8_t long_sent;
    uint32_t changed_at;
    uint32_t pressed_at;
} Key_State;

/* 每个按键保存原始状态、消抖后状态以及长按计时信息。 */
static Key_State g_keys[4];
/* 事件只有一个槽位；主循环读取后清空，避免一个按键被重复执行。 */
static volatile Key_Event g_event;

void Keys_Init(void)
{
    GPIO_InitTypeDef gpio;
    uint8_t i;
    const uint16_t pins[4] = {
        KEY_FUNC_PIN, KEY_PLUS_PIN, KEY_MINUS_PIN, KEY_OK_PIN
    };

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = KEY_FUNC_PIN | KEY_PLUS_PIN | KEY_MINUS_PIN | KEY_OK_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(KEY_PORT, &gpio);

    for (i = 0U; i < 4U; ++i) {
        g_keys[i].pin = pins[i];
        g_keys[i].last_raw = 0U;
        g_keys[i].stable_down = 0U;
        g_keys[i].long_sent = 0U;
        g_keys[i].changed_at = 0U;
        g_keys[i].pressed_at = 0U;
    }
    g_event = KEY_NONE;
}

void Keys_Update(uint32_t now_ms)
{
    uint8_t i;

    for (i = 0U; i < 4U; ++i) {
        uint8_t raw = (GPIO_ReadInputDataBit(KEY_PORT, g_keys[i].pin) == Bit_RESET);

        /* 原始电平发生变化时重新开始30ms消抖计时。 */
        if (raw != g_keys[i].last_raw) {
            g_keys[i].last_raw = raw;
            g_keys[i].changed_at = now_ms;
        }
        /* 电平连续保持30ms后，才接受为一次真实按下或释放。 */
        if ((raw != g_keys[i].stable_down) &&
            ((uint32_t)(now_ms - g_keys[i].changed_at) >= KEY_DEBOUNCE_MS)) {
            g_keys[i].stable_down = raw;
            if (raw != 0U) {
                g_keys[i].pressed_at = now_ms;
                g_keys[i].long_sent = 0U;
            /* 功能键在释放时产生短按；长按已经触发则不再产生短按。 */
            } else if ((i == 0U) && (g_keys[i].long_sent == 0U) &&
                       (g_event == KEY_NONE)) {
                g_event = KEY_FUNC_SHORT;
            } else if ((i != 0U) && (g_event == KEY_NONE)) {
                g_event = (i == 1U) ? KEY_PLUS :
                          ((i == 2U) ? KEY_MINUS : KEY_OK);
            }
        }
        /* 功能键按住1.2秒立即上报长按，不必等到释放。 */
        if ((i == 0U) && (g_keys[i].stable_down != 0U) &&
            (g_keys[i].long_sent == 0U) &&
            ((uint32_t)(now_ms - g_keys[i].pressed_at) >= KEY_LONG_MS) &&
            (g_event == KEY_NONE)) {
            g_keys[i].long_sent = 1U;
            g_event = KEY_FUNC_LONG;
        }
    }
}

Key_Event Keys_GetEvent(void)
{
    Key_Event event = g_event;
    g_event = KEY_NONE;
    return event;
}
