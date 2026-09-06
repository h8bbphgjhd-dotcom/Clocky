#include "clock_manager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>

namespace {

struct TzEntry {
  const char* name;
  const char* posix;
};

const TzEntry kTimezoneTable[] = {
  {"UTC", "UTC0"},
  {"GMT", "GMT0"},
  {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
  {"America/Phoenix", "MST7"},
  {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
  {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
  {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
  {"America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0"},
  {"Pacific/Honolulu", "HST10"},
  {"America/Toronto", "EST5EDT,M3.2.0,M11.1.0"},
  {"America/Vancouver", "PST8PDT,M3.2.0,M11.1.0"},
  {"America/Mexico_City", "CST6"},
  {"America/Sao_Paulo", "<-03>3"},
  {"America/Argentina/Buenos_Aires", "<-03>3"},
  {"America/Lima", "PET5"},
  {"America/Bogota", "COT5"},
  {"America/Caracas", "<-04>4"},
  {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
  {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Stockholm", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Oslo", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Copenhagen", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Warsaw", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Budapest", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Prague", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Vienna", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Zurich", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Istanbul", "TRT-3"},
  {"Asia/Dubai", "GST-4"},
  {"Asia/Kolkata", "IST-5:30"},
  {"Asia/Bangkok", "ICT-7"},
  {"Asia/Singapore", "SGT-8"},
  {"Asia/Hong_Kong", "HKT-8"},
  {"Asia/Shanghai", "CST-8"},
  {"Asia/Tokyo", "JST-9"},
  {"Asia/Seoul", "KST-9"},
  {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"Australia/Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"Australia/Brisbane", "AEST-10"},
  {"Australia/Perth", "AWST-8"},
  {"Australia/Adelaide", "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
  {"Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
  {"Pacific/Fiji", "FJT-12"},
  {"Pacific/Guam", "ChST-10"},
  {"Africa/Cairo", "EET-2EEST,M4.5.5/0,M10.5.4/24"},
  {"Africa/Johannesburg", "SAST-2"},
  {"Africa/Lagos", "WAT-1"},
  {"Africa/Nairobi", "EAT-3"}
};

ClockManager* g_clockInstance = nullptr;

}  // namespace

ClockManager::ClockManager(SettingsManager& settings) : settings_(settings) {}

const char* ClockManager::getPosixTz() const {
  const char* tz = settings_.getSettings().timezone;
  if (!tz || strlen(tz) == 0) {
    return "MST7MDT,M3.2.0,M11.1.0";
  }

  // If already formatted as POSIX (contains comma)
  if (strchr(tz, ',') != nullptr) {
    return tz;
  }

  for (size_t i = 0; i < sizeof(kTimezoneTable) / sizeof(kTimezoneTable[0]); i++) {
    if (strcasecmp(tz, kTimezoneTable[i].name) == 0) {
      return kTimezoneTable[i].posix;
    }
  }

  // Default fallback (Mountain Time)
  return "MST7MDT,M3.2.0,M11.1.0";
}

void ClockManager::begin() {
  g_clockInstance = this;
  applyTimezone();
  seedFromCompileTime();

  // Immediate step on time sync
  sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  sntp_set_time_sync_notification_cb([](struct timeval* tv) {
    if (g_clockInstance) {
      g_clockInstance->onTimeSync(tv);
    }
  });

  forceSync();
}

void ClockManager::update() {
  uint32_t now = millis();

  if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED && !timeSynced_) {
    timeSynced_ = true;
    lastSyncTime_ = time(nullptr);
  }

  // If Wi-Fi is connected but time has not synced yet, retry every 10 seconds
  if (!timeSynced_ && WiFi.isConnected()) {
    if (now - lastSyncAttempt_ > 10000) {
      forceSync();
    }
  } else if (timeSynced_ && (now - lastSyncAttempt_ > SYNC_INTERVAL_MS)) {
    forceSync();
  }
}

void ClockManager::onTimeSync(struct timeval* tv) {
  timeSynced_ = true;
  lastSyncTime_ = tv ? tv->tv_sec : time(nullptr);
  applyTimezone();

  char timeBuf[32];
  char dateBuf[48];
  formatTime(timeBuf, sizeof(timeBuf));
  formatDate(dateBuf, sizeof(dateBuf));
  Serial.printf("[NTP] *** Time synced to local time: %s, %s (TZ: %s, POSIX: %s) ***\n",
                timeBuf, dateBuf, settings_.getSettings().timezone, getPosixTz());
}

void ClockManager::forceSync() {
  applyTimezone();
  Serial.printf("[NTP] Requesting NTP sync for TZ '%s' (%s)...\n",
                settings_.getSettings().timezone, getPosixTz());
  configTzTime(getPosixTz(), "pool.ntp.org", "time.nist.gov", "time.google.com");
  lastSyncAttempt_ = millis();
}

void ClockManager::applyTimezone() {
  const char* posixTz = getPosixTz();
  setenv("TZ", posixTz, 1);
  tzset();
}

void ClockManager::formatTime(char* buffer, size_t size) const {
  time_t t = time(nullptr);
  struct tm local;
  localtime_r(&t, &local);

  const Settings& s = settings_.getSettings();
  if (s.use24Hour) {
    strftime(buffer, size, "%H:%M:%S", &local);
  } else {
    strftime(buffer, size, "%I:%M:%S %p", &local);
    if (buffer[0] == '0') {
      memmove(buffer, buffer + 1, strlen(buffer));
    }
  }
}

void ClockManager::formatDate(char* buffer, size_t size) const {
  time_t t = time(nullptr);
  struct tm local;
  localtime_r(&t, &local);
  strftime(buffer, size, "%B %d, %Y", &local);
}

void ClockManager::formatWeekday(char* buffer, size_t size) const {
  time_t t = time(nullptr);
  struct tm local;
  localtime_r(&t, &local);
  strftime(buffer, size, "%A", &local);
}

int ClockManager::getHour() const {
  time_t t = time(nullptr);
  struct tm local;
  localtime_r(&t, &local);
  return local.tm_hour;
}

int ClockManager::getMinute() const {
  time_t t = time(nullptr);
  struct tm local;
  localtime_r(&t, &local);
  return local.tm_min;
}

int ClockManager::getSecond() const {
  time_t t = time(nullptr);
  struct tm local;
  localtime_r(&t, &local);
  return local.tm_sec;
}

void ClockManager::seedFromCompileTime() {
  time_t t = time(nullptr);
  struct tm* tm = localtime(&t);
  if (tm->tm_year + 1900 > 2025) {
    return;
  }

  struct tm compiled = {};
  if (strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &compiled) == nullptr) {
    return;
  }

  timeval tv;
  tv.tv_sec = mktime(&compiled);
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}
