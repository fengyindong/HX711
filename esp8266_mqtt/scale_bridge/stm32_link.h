#ifndef STM32_LINK_H
#define STM32_LINK_H

#include <Arduino.h>

class Stm32Link {
public:
    /* 初始化ESP8266硬件串口，并扩大底层接收缓冲区。 */
    void begin(unsigned long baudrate);

    /* 仅在收到一条完整的“{...}”JSON帧时返回true。 */
    bool readLine(String &line);

private:
    /* 保存当前正在接收的JSON；帧损坏后等待下一个“{”重新同步。 */
    String buffer_;
    bool inFrame_;
};

#endif
