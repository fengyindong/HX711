#include "stm32_link.h"

/* STM32串口接收模块：把连续字节流恢复为一行一行的JSON消息。 */
void Stm32Link::begin(unsigned long baudrate)
{
    Serial.begin(baudrate);
    buffer_.reserve(160);
}

bool Stm32Link::readLine(String &line)
{
    /*
     * STM32以CRLF结束每条JSON。这里忽略CR，以LF作为帧边界，
     * 因而可以正确处理一次收到多帧或一帧被分多次收到的情况。
     */
    while (Serial.available() > 0) {
        char ch = (char)Serial.read();
        if (ch == '\n') {
            buffer_.trim();
            if (buffer_.length() > 0U) {
                line = buffer_;
                buffer_ = "";
                return true;
            }
            buffer_ = "";
        } else if (ch != '\r') {
            if (buffer_.length() < 159U) buffer_ += ch;
            else buffer_ = ""; /* 超长帧视为异常并丢弃，防止内存持续增长。 */
        }
    }
    return false;
}
