#ifndef STM32_LINK_H
#define STM32_LINK_H

#include <Arduino.h>

class Stm32Link {
public:
    /* 初始化ESP8266硬件串口并预留接收缓冲区。 */
    void begin(unsigned long baudrate);
    /* 收到完整换行帧时返回true，并通过line给出一行JSON。 */
    bool readLine(String &line);

private:
    /* 保存尚未收到换行符的半包数据。 */
    String buffer_;
};

#endif
