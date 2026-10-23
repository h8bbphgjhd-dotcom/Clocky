#include "timezones.h"
#include <cassert>
#include <cstdlib>
#include <ctime>

int hourAt(const char* zone, time_t epoch, int expectedDst) {
  int index = timezoneIndex(zone);
  assert(index >= 0);
  setenv("TZ", kTimezoneTable[index].posix, 1);
  tzset();
  tm local{};
  localtime_r(&epoch, &local);
  assert(local.tm_isdst == expectedDst);
  return local.tm_hour;
}

int main() {
  for (size_t i = 0; i < kTimezoneCount; ++i) {
    assert(strlen(kTimezoneTable[i].name) < 32);
    assert(timezoneIndex(kTimezoneTable[i].name) == static_cast<int>(i));
  }
  assert(timezoneIndex(nullptr) == -1);
  assert(timezoneIndex("") == -1);
  assert(timezoneIndex("Invalid/Zone") == -1);
  assert(timezoneIndex("america/denver") == timezoneIndex("America/Denver"));
  // Noon UTC in winter and summer checks automatic DST and fixed offsets.
  const time_t winter = 1768478400; // 2026-01-15 12:00 UTC
  const time_t summer = 1784116800; // 2026-07-15 12:00 UTC
  assert(hourAt("UTC", winter, 0) == 12);
  assert(hourAt("America/Denver", winter, 0) == 5);
  assert(hourAt("America/Denver", summer, 1) == 6);
  assert(hourAt("America/Phoenix", summer, 0) == 5);
  assert(hourAt("Asia/Kolkata", summer, 0) == 17);
  tm local{};
  localtime_r(&summer, &local);
  assert(local.tm_min == 30);
  assert(hourAt("Australia/Sydney", winter, 1) == 23);
  assert(hourAt("Australia/Sydney", summer, 0) == 22);
}
