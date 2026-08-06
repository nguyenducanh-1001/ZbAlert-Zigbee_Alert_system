#include <Arduino.h>
#include "config.h"
#include "cloud_mqtt.h"
#include "uart_link.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  connectWiFi();
  setupUartBridge();
  setupBlynk();

  Serial.println("Blynk gateway (ESP32-C3/S3) started. Cho du lieu tu ESP32-C6 qua UART...");
}

void loop() {
  ensureWifiConnected();

  if (ensureBlynkConnected()) {
    Blynk.run();
  }

  handleUartBridge();
}