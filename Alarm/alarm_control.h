#pragma once

#include "Zigbee.h"
#include "config.h"

ZigbeeLight zbAlarm = ZigbeeLight(ALARM_ENDPOINT);

void setAlarmState(bool state) {
  digitalWrite(LED_PIN, state ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
  Serial.printf("Alarm state: %s\n", state ? "ON" : "OFF");
}
