#pragma once

#include <list>
#include "logging.h"
#include "Zigbee.h"
#include "config.h"
#include "endpoints.h"

struct DeviceRecord {
  uint16_t shortAddr;
  uint8_t endpoint;
  esp_zb_ieee_addr_t ieeeAddr;
  bool active;
  bool lastStateKnown;
  bool lastState;
  bool occupancyKnown;
  bool occupancy;
  unsigned long lastSeenMs;
};

DeviceRecord devices[MAX_DEVICES];
uint8_t deviceCount = 0;

bool sameIeee(const esp_zb_ieee_addr_t left, const esp_zb_ieee_addr_t right) {
  return memcmp(left, right, sizeof(esp_zb_ieee_addr_t)) == 0;
}

int findDevice(const zb_device_params_t *dev) {
  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices[i].active) {
      continue;
    }

    bool sameShortAddress = devices[i].shortAddr == dev->short_addr;
    bool sameEndpoint = devices[i].endpoint == dev->endpoint;
    bool sameLongAddress = sameIeee(devices[i].ieeeAddr, dev->ieee_addr);

    if (sameEndpoint && (sameShortAddress || sameLongAddress)) {
      return i;
    }
  }

  return -1;
}

int findFreeSlot() {
  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices[i].active) {
      return i;
    }
  }

  return -1;
}

void saveDevice(uint8_t index, const zb_device_params_t *dev) {
  devices[index].shortAddr = dev->short_addr;
  devices[index].endpoint = dev->endpoint;
  memcpy(devices[index].ieeeAddr, dev->ieee_addr, sizeof(esp_zb_ieee_addr_t));
  devices[index].lastSeenMs = millis();
}

void addOrUpdateDevice(const zb_device_params_t *dev, bool seen[]) {
  int index = findDevice(dev);

  if (index < 0) {
    index = findFreeSlot();
    if (index < 0) {
      Serial.println("Device list is full. Cannot save new device.");
      return;
    }

    devices[index].active = true;
    devices[index].lastStateKnown = false;
    devices[index].lastState = false;
    devices[index].occupancyKnown = false;
    devices[index].occupancy = false;
    devices[index].lastSeenMs = millis();
    deviceCount++;

    logf(
      ">>> New bound device: slot=%d short=0x%04X endpoint=%u ieee=%s\n",
      index,
      dev->short_addr,
      dev->endpoint,
      Zigbee.formatIEEEAddress(dev->ieee_addr)
    );
  }

  saveDevice(index, dev);
  seen[index] = true;
}

void addBoundDevicesFrom(std::list<zb_device_params_t *> boundDevices, bool seen[]) {
  for (const auto &dev : boundDevices) {
    if (dev != nullptr) {
      addOrUpdateDevice(dev, seen);
    }
  }
}

void syncBoundDevices() {
  bool seen[MAX_DEVICES] = {false};

  addBoundDevicesFrom(zbSwitch.getBoundDevices(), seen);
  addBoundDevicesFrom(zbPirReceiver.getBoundDevices(), seen);

  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (devices[i].active && !seen[i]) {
      logf(
        "<<< Device removed: slot=%u short=0x%04X endpoint=%u ieee=%s\n",
        i,
        devices[i].shortAddr,
        devices[i].endpoint,
        Zigbee.formatIEEEAddress(devices[i].ieeeAddr)
      );
      devices[i].active = false;
      devices[i].lastStateKnown = false;
      devices[i].occupancyKnown = false;
      if (deviceCount > 0) {
        deviceCount--;
      }
    }
  }
}

int firstAlarmDevice() {
  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (devices[i].active && devices[i].endpoint == ALARM_ENDPOINT) {
      return i;
    }
  }

  return -1;
}

void markPirReport(uint8_t endpoint, uint16_t shortAddr, bool motion) {
  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices[i].active) {
      continue;
    }

    if (devices[i].endpoint == endpoint && devices[i].shortAddr == shortAddr) {
      devices[i].occupancyKnown = true;
      devices[i].occupancy = motion;
      devices[i].lastSeenMs = millis();
    }
  }
}
