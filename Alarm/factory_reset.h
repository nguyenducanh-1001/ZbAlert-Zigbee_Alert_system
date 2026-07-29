#pragma once

#include "Zigbee.h"
#include "config.h"

void handleFactoryResetButton() {
#ifdef BOOT_PIN
  if (digitalRead(BOOT_PIN) != LOW) {
    return;
  }

  delay(100);
  unsigned long pressedAt = millis();

  while (digitalRead(BOOT_PIN) == LOW) {
    digitalWrite(LED_PIN, (millis() / 150) % 2 == 0 ? HIGH : LOW);
    delay(50);

    if (millis() - pressedAt > 3000UL) {
      Serial.println("Factory reset Zigbee and reboot...");
      delay(500);
      Zigbee.factoryReset();
    }
  }

  digitalWrite(LED_PIN, LOW);
#endif
}