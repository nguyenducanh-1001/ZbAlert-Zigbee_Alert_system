#include "Zigbee.h"

#define ALARM_ENDPOINT 20
#define LED_PIN 15
#define BUZZER_PIN 9

ZigbeeLight zbAlarm = ZigbeeLight(ALARM_ENDPOINT);

bool lastConnected = false;

void setAlarmState(bool state) {
  digitalWrite(LED_PIN, state ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
  Serial.printf("Alarm state: %s\n", state ? "ON" : "OFF");
}

void setup() {
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0); // tắt LED RGB tích hợp
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  setAlarmState(false);

  zbAlarm.setManufacturerAndModel("Espressif", "AlarmNode");

  // callback nhận lệnh On/Off từ coordinator
  zbAlarm.onLightChange(setAlarmState);

  Zigbee.addEndpoint(&zbAlarm);

  // node router (không sleep, vì cần luôn lắng nghe lệnh)
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