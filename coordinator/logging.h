#pragma once
#include <Arduino.h>
#include <stdarg.h>

bool verboseLog = false;

void logf(const char *fmt, ...) {
  if (!verboseLog) return;
  char buf[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
}