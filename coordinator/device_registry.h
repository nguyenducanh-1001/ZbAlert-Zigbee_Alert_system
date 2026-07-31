#pragma once

#include <list>
#include "Zigbee.h"
#include "config.h"
#include "endpoints.h"
#include "logging.h"

struct DeviceRecord {
  uint16_t shortAddr;
  uint8_t endpoint;
  esp_zb_ieee_addr_t ieeeAddr;
  bool active;
  bool lastStateKnown;
  bool lastState;
  bool occupancyKnown;
  bool occupancy;
  bool batteryKnown;
  uint8_t batteryPercent;
  unsigned long lastSeenMs;     // lần cuối còn trong bảng bind (không phản ánh online/offline thật)
  unsigned long lastTrafficMs;  // lần cuối THỰC SỰ nhận được dữ liệu (0 = chưa từng)
  bool offlineNotified;
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
  // 0xFFFF (broadcast short addr) và 255 (wildcard endpoint) không phải địa
  // chỉ thiết bị thật - đôi lúc lọt vào trong lúc bind, phải loại bỏ ngay,
  // không thì sẽ lưu thành 1 "device ma" gây rác list / sai địa chỉ gửi lệnh.
  if (dev->short_addr == 0xFFFF || dev->endpoint == 0xFF) {
    static unsigned long lastInvalidWarnMs = 0;
    const unsigned long INVALID_WARN_INTERVAL_MS = 60000UL;  // tối đa 1 lần/phút

    if (millis() - lastInvalidWarnMs >= INVALID_WARN_INTERVAL_MS) {
      logf(
        "!!! Bo qua device khong hop le: short=0x%04X endpoint=%u (broadcast/wildcard)\n",
        dev->short_addr,
        dev->endpoint
      );
      lastInvalidWarnMs = millis();
    }
    return;
  }

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
    devices[index].batteryKnown = false;
    devices[index].batteryPercent = 0;
    devices[index].lastSeenMs = millis();
    devices[index].lastTrafficMs = 0;
    devices[index].offlineNotified = false;
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

  // KHÔNG xoá device chỉ vì 1 hoặc vài lần quét không thấy trong
  // getBoundDevices() - dữ liệu này bị race condition thật (thư viện ghi từ
  // Zigbee task, mình đọc từ loop task, không có khoá) nên có thể thiếu entry
  // liên tục vài chu kỳ mỗi khi có thiết bị KHÁC đang bind cùng lúc, dù thiết
  // bị đang xét vẫn hoạt động bình thường. Việc phát hiện mất kết nối thật đã
  // có checkDevicesOffline() lo, dựa trên traffic ZCL thật đáng tin cậy hơn.
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
      devices[i].lastTrafficMs = millis();

      if (devices[i].offlineNotified) {
        Serial.printf("Device short=0x%04X endpoint=%u da online tro lai.\n", shortAddr, endpoint);
        devices[i].offlineNotified = false;
      }
    }
  }
}

void markBatteryReport(uint8_t endpoint, uint16_t shortAddr, uint8_t percent) {
  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices[i].active) {
      continue;
    }

    if (devices[i].endpoint == endpoint && devices[i].shortAddr == shortAddr) {
      devices[i].batteryKnown = true;
      devices[i].batteryPercent = percent;
      devices[i].lastSeenMs = millis();
      devices[i].lastTrafficMs = millis();

      if (devices[i].offlineNotified) {
        Serial.printf("Device short=0x%04X endpoint=%u da online tro lai.\n", shortAddr, endpoint);
        devices[i].offlineNotified = false;
      }
    }
  }
}

// Failsafe chung: quét toàn bộ device, nếu quá lâu không có traffic THẬT
// (không tính binding table) thì coi là offline - dùng để phát hiện sensor
// bị tắt nguồn/mất kết nối dù binding table vẫn còn giữ nó.
void checkDevicesOffline() {
  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices[i].active) {
      continue;
    }
    if (devices[i].lastTrafficMs == 0) {
      continue;  // chưa từng nhận traffic thật, chưa đủ dữ liệu để kết luận
    }
    if (devices[i].offlineNotified) {
      continue;  // đã báo rồi, tránh spam mỗi vòng loop
    }
    if (millis() - devices[i].lastTrafficMs < DEVICE_OFFLINE_TIMEOUT_MS) {
      continue;
    }

    Serial.printf(
      "!!! Device OFFLINE: short=0x%04X endpoint=%u - khong co traffic that trong %lus\n",
      devices[i].shortAddr,
      devices[i].endpoint,
      DEVICE_OFFLINE_TIMEOUT_MS / 1000UL
    );
    devices[i].offlineNotified = true;
  }
}