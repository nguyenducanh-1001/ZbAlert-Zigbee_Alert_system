#pragma once

#include <Arduino.h>
#include "config.h"
#include "cloud_mqtt.h"

// UART1 rieng cho lien lac voi ESP32-C6 - khong dung chung voi Serial (USB
// CDC) dang dung de debug/console qua Serial Monitor.
HardwareSerial uartBridge(1);

void setupUartBridge() {
  uartBridge.begin(UART_BRIDGE_BAUD, SERIAL_8N1, UART_BRIDGE_RX_PIN, UART_BRIDGE_TX_PIN);
}

// Doc 1 dong tu C6: "<eventType>|<jsonPayload>", tach ra va publish len MQTT.
void handleUartBridge() {
  if (!uartBridge.available()) {
    return;
  }

  String line = uartBridge.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) {
    return;
  }

  int sep = line.indexOf('|');
  if (sep < 0) {
    Serial.print("UART: dong khong dung dinh dang (thieu '|'): ");
    Serial.println(line);
    return;
  }

  String eventType = line.substring(0, sep);
  String payload = line.substring(sep + 1);
  publishEvent(eventType.c_str(), payload.c_str());
}