#include "Zigbee.h"
#include "config.h"
#include "alarm_control.h"
#include "factory_reset.h"

bool lastConnected = false;

void setup() {
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0); // turn off RGB
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
#ifdef BOOT_PIN
  pinMode(BOOT_PIN, INPUT_PULLUP);
#endif
  setAlarmState(false);

  zbAlarm.setManufacturerAndModel("Espressif", "AlarmNode");

  // callback function receive On/Off from coordinator
  zbAlarm.onLightChange(setAlarmState);

  Zigbee.addEndpoint(&zbAlarm);

  // End Device (không sleep, vì cần luôn lắng nghe lệnh) - KHÔNG dùng Router,
  // vì Router có thể bị các thiết bị khác (như sensor) chọn làm parent trong
  // mesh -> tắt nguồn Alarm sẽ kéo theo mất luôn cả sensor. End Device không
  // bao giờ làm parent cho ai, tránh phụ thuộc chéo này.
  if (!Zigbee.begin(ZIGBEE_END_DEVICE)) {
    Serial.println("Zigbee init failed, restarting...");
    ESP.restart();
  }

  Serial.println("Connecting to network...");
  while (!Zigbee.connected()) {
    delay(100);
  }
  lastConnected = true;
  Serial.println("Joined! Waiting for alarm commands...");
}

void loop() {
  handleFactoryResetButton();

  bool connected = Zigbee.connected();

  if (connected != lastConnected) {
    lastConnected = connected;

    if (connected) {
      Serial.println("Connected");
    } else {
      Serial.println("Disconnected");
    }
  }

  delay(500);
}