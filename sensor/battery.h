#pragma once

#include "Zigbee.h"
#include "config.h"
#include "pir_sensor.h"  // dùng chung đối tượng zbPir

// Đọc điện áp qua ADC rồi quy đổi ra % pin còn lại (0-100).
// Chỉnh BATTERY_ADC_PIN / BATTERY_DIVIDER_RATIO / BATTERY_MIN_MV / BATTERY_MAX_MV
// trong config.h cho đúng mạch phân áp thực tế của bạn.
uint8_t readBatteryPercentage() {
  uint32_t adcMv = analogReadMilliVolts(BATTERY_ADC_PIN);
  uint32_t batteryMv = adcMv * BATTERY_DIVIDER_RATIO;

  if (batteryMv <= BATTERY_MIN_MV) {
    return 0;
  }
  if (batteryMv >= BATTERY_MAX_MV) {
    return 100;
  }

  return (uint8_t)((batteryMv - BATTERY_MIN_MV) * 100UL / (BATTERY_MAX_MV - BATTERY_MIN_MV));
}

// Đọc điện áp pin ra đơn vị 100mV (ví dụ 37 = 3.7V) - dùng cho setPowerSource().
uint8_t readBatteryVoltage100mV() {
  uint32_t adcMv = analogReadMilliVolts(BATTERY_ADC_PIN);
  uint32_t batteryMv = adcMv * BATTERY_DIVIDER_RATIO;
  return (uint8_t)(batteryMv / 100UL);
}

void reportBattery() {
  uint8_t percent = readBatteryPercentage();
  uint8_t voltage100mv = readBatteryVoltage100mV();

  zbPir.setBatteryPercentage(percent);
  zbPir.setBatteryVoltage(voltage100mv);
  zbPir.reportBatteryPercentage();  // thử lại - sketch test chạy được khi setPowerSource() có đủ voltage
  Serial.printf("Battery: %u%% (%.1fV)\n", percent, voltage100mv / 10.0);
}