#pragma once

#ifndef ZIGBEE_MODE_ZCZR
#error "Select a Zigbee coordinator/router mode in Arduino IDE: Tools -> Zigbee mode"
#endif

#define SWITCH_ENDPOINT 1
#define PIR_RECEIVER_ENDPOINT 2

#define PIR_NODE_ENDPOINT 10
#define ALARM_ENDPOINT 20

#define MAX_DEVICES 20
#define JOIN_OPEN_SECONDS 180
#define DEVICE_SCAN_INTERVAL_MS 5000UL
#define DEVICE_PRINT_INTERVAL_MS 30000UL
#define STATE_POLL_INTERVAL_MS 10000UL

#define ALARM_ACTION_NONE 0
#define ALARM_ACTION_ON 1
#define ALARM_ACTION_OFF 2

enum CoordinatorMode : uint8_t {
  MODE_AUTO_PIR = 1,
  MODE_MANUAL = 2,
};
