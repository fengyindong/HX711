#ifndef __KEYS_H
#define __KEYS_H

#include "stm32f10x.h"

typedef enum {
    KEY_NONE = 0,
    KEY_FUNC_SHORT,
    KEY_FUNC_LONG,
    KEY_PLUS,
    KEY_MINUS,
    KEY_OK
} Key_Event;

/* 四个按键均使用内部上拉，按下时GPIO接地。 */
void Keys_Init(void);
/* 每1ms调用一次，完成消抖、短按和长按识别。 */
void Keys_Update(uint32_t now_ms);
/* 取走一个待处理事件；没有事件时返回KEY_NONE。 */
Key_Event Keys_GetEvent(void);

#endif
