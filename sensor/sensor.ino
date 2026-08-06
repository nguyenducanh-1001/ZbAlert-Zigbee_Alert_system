#include <Arduino.h>
#include "esp_sleep.h"
#include "config.h"
#include "Zigbee.h"
#include "pir_sensor.h"
#include "watchdog.h"
#include "battery.h"

// Cấu hình 2 nguồn wake cho lần deep sleep tiếp theo:
// 1) Timer  -> heartbeat / cơ hội báo pin định kỳ.
// 2) GPIO   -> PIR chuyển sang mức active -> thức ngay lập tức.
//
// LƯU Ý CHIP: esp_deep_sleep_enable_gpio_wakeup() là API GPIO-wakeup hợp nhất
// dùng cho các chip Zigbee-capable (ESP32-C6 / ESP32-H2), vì các chip này
// không có vùng RTC GPIO riêng như ESP32/S2/S3 cổ điển nên KHÔNG dùng được
// esp_sleep_enable_ext0_wakeup()/ext1_wakeup(). Nếu bạn build cho ESP32/S3
// cổ điển, đổi sang ext0/ext1 wakeup thay cho hàm này.
static void configureNextWakeup() {
  esp_sleep_enable_timer_wakeup((uint64_t)WAKE_TIME_TO_SLEEP_S * 1000000ULL);

  esp_deep_sleep_enable_gpio_wakeup(
    1ULL << PIR_PIN,
    PIR_ACTIVE_LEVEL ? ESP_GPIO_WAKEUP_GPIO_HIGH : ESP_GPIO_WAKEUP_GPIO_LOW
  );
}

static void goToSleep() {
  Serial.println("Going to sleep now.");
  Serial.flush();
  configureNextWakeup();
  esp_deep_sleep_start();
}

void setup() {
#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
#endif
  setupWatchdog();
  Serial.begin(115200);
  delay(200);

  rtcBootCount++;
  esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();

  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
#ifdef BOOT_PIN
  pinMode(BOOT_PIN, INPUT_PULLUP);
#endif

  bool rawPir = readPir();
  setDebugLed(rawPir);

  Serial.println();
  Serial.printf("Wake #%lu, reason=%d, PIR raw=%d\n", (unsigned long)rtcBootCount, (int)wakeReason, rawPir);

  zbPir.setManufacturerAndModel("Espressif", "PirNode");
  zbPir.setSensorType(ESP_ZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_PIR);

  // Khai báo đây là thiết bị chạy pin, % pin + điện áp ban đầu đo được ngay lúc boot
  zbPir.setPowerSource(ZB_POWER_SOURCE_BATTERY, readBatteryPercentage(), readBatteryVoltage100mV());

  Serial.println("Adding PIR occupancy endpoint...");
  Zigbee.addEndpoint(&zbPir);

  // Tăng keep_alive so với mặc định (3s) để tránh coordinator/router coi
  // node "rớt mạng" giữa các chu kỳ deep sleep dài. Tham số thứ 2 của
  // Zigbee.begin() (erase_nvs) để "false" để tái sử dụng thông tin mạng đã
  // join từ NVS, giúp mỗi lần thức dậy join lại nhanh hơn nhiều so với join
  // từ đầu. Kiểm tra chữ ký hàm khớp với version core esp32 bạn đang dùng
  // (Boards Manager) trước khi build, vì API sleepy-device còn khá mới.
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive = ZB_KEEP_ALIVE_MS;

  Serial.println("Starting Zigbee as End Device...");
  if (!Zigbee.begin(&zigbeeConfig, false)) {
    Serial.println("Zigbee failed to start. Restarting...");
    ESP.restart();
  }

  waitForNetwork();

  // Dùng confirmRealMotion() thay vì tin thẳng vào wakeReason==GPIO: GPIO
  // peripheral tắt hoàn toàn trong deep sleep, nên ngay sau BẤT KỲ lần thức
  // nào (kể cả do timer), lần đọc chân PIR đầu tiên có thể bị glitch/nổi
  // giả HIGH trong thoáng chốc - đọc xác nhận lại lần 2 trước khi kết luận
  // đây thực sự là motion.
  bool wokenByPir = (wakeReason == ESP_SLEEP_WAKEUP_GPIO) && confirmRealMotion();
  bool firstBoot = (wakeReason == ESP_SLEEP_WAKEUP_UNDEFINED);

  if (wokenByPir || firstBoot) {
    bool motion = wokenByPir ? true : readPir();
    rtcConfirmedPirState = motion;
    reportPirState(motion, wokenByPir ? "pir-wake" : "boot");

    if (motion) {
      holdAwakeUntilPirClears();
      rtcConfirmedPirState = false;
      reportPirState(false, "pir-clear");
    }
  } else {
    // Wake định kỳ theo timer: gửi heartbeat giữ nguyên trạng thái hiện tại.
    reportPirState(rtcConfirmedPirState, "heartbeat");
  }

  rtcWakeCount++;
  if (firstBoot || (rtcWakeCount % BATTERY_REPORT_EVERY_N_WAKES == 0)) {
    reportBattery();
  }

  // Cho gói tin kịp bay khỏi module trước khi cắt radio vào deep sleep.
  delay(ZB_TX_FLUSH_DELAY_MS);

  goToSleep();
}

void loop() {
  // Không dùng tới - mỗi chu kỳ thức được xử lý trọn vẹn trong setup(),
  // sau đó thiết bị quay lại deep sleep ngay (esp_deep_sleep_start() không
  // bao giờ return, nên loop() sẽ không bao giờ được gọi tới).
}