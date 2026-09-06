#pragma once

#include <Arduino.h>
#include <Preferences.h>

#define MAX_COUNTDOWNS 10

struct Countdown {
  char id[16];           // UUID
  char name[32];         // Event name
  time_t targetDate;     // Target date + time
  bool hasTime;          // Whether time component is set
  uint32_t accentColor;  // 0xRRGGBB
  bool showOnMain;       // Display on main clock screen
  int priority;          // Display order (lower = first)
  bool hideWhenExpired;  // Hide when expired
};

struct Settings {
  char timezone[32];     // POSIX TZ string, e.g., "America/Denver"
  bool use24Hour;        // 24-hour format
  uint8_t layoutId;      // 0=Digital, 1=Minimal, 2=Analog, 3=Focus
  uint8_t themeId;       // Theme preset index
  uint32_t accentColor;  // Global accent color
  bool autoBrightness;   // Auto brightness (future)
  uint8_t brightness;    // 0-255
};

class SettingsManager {
 public:
  SettingsManager();
  bool begin();
  
  // Settings
  const Settings& getSettings() const { return settings_; }
  void setTimezone(const char* tz);
  void setUse24Hour(bool use24);
  void setLayoutId(uint8_t layout);
  void setThemeId(uint8_t theme);
  void setAccentColor(uint32_t color);
  void setBrightness(uint8_t brightness);
  void saveSettings();
  
  // Countdowns
  int getCountdownCount() const { return countdownCount_; }
  const Countdown* getCountdowns() const { return countdowns_; }
  const Countdown* getCountdownById(const char* id) const;
  const Countdown* getNextCountdown() const;
  bool addCountdown(const Countdown& cd);
  bool updateCountdown(const char* id, const Countdown& cd);
  bool deleteCountdown(const char* id);
  void reorderCountdowns(const char* ids[], int count);
  void saveCountdowns();
  
  // Utility
  void resetToDefaults();
  uint32_t getFreeHeap() const;

 private:
  Preferences prefs_;
  Settings settings_;
  Countdown countdowns_[MAX_COUNTDOWNS];
  int countdownCount_ = 0;
  
  void loadSettings();
  void loadCountdowns();
  void generateId(char* id, size_t size);
  static int compareCountdowns(const void* a, const void* b);
};
