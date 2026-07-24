#include <Arduino.h>

#ifndef ZIGBEE_MODE_ED
#error "Select Zigbee ED mode in Arduino IDE: Tools -> Zigbee mode"
#endif

#include "Zigbee.h"

#define PIR_ENDPOINT 10
#define PIR_PIN 6
#define LED_PIN 7

#define PIR_ACTIVE_LEVEL HIGH
#define PIR_DEBOUNCE_MS 80UL
#define HEARTBEAT_REPORT_INTERVAL_MS 10000UL
#define CONNECT_PRINT_INTERVAL_MS 1000UL

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

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
#ifdef BOOT_PIN
  pinMode(BOOT_PIN, INPUT_PULLUP);
#endif

  rawPirState = readPir();
  confirmedPirState = rawPirState;
  rawPirChangedAt = millis();
  setDebugLed(confirmedPirState);

  zbPir.setManufacturerAndModel("Espressif", "PirNode");
  zbPir.setSensorType(ESP_ZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_PIR);

  Serial.println();
  Serial.println("Adding PIR occupancy endpoint...");
  Zigbee.addEndpoint(&zbPir);

  Serial.println("Starting Zigbee as End Device...");
  if (!Zigbee.begin(ZIGBEE_END_DEVICE)) {
    Serial.println("Zigbee failed to start. Restarting...");
    ESP.restart();
  }

  waitForNetwork();

  reportPirState(confirmedPirState, "boot");
  lastHeartbeatReport = millis();
}

void loop() {
  handleFactoryResetButton();

  bool connected = Zigbee.connected();
  if (connected != lastConnected) {
    lastConnected = connected;
    Serial.println(connected ? "Zigbee connected." : "Zigbee disconnected.");

    if (connected) {
      reportPirState(confirmedPirState, "reconnect");
      lastHeartbeatReport = millis();
    }
  }

  bool currentRawState = readPir();
  setDebugLed(currentRawState);

  if (currentRawState != rawPirState) {
    rawPirState = currentRawState;
    rawPirChangedAt = millis();
  }

  if ((millis() - rawPirChangedAt >= PIR_DEBOUNCE_MS) && (rawPirState != confirmedPirState)) {
    confirmedPirState = rawPirState;
    reportPirState(confirmedPirState, "change");
    lastHeartbeatReport = millis();
  }

  if (millis() - lastHeartbeatReport >= HEARTBEAT_REPORT_INTERVAL_MS) {
    reportPirState(confirmedPirState, "heartbeat");
    lastHeartbeatReport = millis();
  }

  delay(20);
}
