#pragma once

#ifndef ZIGBEE_MODE_ED
#error "Select Zigbee ED mode in Arduino IDE: Tools -> Zigbee mode"
#endif

#define PIR_ENDPOINT 10
#define PIR_PIN 6
#define LED_PIN 7

#define PIR_ACTIVE_LEVEL HIGH
#define PIR_DEBOUNCE_MS 80UL
#define HEARTBEAT_REPORT_INTERVAL_MS 10000UL
#define CONNECT_PRINT_INTERVAL_MS 1000UL