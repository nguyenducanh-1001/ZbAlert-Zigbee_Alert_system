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

#define MOTION_TIMEOUT_MS 30000UL

#define DEVICE_OFFLINE_TIMEOUT_MS 30000UL

enum CoordinatorMode : uint8_t {
  MODE_AUTO_PIR = 1,
  MODE_MANUAL = 2,
};

// ---- UART bridge toi ESP32-C3 (chip rieng xu ly WiFi/MQTT) ----
// Board nay (C6) chi lam Zigbee coordinator, KHONG dung WiFi nua - vi C6
// chi co 1 radio 2.4GHz dung chung cho WiFi va Zigbee (802.15.4), chay
// dong thoi gay mat goi UDP/TCP (da xac nhan qua chan doan DNS tra ve
// 0.0.0.0). Moi su kien (PIR, alarm, battery, device online/offline) duoc
// gui qua UART sang ESP32-C3, C3 se publish len MQTT thay.
//
// QUAN TRONG: 2 chan duoi la vi du - kiem tra lai so do chan (pinout) that
// cua board C6 ban dang dung truoc khi noi day, tranh cac chan strapping/
// boot. Noi cheo: TX cua C6 -> RX cua C3, RX cua C6 -> TX cua C3, va phai
// noi chung GND giua 2 board.
#define UART_BRIDGE_BAUD 115200
#define UART_BRIDGE_TX_PIN 2
#define UART_BRIDGE_RX_PIN 3