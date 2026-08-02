#ifndef SCALE_ESP_CONFIG_H
#define SCALE_ESP_CONFIG_H

/* 使用前必须填写Wi-Fi和MQTT服务器信息。 */
#define WIFI_SSID           "fengyindong-F30 PRO"
#define WIFI_PASSWORD       "2721587967"
#define MQTT_HOST           "192.168.31.206"
#define MQTT_PORT           1883
#define MQTT_USERNAME       "fengyindong"
#define MQTT_PASSWORD       "2721587967"
#define MQTT_CLIENT_ID      "hx711-scale-01"
#define MQTT_STATE_TOPIC    "scale/weight"
#define MQTT_COMMAND_TOPIC  "scale/command"

/* ESP8266硬件串口RX=GPIO3，连接STM32的PA2 TX。 */
#define STM32_BAUDRATE      9600

#endif
