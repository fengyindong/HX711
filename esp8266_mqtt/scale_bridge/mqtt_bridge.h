#ifndef MQTT_BRIDGE_H
#define MQTT_BRIDGE_H

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

class MqttBridge {
public:
    MqttBridge();
    /* 配置Wi-Fi工作模式、MQTT服务器和回调函数。 */
    void begin();
    /* 非阻塞维护Wi-Fi与MQTT连接，并处理MQTT心跳。 */
    void loop();
    /* 把STM32原始JSON发布为retain状态，成功返回true。 */
    bool publishState(const String &json);

private:
    void connectWifi();
    void connectMqtt();
    static void onMessage(char *topic, byte *payload, unsigned int length);

    WiFiClient wifiClient_;
    PubSubClient mqtt_;
    unsigned long lastAttempt_; /* 限制重连频率，避免阻塞主循环。 */
};

#endif
