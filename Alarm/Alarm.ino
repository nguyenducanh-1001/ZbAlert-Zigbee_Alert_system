#include "Zigbee.h"
#include "config.h"
#include "alarm_control.h"
#include "factory_reset.h"

bool lastConnected = false;

void setup() {
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0); // tắt LED RGB tích hợp
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
#ifdef BOOT_PIN
  pinMode(BOOT_PIN, INPUT_PULLUP);
#endif
  setAlarmState(false);

  zbAlarm.setManufacturerAndModel("Espressif", "AlarmNode");
  zbAlarm.onLightChange(setAlarmState);
  Zigbee.addEndpoint(&zbAlarm);

  if (!Zigbee.begin(ZIGBEE_ROUTER)) {
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
    Serial.println(connected ? "Connected" : "Disconnected");
  }

  delay(500);
}