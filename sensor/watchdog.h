#pragma once

#include "esp_task_wdt.h"
#include "config.h"

void setupWatchdog() {
  esp_task_wdt_deinit();  // gỡ watchdog mặc định (nếu có) trước khi cấu hình lại

  esp_task_wdt_config_t wdtConfig = {
    .timeout_ms = WATCHDOG_TIMEOUT_MS,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true,
  };

  esp_task_wdt_init(&wdtConfig);
  esp_task_wdt_add(NULL);
}

void feedWatchdog() {
  esp_task_wdt_reset();
}