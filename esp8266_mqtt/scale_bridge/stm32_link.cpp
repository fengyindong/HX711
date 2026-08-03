#include "stm32_link.h"

/* STM32串口接收模块：把连续字节流恢复为完整JSON消息。 */
void Stm32Link::begin(unsigned long baudrate)
{
    /*
     * 这里扩大的是ESP8266硬件串口RX环形缓冲区。
     * String::reserve()只影响应用层字符串，不能防止串口字节被覆盖。
     * 在Serial.begin()前设置，以兼容ESP8266 Arduino Core的初始化要求。
     */
    Serial.setRxBufferSize(512U);
    Serial.begin(baudrate);
    buffer_.reserve(256U);
    buffer_ = "";
    inFrame_ = false;
}

bool Stm32Link::readLine(String &line)
{
    /*
     * 严格寻找“{”和“}”，不再把任意换行前的数据视为有效消息。
     * 若前半帧已经丢失，残余尾部会被忽略，直到下一帧“{”出现。
     */
    while (Serial.available() > 0) {
        char ch = (char)Serial.read();

        if (ch == '{') {
            /* 左花括号也是重同步点，丢弃此前可能损坏的半帧。 */
            buffer_ = "";
            buffer_ += ch;
            inFrame_ = true;
            continue;
        }

        if (!inFrame_) continue;

        /* 当前状态帧小于255字节；超长说明数据异常，直接丢弃。 */
        if (buffer_.length() >= 255U) {
            buffer_ = "";
            inFrame_ = false;
            continue;
        }

        if ((ch != '\r') && (ch != '\n')) buffer_ += ch;

        if (ch == '}') {
            line = buffer_;
            buffer_ = "";
            inFrame_ = false;
            return true;
        }
    }

    return false;
}
