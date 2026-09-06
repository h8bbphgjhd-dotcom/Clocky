#pragma once

#include <ctime>
#include "settings_manager.h"

class CountdownManager {
 public:
  CountdownManager(SettingsManager& settings);
  
  // Format time remaining for display
  void formatRemaining(time_t targetDate, char* buffer, size_t size, bool showSeconds = false) const;
  void formatRemainingShort(time_t targetDate, char* buffer, size_t size) const;
  
  // Check if countdown is expired
  bool isExpired(time_t targetDate) const;
  
  // Get days/hours/minutes/seconds remaining
  struct RemainingTime {
    int days = 0;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    bool expired = false;
  };
  RemainingTime getRemaining(time_t targetDate) const;

 private:
  SettingsManager& settings_;
};

