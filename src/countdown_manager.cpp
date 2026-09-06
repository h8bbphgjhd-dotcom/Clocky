#include "countdown_manager.h"
#include <ctime>
#include <cstdio>

CountdownManager::CountdownManager(SettingsManager& settings) : settings_(settings) {}

void CountdownManager::formatRemaining(time_t targetDate, char* buffer, size_t size, bool showSeconds) const {
  RemainingTime rt = getRemaining(targetDate);
  if (rt.expired) {
    snprintf(buffer, size, "EXPIRED");
    return;
  }
  
  if (rt.days > 0) {
    if (showSeconds) {
      snprintf(buffer, size, "%d DAYS %02d:%02d:%02d", rt.days, rt.hours, rt.minutes, rt.seconds);
    } else {
      snprintf(buffer, size, "%d DAYS %02d HOURS %02d MIN", rt.days, rt.hours, rt.minutes);
    }
  } else if (rt.hours > 0) {
    if (showSeconds) {
      snprintf(buffer, size, "%02d:%02d:%02d", rt.hours, rt.minutes, rt.seconds);
    } else {
      snprintf(buffer, size, "%02d HOURS %02d MIN", rt.hours, rt.minutes);
    }
  } else if (rt.minutes > 0) {
    if (showSeconds) {
      snprintf(buffer, size, "%02d:%02d", rt.minutes, rt.seconds);
    } else {
      snprintf(buffer, size, "%02d MIN %02d SEC", rt.minutes, rt.seconds);
    }
  } else {
    snprintf(buffer, size, "%02d SEC", rt.seconds);
  }
}

void CountdownManager::formatRemainingShort(time_t targetDate, char* buffer, size_t size) const {
  RemainingTime rt = getRemaining(targetDate);
  if (rt.expired) {
    snprintf(buffer, size, "ENDED");
    return;
  }
  
  if (rt.days > 0) {
    snprintf(buffer, size, "%dd %dh", rt.days, rt.hours);
  } else if (rt.hours > 0) {
    snprintf(buffer, size, "%dh %dm", rt.hours, rt.minutes);
  } else if (rt.minutes > 0) {
    snprintf(buffer, size, "%dm %ds", rt.minutes, rt.seconds);
  } else {
    snprintf(buffer, size, "%ds", rt.seconds);
  }
}

bool CountdownManager::isExpired(time_t targetDate) const {
  return targetDate <= time(nullptr);
}

CountdownManager::RemainingTime CountdownManager::getRemaining(time_t targetDate) const {
  RemainingTime rt;
  time_t now = time(nullptr);
  
  if (targetDate <= now) {
    rt.expired = true;
    return rt;
  }
  
  time_t diff = targetDate - now;
  rt.days = diff / 86400;
  diff %= 86400;
  rt.hours = diff / 3600;
  diff %= 3600;
  rt.minutes = diff / 60;
  rt.seconds = diff % 60;
  
  return rt;
}
