#pragma once

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "clock_manager.h"
#include "countdown_manager.h"
#include "settings_manager.h"
#include "touch_map.h"
#include "focus_timer.h"

enum ScreenState {
  SCREEN_CLOCK = 0,
  SCREEN_MENU = 1,
  SCREEN_COUNTDOWNS = 2,
  SCREEN_COUNTDOWN_EDIT = 3,
  SCREEN_SETTINGS = 4,
  SCREEN_WIFI_SETUP = 5,
  SCREEN_ABOUT = 6,
  SCREEN_TIMEZONE = 7
};

enum LayoutId {
  LAYOUT_DIGITAL = 0,
  LAYOUT_MINIMAL = 1,
  LAYOUT_ANALOG = 2,
  LAYOUT_FOCUS = 3,
  LAYOUT_FLIP = 4,
  LAYOUT_PIXEL = 5,
  LAYOUT_SMOOTH = 6,
  LAYOUT_COUNT = 7
};

enum ThemeId {
  THEME_DEFAULT = 0,
  THEME_NORD = 1,
  THEME_DRACULA = 2,
  THEME_SOLARIZED_LIGHT = 3,
  THEME_HIGH_CONTRAST = 4,
  THEME_WARM = 5,
  THEME_COUNT = 6
};

struct ThemeColors {
  uint16_t bg;
  uint16_t panel;
  uint16_t panelAlt;
  uint16_t accent;
  uint16_t text;
  uint16_t textMuted;
  uint16_t textOnAccent;
  uint16_t border;
  uint16_t danger;
};

class WiFiManagerWrapper;

class UIManager {
 public:
  UIManager(SettingsManager& settings, ClockManager& clock, CountdownManager& countdown, FocusTimer& focus);
  void begin(TFT_eSPI& tft, XPT2046_Touchscreen& touch);
  void render(TFT_eSPI& tft, const TouchCal::Sample& touch);
  void loop(TFT_eSPI& tft, XPT2046_Touchscreen& touch);
  
  // Screen navigation
  void showMenu();
  void hideMenu();
  void showCountdowns();
  void showSettings();
  void showAbout();
  void startWifiSetup();
  void backToClock();
  
  // Countdown editing
  void editCountdown(const char* id = nullptr);  // nullptr = new
  void deleteCountdown(const char* id);
  
  // Theme & Layout
  void setLayout(uint8_t layout);
  void setTheme(uint8_t theme);
  void cycleLayout();
  
  // Get current screen
  ScreenState getCurrentScreen() const { return currentScreen_; }
  void setWifiManager(WiFiManagerWrapper* wifi) { wifi_ = wifi; }

 private:
  SettingsManager& settings_;
  ClockManager& clock_;
  CountdownManager& countdown_;
  FocusTimer& focus_;
  XPT2046_Touchscreen* touch_ = nullptr;
  WiFiManagerWrapper* wifi_ = nullptr;
  bool screenNeedsRedraw_ = true;
  int lastSecond_ = -1;
  char lastTimezone_[32] = "";
  uint32_t lastWifiCheck_ = 0;
  char lastWifiDetails_[64] = "";
  uint8_t countdownPage_ = 0;
  int editYear_ = 2026;
  int editMonth_ = 12;
  int editDay_ = 25;
  int editHour_ = 0;
  int editMinute_ = 0;
  int presetNameIndex_ = 0;
  
  ScreenState currentScreen_ = SCREEN_CLOCK;
  ScreenState previousScreen_ = SCREEN_CLOCK;
  bool menuVisible_ = false;
  uint32_t menuShowTime_ = 0;
  
  // Touch handling
  struct TouchState {
    bool pressed = false;
    int16_t x = 0, y = 0;
    int16_t startX = 0, startY = 0;
    int16_t lastX = 0, lastY = 0;
    uint32_t pressTime = 0;
    uint8_t sampleCount = 0;
    bool moved = false;
  } touchState_;
  uint32_t lastTapTime_ = 0;
  
  // Nonblocking backlight transitions avoid allocating full-screen framebuffers.
  enum class MotionPhase { Idle, FadeOut, FadeIn };
  MotionPhase motionPhase_ = MotionPhase::Idle;
  uint32_t motionStarted_ = 0;
  uint32_t renderedView_ = UINT32_MAX;
  bool waitForTouchRelease_ = false;
  float displayedBrightness_ = 0;
  float brightnessFrom_ = 0;
  uint8_t brightnessTarget_ = 0;
  uint32_t brightnessStarted_ = 0;
  int lastBacklight_ = -1;
  void renderContent(TFT_eSPI& tft, const TouchCal::Sample& touch);
  void updateBacklight(uint32_t now, float visibility);

  // Countdown edit state
  char editingCountdownId_[16] = "";
  char editName_[32] = "";
  time_t editTargetDate_ = 0;
  bool editHasTime_ = false;
  uint32_t editAccentColor_ = 0x00FFFF;
  bool editShowOnMain_ = true;
  bool editHideWhenExpired_ = false;
  uint8_t editField_ = 0;  // 0=name, 1=date, 2=time, 3=color, 4=showOnMain, 5=hideWhenExpired
  
  // Settings UI state
  uint8_t timezonePage_ = 0;
  uint8_t settingsSection_ = 0;  // 0=wifi, 1=time, 2=display, 3=about
  
  // Theme colors
  ThemeColors currentTheme_;
  
  // Layout rendering
  void renderClock(TFT_eSPI& tft, const TouchCal::Sample& touch);
  void renderMenu(TFT_eSPI& tft);
  void renderCountdowns(TFT_eSPI& tft, const TouchCal::Sample& touch);
  void renderCountdownEdit(TFT_eSPI& tft, const TouchCal::Sample& touch);
  void renderSettings(TFT_eSPI& tft, const TouchCal::Sample& touch);
  void renderTimezone(TFT_eSPI& tft);
  void renderAbout(TFT_eSPI& tft);
  void renderWifiSetup(TFT_eSPI& tft);
  
  void drawClockChrome(TFT_eSPI& tft, const char* label);
  void drawNextEvent(TFT_eSPI& tft, int y);
  void drawPageHeader(TFT_eSPI& tft, const char* eyebrow, const char* title,
                      const char* action = nullptr);
  void drawSettingValue(TFT_eSPI& tft, int y, const char* label, const char* value);

  // Layout-specific clock rendering
  void renderDigitalLayout(TFT_eSPI& tft);
  void renderMinimalLayout(TFT_eSPI& tft);
  void renderAnalogLayout(TFT_eSPI& tft);
  void renderFocusLayout(TFT_eSPI& tft);
  void renderFlipLayout(TFT_eSPI& tft);
  void renderPixelLayout(TFT_eSPI& tft);
  void renderSmoothLayout(TFT_eSPI& tft);
  
  // Touch handling
  void handleTouch(TFT_eSPI& tft, const TouchCal::Sample& touch);
  void handleClockTouch(const TouchCal::Sample& touch);
  void handleMenuTouch(const TouchCal::Sample& touch);
  void handleCountdownsTouch(const TouchCal::Sample& touch);
  void handleCountdownEditTouch(const TouchCal::Sample& touch);
  void handleSettingsTouch(const TouchCal::Sample& touch);
  void handleTimezoneTouch(const TouchCal::Sample& touch);
  void handleAboutTouch(const TouchCal::Sample& touch);
  void handleWifiSetupTouch(const TouchCal::Sample& touch);
  
  // UI primitives
  void drawRoundedRect(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
  void fillRoundedRect(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
  void drawButton(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, const char* label, uint16_t bg, uint16_t textColor, bool pressed = false);
  bool isInRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh);
  
  // Theme
  void applyTheme(uint8_t themeId);
  uint16_t rgbTo565(uint32_t rgb);
  uint32_t blendColors(uint32_t c1, uint32_t c2, float ratio);
  
  // Time formatting helpers
  void formatCountdownRemaining(const Countdown* cd, char* buffer, size_t size, bool shortFormat = false);
};
