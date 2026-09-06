#include "clock_time.h"

#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

void ClockTime::begin() { seedFromCompileTimeIfUnset(); }

void ClockTime::formatTime(char* buffer, size_t size) const {
  const time_t t = ::time(nullptr);
  struct tm local {};
  localtime_r(&t, &local);
  strftime(buffer, size, "%H:%M:%S", &local);
}

void ClockTime::formatDate(char* buffer, size_t size) const {
  const time_t t = ::time(nullptr);
  struct tm local {};
  localtime_r(&t, &local);
  strftime(buffer, size, "%Y-%m-%d", &local);
}

int ClockTime::second() const {
  const time_t t = ::time(nullptr);
  struct tm local {};
  localtime_r(&t, &local);
  return local.tm_sec;
}

void ClockTime::seedFromCompileTimeIfUnset() {
  if (::time(nullptr) > 1000000) {
    return;
  }

  struct tm compiled {};
  if (strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &compiled) == nullptr) {
    return;
  }

  timeval tv {};
  tv.tv_sec = mktime(&compiled);
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}
