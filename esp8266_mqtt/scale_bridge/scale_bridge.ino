#include "config.h"
#include "mqtt_bridge.h"
#include "stm32_link.h"

/* ESP8266入口：连接串口接收器与MQTT桥，不参与重量计算。 */
static Stm32Link stm32;
static MqttBridge mqtt;

void setup()
{
    /* 两端串口波特率必须与STM32 USART2保持一致。 */
    stm32.begin(STM32_BAUDRATE);
    mqtt.begin();
}

void loop()
{
    String json;
    /* 持续维护网络连接，并把每条STM32 JSON原样发布到MQTT。 */
    mqtt.loop();
    if (stm32.readLine(json)) mqtt.publishState(json);
    delay(1);
}
