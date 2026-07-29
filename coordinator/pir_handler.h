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