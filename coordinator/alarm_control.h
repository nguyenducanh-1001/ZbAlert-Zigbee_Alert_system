#pragma once

#include "config.h"
#include "endpoints.h"
#include "device_registry.h"
#include "logging.h"
#include "uart_link.h"

// Khi hàm được gọi tự động (mode 1, poll định kỳ) truyền announce=false để im lặng;
// khi người dùng gõ lệnh, giữ mặc định announce=true để luôn thấy kết quả.
bool announceNextStateReport = false;

void printTarget(uint8_t index, const char *action, bool announce) {
  if (!announce && !verboseLog) {
    return;
  }

  Serial.printf(
    "%s -> slot=%u short=0x%04X endpoint=%u\n",
    action,
    index,
    devices[index].shortAddr,
    devices[index].endpoint
  );
}

void sendAlarmOn(bool announce = true) {
  syncBoundDevices();
  int index = firstAlarmDevice();
  announceNextStateReport = announce;

  if (index >= 0) {
    printTarget(index, "Alarm ON", announce);
    zbSwitch.lightOn(devices[index].endpoint, devices[index].shortAddr);
    return;
  }

  if (zbSwitch.bound()) {
    if (announce || verboseLog) {
      Serial.println("Alarm endpoint not found. Sending ON to all bound On/Off devices.");
    }
    zbSwitch.lightOn();
  } else if (announce || verboseLog) {
    Serial.println("No bound alarm/light yet. Pair alarm_node first.");
  }
}

void sendAlarmOff(bool announce = true) {
  syncBoundDevices();
  int index = firstAlarmDevice();
  announceNextStateReport = announce;

  if (index >= 0) {
    printTarget(index, "Alarm OFF", announce);
    zbSwitch.lightOff(devices[index].endpoint, devices[index].shortAddr);
    return;
  }

  if (zbSwitch.bound()) {
    if (announce || verboseLog) {
      Serial.println("Alarm endpoint not found. Sending OFF to all bound On/Off devices.");
    }
    zbSwitch.lightOff();
  } else if (announce || verboseLog) {
    Serial.println("No bound alarm/light yet. Pair alarm_node first.");
  }
}

void readAlarmState(bool announce = true) {
  syncBoundDevices();
  int index = firstAlarmDevice();
  announceNextStateReport = announce;

  if (index >= 0) {
    printTarget(index, "Read state", announce);
    zbSwitch.getLightState(devices[index].endpoint, devices[index].shortAddr);
    return;
  }

  if (zbSwitch.bound()) {
    if (announce || verboseLog) {
      Serial.println("Alarm endpoint not found. Reading state from all bound On/Off devices.");
    }
    zbSwitch.getLightState();
  } else if (announce || verboseLog) {
    Serial.println("No bound alarm/light yet. Pair alarm_node first.");
  }
}

void onLightStateChange(bool state) {
  if (announceNextStateReport || verboseLog) {
    Serial.printf("Light/alarm state report: %s\n", state ? "ON" : "OFF");
  }
  announceNextStateReport = false;
  // Alarm không nằm trong 3 event gửi cloud (motion/clear/battery) - chỉ giữ log local.

  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (devices[i].active && devices[i].endpoint == ALARM_ENDPOINT) {
      devices[i].lastStateKnown = true;
      devices[i].lastState = state;
      devices[i].lastSeenMs = millis();
      devices[i].lastTrafficMs = millis();
      markDeviceOnline(i);
    }
  }
}