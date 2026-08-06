#pragma once

#include <Arduino.h>
#include "config.h"

void sendAlarmOn(bool announce = true);
void sendAlarmOff(bool announce = true);

// UART1 rieng cho lien lac voi ESP32-C3 - khong dung chung voi Serial (USB
// CDC) dang dung de debug/console qua Serial Monitor.
HardwareSerial uartBridge(1);

void setupUartBridge() {
  uartBridge.begin(UART_BRIDGE_BAUD, SERIAL_8N1, UART_BRIDGE_RX_PIN, UART_BRIDGE_TX_PIN);
  // Mac dinh Stream::readStringUntil() cho timeout 1000ms - qua lau doi voi
  // toc do 115200 baud giua 2 board canh nhau, gay do tre khi doc dong chua
  // toi du (available()=true nhung chi moi vai byte dau). Giam xuong 50ms.
  uartBridge.setTimeout(50);
}

// Gui 1 su kien sang C3 theo dinh dang dong van ban: <eventType>|<jsonPayload>
// C3 se tach chuoi theo dau '|' de biet topic con (eventType) va payload,
// roi tu ghep topic day du va publish len MQTT.
void publishEvent(const char *eventType, const char *jsonPayload) {
  uartBridge.print(eventType);
  uartBridge.print('|');
  uartBridge.println(jsonPayload);
}

// Cac ham duoi day GIU NGUYEN chu ky (signature) so voi cloud_mqtt.h cu -
// device_registry.h, alarm_control.h, pir_handler.h khong can sua gi them
// ngoai dong #include.

void publishMotionEvent(bool motion, uint16_t shortAddr) {
  char payload[128];
  snprintf(
    payload, sizeof(payload), "{\"event\":\"%s\",\"short_addr\":\"0x%04X\",\"ts\":%lu}", motion ? "motion" : "clear", shortAddr, millis()
  );
  publishEvent("pir", payload);
}

void publishAlarmEvent(bool state) {
  char payload[96];
  snprintf(payload, sizeof(payload), "{\"event\":\"alarm\",\"state\":\"%s\",\"ts\":%lu}", state ? "on" : "off", millis());
  publishEvent("alarm", payload);
}

void publishDeviceEvent(const char *action, uint16_t shortAddr, uint8_t endpoint) {
  char payload[128];
  snprintf(
    payload, sizeof(payload), "{\"event\":\"%s\",\"short_addr\":\"0x%04X\",\"endpoint\":%u,\"ts\":%lu}", action, shortAddr, endpoint,
    millis()
  );
  publishEvent("device", payload);
}

void publishBatteryEvent(uint8_t percent, uint16_t shortAddr) {
  char payload[112];
  snprintf(
    payload, sizeof(payload), "{\"event\":\"battery\",\"percent\":%u,\"short_addr\":\"0x%04X\",\"ts\":%lu}", percent, shortAddr, millis()
  );
  publishEvent("battery", payload);
}

// Doc lenh dieu khien tu C3 gui xuong qua UART: "cmd|on" hoac "cmd|off"
void handleUartCommand() {
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
    return;
  }

  String type = line.substring(0, sep);
  String value = line.substring(sep + 1);

  if (type == "cmd") {
    Serial.printf("Nhan lenh tu C3: %s\n", value.c_str());
    if (value == "on") {
      sendAlarmOn(false);
    } else if (value == "off") {
      sendAlarmOff(false);
    }
  }
}