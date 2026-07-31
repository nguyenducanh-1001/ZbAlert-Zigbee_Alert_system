#pragma once

#include "config.h"
#include "device_registry.h"
#include "alarm_control.h"
#include "logging.h"

CoordinatorMode currentMode = MODE_AUTO_PIR;
volatile uint8_t pendingAlarmAction = ALARM_ACTION_NONE;

bool lastPirKnown = false;
bool lastPirMotion = false;
uint16_t lastPirShortAddr = 0xFFFF;
uint8_t lastPirEndpoint = 0;
unsigned long lastPirReportMs = 0;

const char *modeName() {
  return currentMode == MODE_AUTO_PIR ? "mode 1: PIR -> alarm" : "mode 2: manual";
}

void printMode() {
  Serial.print("Current mode: ");
  Serial.println(modeName());

  if (lastPirKnown) {
    Serial.printf(
      "Last PIR: %s from short=0x%04X endpoint=%u, %lu second(s) ago\n",
      lastPirMotion ? "MOTION" : "CLEAR",
      lastPirShortAddr,
      lastPirEndpoint,
      (millis() - lastPirReportMs) / 1000UL
    );
  } else {
    Serial.println("Last PIR: none");
  }
}

void setMode(CoordinatorMode mode) {
  currentMode = mode;
  pendingAlarmAction = ALARM_ACTION_NONE;
  printMode();
}

void onPirReport(bool motion, uint8_t srcEndpoint, uint16_t shortAddr) {
  lastPirKnown = true;
  lastPirMotion = motion;
  lastPirShortAddr = shortAddr;
  lastPirEndpoint = srcEndpoint;
  lastPirReportMs = millis();

  markPirReport(srcEndpoint, shortAddr, motion);

  logf(
    "PIR report: %s from short=0x%04X endpoint=%u | %s\n",
    motion ? "MOTION" : "CLEAR",
    shortAddr,
    srcEndpoint,
    modeName()
  );

  if (currentMode == MODE_AUTO_PIR) {
    pendingAlarmAction = motion ? ALARM_ACTION_ON : ALARM_ACTION_OFF;
  }
}

unsigned long lastBatteryReportMs = 0;
// Cửa sổ lọc trùng: gói bị tầng radio gửi lại (retransmit do mất ACK) sẽ đến
// rất gần nhau (thường <1s); chu kỳ report thật sự cách nhau hàng chục giây
// (BATTERY_REPORT_INTERVAL_MS bên sensor). 3s đủ rộng để lọc trùng mà không
// bao giờ chặn nhầm 1 report thật.
const unsigned long BATTERY_DEDUP_WINDOW_MS = 3000UL;

void onBatteryReport(uint8_t percent, uint8_t srcEndpoint, uint16_t shortAddr) {
  unsigned long now = millis();

  if (now - lastBatteryReportMs < BATTERY_DEDUP_WINDOW_MS) {
    logf("Bo qua battery report trung (den qua gan lan truoc, khoang %lums).\n", now - lastBatteryReportMs);
    return;
  }
  lastBatteryReportMs = now;

  markBatteryReport(srcEndpoint, shortAddr, percent);
  Serial.printf("Battery report: %u%% from short=0x%04X endpoint=%u\n", percent, shortAddr, srcEndpoint);
}

void handlePendingAlarmAction() {
  uint8_t action = pendingAlarmAction;
  if (action == ALARM_ACTION_NONE) {
    return;
  }

  pendingAlarmAction = ALARM_ACTION_NONE;

  if (action == ALARM_ACTION_ON) {
    sendAlarmOn(false);
  } else if (action == ALARM_ACTION_OFF) {
    sendAlarmOff(false);
  }
}

// Failsafe: nếu đang coi là MOTION (nên alarm đang ON) mà quá lâu không có
// báo cáo PIR mới (cả report thay đổi lẫn heartbeat) - rất có thể sensor đã
// mất kết nối/rớt mạng - tự tắt alarm để tránh bật mãi mãi.
void checkPirTimeout() {
  if (currentMode != MODE_AUTO_PIR) {
    return;
  }
  if (!lastPirKnown || !lastPirMotion) {
    return;
  }
  if (millis() - lastPirReportMs < MOTION_TIMEOUT_MS) {
    return;
  }

  Serial.printf(
    "PIR timeout: khong co bao cao moi tu short=0x%04X trong %lus -> failsafe tat alarm.\n", lastPirShortAddr, MOTION_TIMEOUT_MS / 1000UL
  );

  lastPirMotion = false;  // tránh lặp lại failsafe mỗi vòng loop
  pendingAlarmAction = ALARM_ACTION_OFF;
}