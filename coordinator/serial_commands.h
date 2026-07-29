#pragma once

#include "config.h"
#include "endpoints.h"
#include "device_registry.h"
#include "alarm_control.h"
#include "pir_handler.h"
#include "logging.h"

void printHelp() {
  Serial.println();
  Serial.println("Coordinator commands:");
  Serial.println("  help       - show commands");
  Serial.println("  mode       - show current mode");
  Serial.println("  mode1/auto - PIR node controls alarm node");
  Serial.println("  mode2/man  - manual control only");
  Serial.println("  list       - print saved bound devices");
  Serial.println("  bound      - print raw Zigbee bound devices");
  Serial.println("  open       - open Zigbee network for pairing");
  Serial.println("  close      - close Zigbee network");
  Serial.println("  on         - turn alarm/light ON");
  Serial.println("  off        - turn alarm/light OFF");
  Serial.println("  state      - read alarm/light state");
  Serial.println("  test       - turn ON for 2 seconds, then OFF");
  Serial.println("  reset      - Zigbee factory reset");
  Serial.printf("  verbose    - toggle full auto-event log (currently %s)\n", verboseLog ? "ON" : "OFF");
  Serial.println();
}

void printRawBoundDevices() {
  Serial.println("Switch/alarm bound devices:");
  zbSwitch.printBoundDevices(Serial);
  Serial.println("PIR receiver bound devices:");
  zbPirReceiver.printBoundDevices(Serial);
}

void printDevices() {
  syncBoundDevices();

  Serial.println();
  Serial.println("===== Bound devices =====");
  Serial.print("Mode: ");
  Serial.println(modeName());

  if (deviceCount == 0) {
    Serial.println("No devices bound yet.");
  }

  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices[i].active) {
      continue;
    }

    Serial.printf(
      "[%u] short=0x%04X endpoint=%u ieee=%s",
      i,
      devices[i].shortAddr,
      devices[i].endpoint,
      Zigbee.formatIEEEAddress(devices[i].ieeeAddr)
    );

    if (devices[i].endpoint == ALARM_ENDPOINT) {
      Serial.print(" alarm");
      if (devices[i].lastStateKnown) {
        Serial.printf(" state=%s", devices[i].lastState ? "ON" : "OFF");
      }
    }

    if (devices[i].endpoint == PIR_NODE_ENDPOINT) {
      Serial.print(" pir");
      if (devices[i].occupancyKnown) {
        Serial.printf(" motion=%s", devices[i].occupancy ? "YES" : "NO");
      }
    }

    if (devices[i].lastSeenMs > 0) {
      Serial.printf(" seen=%lus", (millis() - devices[i].lastSeenMs) / 1000UL);
    }

    Serial.println();
  }

  Serial.printf("Total: %u device(s)\n", deviceCount);
  Serial.println("=========================");
  Serial.println();
}

void handleCommand(String command) {
  command.trim();
  command.toLowerCase();

  if (command.length() == 0) {
    return;
  }

  if (command == "help" || command == "?") {
    printHelp();
  } else if (command == "mode") {
    printMode();
  } else if (command == "mode1" || command == "auto") {
    setMode(MODE_AUTO_PIR);
  } else if (command == "mode2" || command == "manual" || command == "man") {
    setMode(MODE_MANUAL);
  } else if (command == "list") {
    printDevices();
  } else if (command == "bound") {
    printRawBoundDevices();
  } else if (command == "open") {
    Serial.printf("Opening Zigbee network for %u seconds...\n", JOIN_OPEN_SECONDS);
    Zigbee.openNetwork(JOIN_OPEN_SECONDS);
  } else if (command == "close") {
    Serial.println("Closing Zigbee network...");
    Zigbee.closeNetwork();
  } else if (command == "on") {
    sendAlarmOn();
  } else if (command == "off") {
    sendAlarmOff();
  } else if (command == "state") {
    readAlarmState();
  } else if (command == "test") {
    sendAlarmOn();
    delay(2000);
    sendAlarmOff();
  } else if (command == "verbose") {
    verboseLog = !verboseLog;
    Serial.printf("Verbose auto-event log: %s\n", verboseLog ? "ON" : "OFF");
  } else if (command == "reset") {
    Serial.println("Factory reset in 2 seconds...");
    delay(2000);
    Zigbee.factoryReset();
  } else {
    Serial.print("Unknown command: ");
    Serial.println(command);
    Serial.println("Type 'help' to see commands.");
  }
}

void handleSerial() {
  if (!Serial.available()) {
    return;
  }

  String command = Serial.readStringUntil('\n');
  handleCommand(command);
}