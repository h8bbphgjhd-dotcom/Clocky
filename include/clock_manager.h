#pragma once

#include <ctime>
#include <sys/time.h>
#include "settings_manager.h"

class ClockManager {
 public:
  ClockManager(SettingsManager& settings);
  void begin();
  void update();
  
  // Time formatting
  void formatTime(char* buffer, size_t size) const;
  void formatDate(char* buffer, size_t size) const;
  void formatWeekday(char* buffer, size_t size) const;
  int getHour() const;
  int getMinute() const;
  int getSecond() const;
  
  // Status
  bool isTimeSynced() const { return timeSynced_; }
  time_t getLastSyncTime() const { return lastSyncTime_; }
  const char* getTimezone() const { return settings_.getSettings().timezone; }
  const char* getPosixTz() const;
  
  // Force NTP sync & apply timezone
  void forceSync();
  void applyTimezone();
  void onTimeSync(struct timeval* tv);

 private:
  SettingsManager& settings_;
  bool timeSynced_ = false;
  time_t lastSyncTime_ = 0;
  uint32_t lastSyncAttempt_ = 0;
  static constexpr uint32_t SYNC_INTERVAL_MS = 3600000;  // 1 hour
  
  void seedFromCompileTime();
};
