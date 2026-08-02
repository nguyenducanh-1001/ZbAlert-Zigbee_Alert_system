#include <Arduino.h>
#include "config.h"
#include "Zigbee.h"
#include "pir_sensor.h"
#include "watchdog.h"
#include "battery.h"
#include "ota_client.h"

unsigned long lastBatteryReport = 0;

void setup() {
#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
#endif
  setupWatchdog();
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

  // Khai báo đây là thiết bị chạy pin, % pin + điện áp ban đầu đo được ngay lúc boot
  zbPir.setPowerSource(ZB_POWER_SOURCE_BATTERY, readBatteryPercentage(), readBatteryVoltage100mV());

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

  reportBattery();
  lastBatteryReport = millis();
}

void loop() {
  feedWatchdog();
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

  if (millis() - lastBatteryReport >= BATTERY_REPORT_INTERVAL_MS) {
    reportBattery();
    lastBatteryReport = millis();
  }

  delay(20);
}
