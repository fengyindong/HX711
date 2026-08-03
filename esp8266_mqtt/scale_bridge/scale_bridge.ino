#include "config.h"
#include "mqtt_bridge.h"
#include "stm32_link.h"

/* ESP8266入口：连接串口接收器与MQTT桥，不参与重量计算。 */
static Stm32Link stm32;
static MqttBridge mqtt;

void setup()
{
    /* 两端串口波特率必须与STM32 USART2保持一致。 */
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    stm32.begin(STM32_BAUDRATE);
    mqtt.begin();
}

void loop()
{
    String json;
    static unsigned long lastStatus = 0UL;
    static unsigned long lastNetworkReport = 0UL;
    /* 持续维护网络连接，并把每条STM32 JSON原样发布到MQTT。 */
    mqtt.loop();
    if (stm32.readLine(json)) mqtt.publishState(json);

    /* 每30秒发布一次ESP8266自身网络状态。 */
    if (millis() - lastNetworkReport >= 30000UL) {
        lastNetworkReport = millis();
        mqtt.publishNetworkInfo();
    }

    /* 板载LED显示网络状态：在线熄灭，连接中以1Hz闪烁。 */
    if (millis() - lastStatus >= 500UL) {
        lastStatus = millis();
        if (mqtt.mqttConnected()) digitalWrite(LED_BUILTIN, HIGH);
        else digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
    delay(1);
}