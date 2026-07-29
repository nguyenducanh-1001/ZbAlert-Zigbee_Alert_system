#pragma once

#include "Zigbee.h"
#include "config.h"

ZigbeeOccupancySensor zbPir(PIR_ENDPOINT);

bool lastConnected = false;
bool rawPirState = false;
bool confirmedPirState = false;
unsigned long rawPirChangedAt = 0;
unsigned long lastHeartbeatReport = 0;
unsigned long lastConnectPrint = 0;

bool readPir() {
  return digitalRead(PIR_PIN) == PIR_ACTIVE_LEVEL;
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

  Serial.printf(
    "PIR %s | reason=%s | report=%s\n",
    motion ? "MOTION" : "CLEAR",
    reason,
    reported ? "OK" : "FAIL"
  );
}

void waitForNetwork() {
  Serial.print("Connecting to Zigbee network");
  while (!Zigbee.connected()) {
    unsigned long now = millis();
    bool motion = readPir();
    setDebugLed(motion);

    if (now - lastConnectPrint >= CONNECT_PRINT_INTERVAL_MS) {
      Serial.print(".");
      lastConnectPrint = now;
    }

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
    setDebugLed((millis() / 150) % 2 == 0);
    delay(50);

    if (millis() - pressedAt > 3000UL) {
      Serial.println("Factory reset Zigbee and reboot...");
      delay(500);
      Zigbee.factoryReset();
    }
  }

  setDebugLed(confirmedPirState);
#endif
}
