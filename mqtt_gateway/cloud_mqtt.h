#pragma once

#include <WiFi.h>
#include "config.h"

#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp32.h>

unsigned long lastWifiAttempt = 0;
unsigned long wifiBackoffMs = 15000UL;
const unsigned long WIFI_RECONNECT_MAX_MS = 60000UL;

unsigned long lastBlynkAttempt = 0;
const unsigned long BLYNK_RECONNECT_INTERVAL_MS = 5000UL;

// Dinh nghia o uart_link.h - gui lenh xuong C6 qua UART khi app doi Alarm.
void sendCommandDownlink(const char *cmd);

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

// Chi cau hinh, KHONG tu ket noi ngay (khac Blynk.begin() mac dinh se tu lo
// WiFi luon) - de logic WiFi tu viet o tren toan quyen quyet dinh thoi diem
// ket noi, tranh xung dot 2 co che quan ly WiFi cung luc.
void setupBlynk() {
  Blynk.config(BLYNK_AUTH_TOKEN);
}

// Goi dinh ky trong loop(), khong chan (non-blocking), co backoff co dinh
// don gian (Blynk.connect() da co timeout noi bo nen khong can exponential
// backoff phuc tap nhu WiFi).
bool ensureBlynkConnected() {
  if (Blynk.connected()) {
    return true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  unsigned long now = millis();
  if (now - lastBlynkAttempt < BLYNK_RECONNECT_INTERVAL_MS) {
    return false;
  }
  lastBlynkAttempt = now;

  Serial.print("Connecting Blynk...");
  if (Blynk.connect()) {
    Serial.println("OK");
    return true;
  }

  Serial.println("failed");
  return false;
}

// ================== Trich gia tri tu payload JSON don gian ==================
// Cac payload JSON tu C6 gui sang co cau truc co dinh, don gian (khong long
// nhau, khong mang) nen tu parse bang strstr/strtol la du, khong can keo them
// thu vien ArduinoJson chi de doc vai truong.

bool extractJsonString(const char *json, const char *key, char *out, size_t outSize) {
  char pattern[32];
  snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
  const char *start = strstr(json, pattern);
  if (start == nullptr) {
    return false;
  }
  start += strlen(pattern);
  const char *end = strchr(start, '"');
  if (end == nullptr) {
    return false;
  }
  size_t len = (size_t)(end - start);
  if (len >= outSize) {
    len = outSize - 1;
  }
  memcpy(out, start, len);
  out[len] = '\0';
  return true;
}

bool extractJsonInt(const char *json, const char *key, long *out) {
  char pattern[32];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char *start = strstr(json, pattern);
  if (start == nullptr) {
    return false;
  }
  start += strlen(pattern);
  *out = strtol(start, nullptr, 10);
  return true;
}

// ================== Chuyen su kien tu C6 sang Virtual Pin cua Blynk ==================
// Goi tu uart_link.h moi khi doc duoc 1 dong "<eventType>|<jsonPayload>" tu C6.
// Mapping virtual pin: V0=PIR (motion/clear), V1=Battery (%), V2=Alarm (0/1),
// V3=DeviceStatus (text). Phai khop dung ten/kieu datastream da tao trong
// Blynk Console.
// Endpoint la ID co dinh theo loai thiet bi trong mang Zigbee (dat trong
// config.h cua C6: ALARM_ENDPOINT=20, PIR_NODE_ENDPOINT=10), khong doi theo
// thoi gian nhu short_addr (short_addr co the doi khi thiet bi rejoin mang).
const char *friendlyNodeName(long endpoint) {
  if (endpoint == 20) return "Alarm";
  if (endpoint == 10) return "Sensor";
  return "Thiet bi khac";
}

void routeEventToBlynk(const char *eventType, const char *jsonPayload) {
  if (!Blynk.connected()) {
    Serial.println("Blynk chua ket noi, bo qua cap nhat.");
    return;
  }

  if (strcmp(eventType, "pir") == 0) {
    char event[16];
    if (extractJsonString(jsonPayload, "event", event, sizeof(event))) {
      // Datastream PIR (V0) la kieu Enum ("clear"=0, "motion"=1) - phai gui
      // so nguyen index, khong gui chuoi text. Neu ban sap xep thu tu nhan
      // Enum khac trong Blynk Console (vd "motion" dung index 0), doi lai
      // gia tri 0/1 duoi day cho khop.
      int enumIndex = (strcmp(event, "motion") == 0) ? 1 : 0;
      Blynk.virtualWrite(V0, enumIndex);
    }
    return;
  }

  if (strcmp(eventType, "battery") == 0) {
    long percent;
    if (extractJsonInt(jsonPayload, "percent", &percent)) {
      Blynk.virtualWrite(V1, (int)percent);
    }
    return;
  }

  if (strcmp(eventType, "alarm") == 0) {
    char state[8];
    if (extractJsonString(jsonPayload, "state", state, sizeof(state))) {
      Blynk.virtualWrite(V2, strcmp(state, "on") == 0 ? 1 : 0);
    }
    return;
  }

  if (strcmp(eventType, "device") == 0) {
    char event[16];
    char shortAddr[16];
    long endpoint;
    bool hasEvent = extractJsonString(jsonPayload, "event", event, sizeof(event));
    bool hasAddr = extractJsonString(jsonPayload, "short_addr", shortAddr, sizeof(shortAddr));
    bool hasEndpoint = extractJsonInt(jsonPayload, "endpoint", &endpoint);
    if (hasEvent && hasAddr && hasEndpoint) {
      char status[40];
      snprintf(status, sizeof(status), "%s: %s", friendlyNodeName(endpoint), event);
      Blynk.virtualWrite(V3, status);
    }
    return;
  }

  // Cac eventType khac (vd "system") chua map sang datastream nao - bo qua.
}

// ================== Nhan lenh dieu khien tu app (downlink) ==================
// Tu dong duoc goi khi widget Switch gan voi Virtual Pin V2 (Alarm) tren app
// doi gia tri - khong can code goi thu cong.
BLYNK_WRITE(V2) {
  int value = param.asInt();
  Serial.printf("Blynk cmd nhan tu app: Alarm = %d\n", value);
  sendCommandDownlink(value == 1 ? "on" : "off");
}