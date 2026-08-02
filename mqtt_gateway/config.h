#pragma once

// ---- WiFi ----
#define WIFI_SSID "Thuy"
#define WIFI_PASSWORD "qt285vinhxuyen"
#define WIFI_CONNECT_TIMEOUT_MS 15000UL

// ---- MQTT (HiveMQ Cloud - user/pass + TLS) ----
#define MQTT_BROKER_HOST "60784e93683d45dc9b0d9b7ed3d1d38e.s1.eu.hivemq.cloud"
#define MQTT_BROKER_PORT 8883
#define MQTT_USERNAME "esp32c6_coordinator"
#define MQTT_PASSWORD "23020780"
#define MQTT_CLIENT_ID "esp32c3-mqtt-gateway"
#define MQTT_TOPIC_PREFIX "smarthome/security/"
#define MQTT_RECONNECT_INTERVAL_MS 5000UL

// ---- UART bridge tu ESP32-C6 (chip Zigbee) ----
// QUAN TRONG: 2 chan duoi la vi du - kiem tra lai so do chan (pinout) that
// cua board C3 ban dang dung, tranh cac chan strapping (tren nhieu board
// C3 la GPIO2, GPIO8, GPIO9). Noi cheo: TX cua C6 -> RX cua C3, RX cua C6
// -> TX cua C3, va phai noi chung GND giua 2 board.
#define UART_BRIDGE_BAUD 115200
#define UART_BRIDGE_TX_PIN 5
#define UART_BRIDGE_RX_PIN 4