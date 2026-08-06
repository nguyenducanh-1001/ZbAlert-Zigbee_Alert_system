#pragma once

#include "watchdog.h"
#include "Zigbee.h"
#include "config.h"

ZigbeeOccupancySensor zbPir(PIR_ENDPOINT);

// RTC_DATA_ATTR: giữ nguyên giá trị qua các lần deep sleep (mất giá trị khi
// mất nguồn hoàn toàn hoặc nạp lại firmware / nhấn reset EN).
RTC_DATA_ATTR bool rtcConfirmedPirState = false;
RTC_DATA_ATTR uint32_t rtcWakeCount = 0;  // đếm số lần wake định kỳ, để rải lịch báo pin
RTC_DATA_ATTR uint32_t rtcBootCount = 0;  // debug: tổng số lần thức dậy

bool lastConnected = false;
unsigned long lastConnectPrint = 0;

bool readPir() {
  return digitalRead(PIR_PIN) == PIR_ACTIVE_LEVEL;
}

// GPIO peripheral bị tắt hoàn toàn trong deep sleep và không giữ được trạng
// thái chân qua các lần ngủ - ngay khi vừa thức dậy (kể cả thức bởi TIMER,
// không phải PIR), lần đọc digitalRead() đầu tiên đôi khi bị glitch/nổi
// trong vài ms trước khi mức điện áp thật của chân ổn định trở lại. Đọc lại
// lần 2 sau một khoảng ngắn để xác nhận mức active là thật, tránh báo motion
// giả mỗi lần thức định kỳ.
bool confirmRealMotion() {
  if (!readPir()) {
    return false;
  }
  delay(30);
  return readPir();
}

void setDebugLed(bool motion) {
  digitalWrite(LED_PIN, motion ? HIGH : LOW);
}

void reportPirState(bool motion, const char *reason) {
  if (!Zigbee.connected()) {
    return;
  }

  bool updated = zbPir.setOccupancy(motion);
  bool reported = updated && zbPir.report();

  Serial.printf("PIR %s %s %s\n", motion ? "MOTION" : "CLEAR", reason, reported ? "OK" : "FAIL");
}

// Dinh nghia ben duoi - forward declare de goi duoc trong waitForNetwork().
void handleFactoryResetButton();

void waitForNetwork() {
  Serial.print("Connecting to Zigbee network");
  while (!Zigbee.connected()) {
    feedWatchdog();
    unsigned long now = millis();

    if (now - lastConnectPrint >= CONNECT_PRINT_INTERVAL_MS) {
      Serial.print(".");
      lastConnectPrint = now;
    }

    handleFactoryResetButton();

    delay(20);
  }

  Serial.println();
  Serial.println("Joined Zigbee network.");
  lastConnected = true;
}

void handleFactoryResetButton() {
#ifdef BOOT_PIN
  if (digitalRead(BOOT_PIN) != LOW) {
    return;
  }

  delay(100);
  unsigned long pressedAt = millis();

  while (digitalRead(BOOT_PIN) == LOW) {
    feedWatchdog();
    setDebugLed((millis() / 150) % 2 == 0);
    delay(50);

    if (millis() - pressedAt > 3000UL) {
      Serial.println("Factory reset Zigbee and reboot...");
      delay(500);
      Zigbee.factoryReset();
    }
  }

  setDebugLed(rtcConfirmedPirState);
#endif
}

// Sau khi wake vì PIR active, KHÔNG ngủ lại ngay mà ở lại "thức" cho tới khi
// PIR nhả (LOW), rồi mới báo CLEAR và quay lại deep sleep. Có chặn an toàn
// PIR_ACTIVE_MAX_HOLD_MS để không bị treo thức vô thời hạn nếu PIR kẹt/hỏng.
void holdAwakeUntilPirClears() {
  unsigned long activeSince = millis();
  while (readPir()) {
    feedWatchdog();
    handleFactoryResetButton();

    if (millis() - activeSince > PIR_ACTIVE_MAX_HOLD_MS) {
      Serial.println("PIR active qua lau, bo qua cho nha de tranh treo thuc.");
      break;
    }

    delay(50);
  }
}