#include <Arduino.h>
#include "config.h"
#include "Zigbee.h"
#include "ha/esp_zigbee_ha_standard.h"
#include <list>

#include "pir_receiver.h"
#include "endpoints.h"
#include "device_registry.h"
#include "alarm_control.h"
#include "pir_handler.h"
#include "serial_commands.h"

void setup() {
#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
#endif

  Serial.begin(115200);
  Serial.setTimeout(50);
  delay(500);

  memset(devices, 0, sizeof(devices));

  zbSwitch.setManufacturerAndModel("Espressif", "CoordinatorGateway");
  zbSwitch.allowMultipleBinding(true);
  zbSwitch.onLightStateChange(onLightStateChange);

  zbPirReceiver.setManufacturerAndModel("Espressif", "PirReceiver");
  zbPirReceiver.allowMultipleBinding(true);
  zbPirReceiver.onOccupancyChange(onPirReport);

  Zigbee.allowMultiEndpointBinding(true);

  Serial.println();
  Serial.println("Adding coordinator switch and PIR receiver endpoints...");
  Zigbee.addEndpoint(&zbSwitch);
  Zigbee.addEndpoint(&zbPirReceiver);

  Zigbee.setRebootOpenNetwork(JOIN_OPEN_SECONDS);

  if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
    Serial.println("Zigbee failed to start. Restarting...");
    ESP.restart();
  }

  Serial.println("Coordinator started.");
  Serial.printf("Network is open for %u seconds after boot.\n", JOIN_OPEN_SECONDS);
  Serial.println("Mode 1: PIR node controls alarm node.");
  Serial.println("Mode 2: manual commands control alarm node.");
  printHelp();
  printMode();
}

void loop() {
  handleSerial();
  handlePendingAlarmAction();

  static unsigned long lastScan = 0;
  if (millis() - lastScan >= DEVICE_SCAN_INTERVAL_MS) {
    syncBoundDevices();
    lastScan = millis();
  }

  static unsigned long lastPoll = 0;
  if (millis() - lastPoll >= STATE_POLL_INTERVAL_MS) {
    if (zbSwitch.bound()) {
      readAlarmState(false);  // false = tự động, im lặng trừ khi bật "verbose"
    }
    lastPoll = millis();
  }

  delay(20);
}