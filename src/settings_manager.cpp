#include "settings_manager.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <cstring>
#include <cstdlib>
#include <ctime>

SettingsManager::SettingsManager() {
  memset(&settings_, 0, sizeof(settings_));
  strlcpy(settings_.timezone, "America/Denver", sizeof(settings_.timezone));
  settings_.use24Hour = false;
  settings_.layoutId = 0;
  settings_.themeId = 0;
  settings_.accentColor = 0x00FFFF;
  settings_.autoBrightness = false;
  settings_.brightness = 200;
  memset(countdowns_, 0, sizeof(countdowns_));
  countdownCount_ = 0;
}

bool SettingsManager::begin() {
  if (!prefs_.begin("cyd-clock", false)) {
    return false;
  }
  loadSettings();
  loadCountdowns();
  return true;
}

void SettingsManager::loadSettings() {
  // Default settings
  strlcpy(settings_.timezone, "America/Denver", sizeof(settings_.timezone));
  settings_.use24Hour = false;
  settings_.layoutId = 0;
  settings_.themeId = 0;
  settings_.accentColor = 0x00FFFF;  // Cyan
  settings_.autoBrightness = false;
  settings_.brightness = 200;

  // Try to load from NVS
  size_t len = prefs_.getBytesLength("settings");
  if (len == sizeof(Settings)) {
    prefs_.getBytes("settings", &settings_, sizeof(Settings));
  } else {
    saveSettings();  // Save defaults
  }
}

void SettingsManager::loadCountdowns() {
  countdownCount_ = 0;
  String json = prefs_.getString("countdowns", "");
  if (json.length() > 0) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (!err && doc.is<JsonArray>()) {
      JsonArray arr = doc.as<JsonArray>();
      for (JsonObject obj : arr) {
        if (countdownCount_ >= MAX_COUNTDOWNS) break;
        Countdown& cd = countdowns_[countdownCount_];
        strlcpy(cd.id, obj["id"] | "", sizeof(cd.id));
        strlcpy(cd.name, obj["name"] | "", sizeof(cd.name));
        cd.targetDate = obj["targetDate"] | 0;
        cd.hasTime = obj["hasTime"] | false;
        cd.accentColor = obj["accentColor"] | 0x00FFFF;
        cd.showOnMain = obj["showOnMain"] | true;
        cd.priority = obj["priority"] | countdownCount_;
        cd.hideWhenExpired = obj["hideWhenExpired"] | false;
        countdownCount_++;
      }
      // Sort by priority
      qsort(countdowns_, countdownCount_, sizeof(Countdown), compareCountdowns);
    }
  }
}

void SettingsManager::saveSettings() {
  prefs_.putBytes("settings", &settings_, sizeof(Settings));
}

void SettingsManager::saveCountdowns() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < countdownCount_; i++) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = countdowns_[i].id;
    obj["name"] = countdowns_[i].name;
    obj["targetDate"] = countdowns_[i].targetDate;
    obj["hasTime"] = countdowns_[i].hasTime;
    obj["accentColor"] = countdowns_[i].accentColor;
    obj["showOnMain"] = countdowns_[i].showOnMain;
    obj["priority"] = countdowns_[i].priority;
    obj["hideWhenExpired"] = countdowns_[i].hideWhenExpired;
  }
  String json;
  serializeJson(doc, json);
  prefs_.putString("countdowns", json);
}

void SettingsManager::setTimezone(const char* tz) {
  strlcpy(settings_.timezone, tz, sizeof(settings_.timezone));
  saveSettings();
  setenv("TZ", tz, 1);
  tzset();
}

void SettingsManager::setUse24Hour(bool use24) {
  settings_.use24Hour = use24;
  saveSettings();
}

void SettingsManager::setLayoutId(uint8_t layout) {
  settings_.layoutId = layout;
  saveSettings();
}

void SettingsManager::setThemeId(uint8_t theme) {
  settings_.themeId = theme;
  saveSettings();
}

void SettingsManager::setAccentColor(uint32_t color) {
  settings_.accentColor = color;
  saveSettings();
}

void SettingsManager::setBrightness(uint8_t brightness) {
  settings_.brightness = brightness;
  saveSettings();
}

const Countdown* SettingsManager::getCountdownById(const char* id) const {
  for (int i = 0; i < countdownCount_; i++) {
    if (strcmp(countdowns_[i].id, id) == 0) {
      return &countdowns_[i];
    }
  }
  return nullptr;
}

const Countdown* SettingsManager::getNextCountdown() const {
  time_t now = time(nullptr);
  const Countdown* next = nullptr;
  time_t minDiff = 0;
  
  for (int i = 0; i < countdownCount_; i++) {
    const Countdown& cd = countdowns_[i];
    if (cd.targetDate <= now) continue;  // Skip expired
    
    time_t diff = cd.targetDate - now;
    if (next == nullptr || diff < minDiff) {
      next = &cd;
      minDiff = diff;
    }
  }
  return next;
}

bool SettingsManager::addCountdown(const Countdown& cd) {
  if (countdownCount_ >= MAX_COUNTDOWNS) return false;
  
  Countdown newCd = cd;
  generateId(newCd.id, sizeof(newCd.id));
  newCd.priority = countdownCount_;
  
  countdowns_[countdownCount_++] = newCd;
  saveCountdowns();
  return true;
}

bool SettingsManager::updateCountdown(const char* id, const Countdown& cd) {
  for (int i = 0; i < countdownCount_; i++) {
    if (strcmp(countdowns_[i].id, id) == 0) {
      Countdown updated = cd;
      strlcpy(updated.id, id, sizeof(updated.id));
      countdowns_[i] = updated;
      qsort(countdowns_, countdownCount_, sizeof(Countdown), compareCountdowns);
      saveCountdowns();
      return true;
    }
  }
  return false;
}

bool SettingsManager::deleteCountdown(const char* id) {
  for (int i = 0; i < countdownCount_; i++) {
    if (strcmp(countdowns_[i].id, id) == 0) {
      // Shift remaining
      for (int j = i; j < countdownCount_ - 1; j++) {
        countdowns_[j] = countdowns_[j + 1];
      }
      countdownCount_--;
      saveCountdowns();
      return true;
    }
  }
  return false;
}

void SettingsManager::reorderCountdowns(const char* ids[], int count) {
  Countdown temp[MAX_COUNTDOWNS];
  int idx = 0;
  for (int i = 0; i < count && idx < MAX_COUNTDOWNS; i++) {
    const Countdown* cd = getCountdownById(ids[i]);
    if (cd) {
      temp[idx++] = *cd;
    }
  }
  // Add any not in the list
  for (int i = 0; i < countdownCount_ && idx < MAX_COUNTDOWNS; i++) {
    bool found = false;
    for (int j = 0; j < count; j++) {
      if (strcmp(countdowns_[i].id, ids[j]) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      temp[idx++] = countdowns_[i];
    }
  }
  countdownCount_ = idx;
  memcpy(countdowns_, temp, sizeof(Countdown) * countdownCount_);
  // Update priorities
  for (int i = 0; i < countdownCount_; i++) {
    countdowns_[i].priority = i;
  }
  saveCountdowns();
}

void SettingsManager::resetToDefaults() {
  prefs_.clear();
  loadSettings();
  countdownCount_ = 0;
  saveCountdowns();
}

uint32_t SettingsManager::getFreeHeap() const {
  return ESP.getFreeHeap();
}

void SettingsManager::generateId(char* id, size_t size) {
  // Simple UUID-like generator
  uint32_t r = esp_random();
  snprintf(id, size, "%08lx", r);
}

int SettingsManager::compareCountdowns(const void* a, const void* b) {
  const Countdown* ca = static_cast<const Countdown*>(a);
  const Countdown* cb = static_cast<const Countdown*>(b);
  
  time_t now = time(nullptr);
  bool aExpired = ca->targetDate <= now;
  bool bExpired = cb->targetDate <= now;
  
  if (aExpired && !bExpired) return 1;
  if (!aExpired && bExpired) return -1;
  if (aExpired && bExpired) return ca->priority - cb->priority;
  
  // Both not expired - sort by target date
  if (ca->targetDate < cb->targetDate) return -1;
  if (ca->targetDate > cb->targetDate) return 1;
  return ca->priority - cb->priority;
}
