#pragma once

#include <Arduino.h>
#include "config.h"
#include "cloud_mqtt.h"

// UART1 rieng cho lien lac voi ESP32-C6 - khong dung chung voi Serial (USB
// CDC) dang dung de debug/console qua Serial Monitor.
HardwareSerial uartBridge(1);

void setupUartBridge() {
  uartBridge.begin(UART_BRIDGE_BAUD, SERIAL_8N1, UART_BRIDGE_RX_PIN, UART_BRIDGE_TX_PIN);
  // Mac dinh Stream::readStringUntil() cho timeout 1000ms - qua lau doi voi
  // toc do 115200 baud giua 2 board canh nhau, gay do tre khi doc dong chua
  // toi du (available()=true nhung chi moi vai byte dau). Giam xuong 50ms.
  uartBridge.setTimeout(50);
}

// Doc 1 dong tu C6: "<eventType>|<jsonPayload>", tach ra va chuyen sang Blynk
// (goi routeEventToBlynk() dinh nghia trong cloud_mqtt.h).
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
  routeEventToBlynk(eventType.c_str(), payload.c_str());
}

// Gui lenh dieu khien xuong C6 qua UART, dinh dang: "cmd|<value>". C6 se doc
// va goi sendAlarmOn()/sendAlarmOff() tuong ung. Duoc goi tu BLYNK_WRITE(V2)
// trong cloud_mqtt.h khi app doi gia tri Alarm.
void sendCommandDownlink(const char *cmd) {
  Serial.printf("Gui lenh xuong C6: %s\n", cmd);
  uartBridge.print("cmd|");
  uartBridge.println(cmd);
}