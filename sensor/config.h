#pragma once

#ifndef ZIGBEE_MODE_ED
#error "Select Zigbee ED mode in Arduino IDE: Tools -> Zigbee mode"
#endif

#define PIR_ENDPOINT 10
#define PIR_PIN 2
#define LED_PIN 15

#define PIR_ACTIVE_LEVEL HIGH
#define PIR_DEBOUNCE_MS 80UL
#define HEARTBEAT_REPORT_INTERVAL_MS 10000UL
#define CONNECT_PRINT_INTERVAL_MS 1000UL
#define WATCHDOG_TIMEOUT_MS 30000UL

#define BATTERY_ADC_PIN 4
#define BATTERY_DIVIDER_RATIO 2

#define BATTERY_MIN_MV 3000  // ~0%  (pin gần cạn)
#define BATTERY_MAX_MV 4200  // ~100% (pin đầy)
// Chu kỳ gửi % pin lên coordinator
#define BATTERY_REPORT_INTERVAL_MS 30000UL  // 30s