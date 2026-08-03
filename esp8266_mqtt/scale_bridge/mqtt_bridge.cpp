#include "mqtt_bridge.h"
#include "config.h"

/* 网络桥接模块：维护Wi-Fi/MQTT连接，并发布最近的电子秤状态。 */
MqttBridge::MqttBridge() : mqtt_(wifiClient_), lastAttempt_(0UL)
{
}

void MqttBridge::begin()
{
    /* STA模式表示ESP8266作为客户端接入现有无线路由器。 */
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    mqtt_.setServer(MQTT_HOST, MQTT_PORT);
    mqtt_.setCallback(onMessage);
    mqtt_.setBufferSize(512U);
    mqtt_.setKeepAlive(30U);
    mqtt_.setSocketTimeout(3U);
    pendingState_.reserve(256U);
    connectWifi();
}

void MqttBridge::connectWifi()
{
    if (WiFi.status() != WL_CONNECTED) WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void MqttBridge::connectMqtt()
{
    bool connected;
    /* 用户名或密码为空时使用匿名连接，适配无认证的本地Broker。 */
    if ((MQTT_USERNAME[0] != '\0') || (MQTT_PASSWORD[0] != '\0')) {
        connected = mqtt_.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD,
                                  MQTT_STATUS_TOPIC, 1U, true, "offline");
    } else {
        connected = mqtt_.connect(MQTT_CLIENT_ID, MQTT_STATUS_TOPIC,
                                  1U, true, "offline");
    }
    if (connected) {
        mqtt_.subscribe(MQTT_COMMAND_TOPIC);
        mqtt_.publish(MQTT_STATUS_TOPIC, "online", true);
        if (pendingState_.length() > 0U) {
            mqtt_.publish(MQTT_STATE_TOPIC, pendingState_.c_str(), true);
            pendingState_ = "";
        }
    }
}

void MqttBridge::loop()
{
    unsigned long now = millis();
    /* Wi-Fi每5秒重试一次；联网后MQTT每3秒重试一次。 */
    if (WiFi.status() != WL_CONNECTED) {
        if ((unsigned long)(now - lastAttempt_) >= 5000UL) {
            lastAttempt_ = now;
            connectWifi();
        }
        return;
    }
    if (!mqtt_.connected()) {
        if ((unsigned long)(now - lastAttempt_) >= 3000UL) {
            lastAttempt_ = now;
            connectMqtt();
        }
        return;
    }
    mqtt_.loop();
}

bool MqttBridge::publishState(const String &json)
{
    if (!mqtt_.connected()) {
        pendingState_ = json;
        return false;
    }
    /* retain=true让新订阅者能够立即得到最近一次称重状态。 */
    if (mqtt_.publish(MQTT_STATE_TOPIC, json.c_str(), true)) {
        pendingState_ = "";
        return true;
    }
    pendingState_ = json;
    return false;
}

bool MqttBridge::publishNetworkInfo()
{
    char payload[160];
    String ip;

    if (!mqtt_.connected() || (WiFi.status() != WL_CONNECTED)) return false;
    ip = WiFi.localIP().toString();
    snprintf(payload, sizeof(payload),
             "{\"device\":\"hx711-scale\",\"status\":\"online\","
             "\"ip\":\"%s\",\"rssi_dbm\":%ld,\"uptime_s\":%lu}",
             ip.c_str(), (long)WiFi.RSSI(), (unsigned long)(millis() / 1000UL));
    return mqtt_.publish(MQTT_NETWORK_TOPIC, payload, true);
}

bool MqttBridge::wifiConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

bool MqttBridge::mqttConnected()
{
    return mqtt_.connected();
}

String MqttBridge::statusText()
{
    if (WiFi.status() != WL_CONNECTED) return "wifi_connecting";
    if (!mqtt_.connected()) return "mqtt_connecting";
    return "online";
}

void MqttBridge::onMessage(char *topic, byte *payload, unsigned int length)
{
    /* 预留远程去皮等命令入口；当前版本只订阅但不执行远程控制。 */
    (void)topic;
    (void)payload;
    (void)length;
}
