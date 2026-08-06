#pragma once

#ifndef ZIGBEE_MODE_ED
#error "Select Zigbee ED mode in Arduino IDE: Tools -> Zigbee mode"
#endif

#define BOOT_PIN 0
#define PIR_ENDPOINT 10
#define PIR_PIN 2
#define LED_PIN 15

#define PIR_ACTIVE_LEVEL HIGH
#define CONNECT_PRINT_INTERVAL_MS 1000UL
#define WATCHDOG_TIMEOUT_MS 30000UL

#define BATTERY_ADC_PIN 4
#define BATTERY_DIVIDER_RATIO 2

#define BATTERY_MIN_MV 3000  // ~0%  (pin gần cạn)
#define BATTERY_MAX_MV 3800  // ~100% (pin đầy)

// ==== Deep sleep ====
// Chu kỳ wake định kỳ khi KHÔNG có motion (heartbeat + cơ hội kiểm tra pin).
// PIR vẫn đánh thức ngay lập tức khi có motion nhờ GPIO wakeup, không phụ
// thuộc vào chu kỳ này.
#define WAKE_TIME_TO_SLEEP_S 60UL

// Báo pin mỗi N lần wake định kỳ (vd 5 lần * 60s ~ 5 phút/lần báo pin).
#define BATTERY_REPORT_EVERY_N_WAKES 1U

// An toàn: nếu PIR giữ mức active liên tục quá lâu (kẹt / hỏng cảm biến),
// vẫn thoát vòng chờ để không "treo thức" vô thời hạn, tốn pin.
#define PIR_ACTIVE_MAX_HOLD_MS 300000UL  // 5 phút

// Đợi cho gói tin Zigbee kịp gửi đi trước khi cắt radio vào deep sleep.
#define ZB_TX_FLUSH_DELAY_MS 800UL

// keep_alive lớn hơn mặc định (3s) để coordinator không coi node "rớt mạng"
// giữa các chu kỳ ngủ dài - theo khuyến nghị của ví dụ chính thức
// Zigbee_Temp_Hum_Sensor_Sleepy.
#define ZB_KEEP_ALIVE_MS 10000UL