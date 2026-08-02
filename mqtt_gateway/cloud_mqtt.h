#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"

WiFiClientSecure secureWifiClient;
PubSubClient mqttClient(secureWifiClient);
unsigned long lastMqttAttempt = 0;
unsigned long lastWifiAttempt = 0;
unsigned long wifiBackoffMs = 15000UL;
const unsigned long WIFI_RECONNECT_MAX_MS = 60000UL;

void onWifiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("WiFi disconnect reason code: %d\n", info.wifi_sta_disconnected.reason);
}

// Reset backoff ve gia tri thap khi ket noi lai thanh cong, tranh cho lan
// mat ket noi tiep theo phai cho lau vi backoff cu con giu tu truoc.
void onWifiGotIp(WiFiEvent_t event, WiFiEventInfo_t info) {
  wifiBackoffMs = 15000UL;
}

void connectWiFi() {
  WiFi.onEvent(onWifiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(onWifiGotIp, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  // Giam cong suat phat WiFi - dong dien tieu thu tang manh khi phat o cong
  // suat cao, neu bo dieu ap 3.3V tren board yeu (thuong gap o cac board
  // "Super Mini" gia re) se bi sut ap dung luc xac thuc, gay AUTH_EXPIRE.
  // Giam cong suat lam giam dinh dong, danh doi la giam tam phu song - chap
  // nhan duoc vi board dung trong nha, gan router.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  // Tat auto-reconnect co san cua ESP-IDF - neu khong, no se tu dong thu
  // ket noi lai rat nhanh moi khi disconnect, chong lan len logic
  // ensureWifiConnected() cua minh, tao ra vong lap ket noi lai lien tuc.
  // Router co the coi day la hanh vi bat thuong va bat dau tu choi/het han
  // xac thuc lien tuc (reason=2 AUTH_EXPIRE) de chong lu.
  WiFi.setAutoReconnect(false);

  // Dung DNS cong khai (Google + Cloudflare) thay vi DNS router - tranh loi
  // phan giai ten mien that bai am tham (tra ve 0.0.0.0). Van giu IP/gateway/
  // subnet theo DHCP binh thuong (INADDR_NONE = de DHCP tu quyet dinh).
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));

  Serial.printf("Connecting to WiFi \"%s\"", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect failed (timeout). Se tu ket noi lai trong loop().");
  }
}

// connectWiFi() chi chay 1 lan trong setup(). Ham nay goi trong loop() de tu
// ket noi lai dinh ky khi mat WiFi, khong chan (non-blocking). Dung
// exponential backoff (tang dan thoi gian cho giua cac lan thu) thay vi
// khoang thoi gian co dinh, tranh lam phien router qua nhanh.
void ensureWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  if (now - lastWifiAttempt < wifiBackoffMs) {
    return;
  }
  lastWifiAttempt = now;

  Serial.printf("WiFi mat ket noi, dang thu ket noi lai (backoff=%lus)...\n", wifiBackoffMs / 1000UL);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  wifiBackoffMs = min(wifiBackoffMs * 2, WIFI_RECONNECT_MAX_MS);
}

void setupMqtt() {
  // DEMO: bo qua xac thuc chung chi CA cho don gian.
  // Muon an toan hon: dung secureWifiClient.setCACert(rootCaPem) voi root CA
  // dung cua HiveMQ (ISRG Root X1) hoac AWS IoT (Amazon Root CA 1).
  secureWifiClient.setInsecure();
  mqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
}

bool ensureMqttConnected() {
  if (mqttClient.connected()) {
    return true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  unsigned long now = millis();
  if (now - lastMqttAttempt < MQTT_RECONNECT_INTERVAL_MS) {
    return false;
  }
  lastMqttAttempt = now;

  Serial.print("Connecting MQTT...");
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("OK");
    return true;
  }

  Serial.printf("failed, rc=%d\n", mqttClient.state());
  return false;
}

// Nhan eventType + payload JSON tu C6 qua UART (goi tu uart_link.h), tu ghep
// thanh topic day du va publish len MQTT.
void publishEvent(const char *eventType, const char *jsonPayload) {
  if (!ensureMqttConnected()) {
    Serial.println("MQTT chua ket noi, bo qua publish.");
    return;
  }

  char topic[96];
  snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_PREFIX, eventType);
  mqttClient.publish(topic, jsonPayload);
}