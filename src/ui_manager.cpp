#include "ui_manager.h"
#include "wifi_manager.h"
#include "pins.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

constexpr int16_t kWidth = TouchCal::kScreenWidth;
constexpr int16_t kHeight = TouchCal::kScreenHeight;

const char* const kPresetNames[] = {
  "Christmas", "New Year", "Birthday", "Vacation", 
  "Anniversary", "Holiday", "Halloween", "Thanksgiving", 
  "Wedding", "Graduation", "Party", "Launch", "Deadline"
};
constexpr int kNumPresetNames = sizeof(kPresetNames) / sizeof(kPresetNames[0]);

const char* const kMonthNames[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

const char* const kLayoutNames[] = {
  "Digital", "Minimal", "Analog", "Focus"
};

const char* const kThemeNames[] = {
  "Default", "Nord", "Dracula", "Solarized Light", "High Contrast", "Warm"
};

}  // namespace

UIManager::UIManager(SettingsManager& settings, ClockManager& clock, CountdownManager& countdown)
    : settings_(settings), clock_(clock), countdown_(countdown) {
  memset(&currentTheme_, 0, sizeof(currentTheme_));
  memset(&touchState_, 0, sizeof(touchState_));
}

void UIManager::begin(TFT_eSPI& tft, XPT2046_Touchscreen& touch) {
  touch_ = &touch;
  applyTheme(settings_.getSettings().themeId);
  screenNeedsRedraw_ = true;
  lastSecond_ = -1;
  currentScreen_ = SCREEN_CLOCK;
  menuVisible_ = false;

  tft.fillScreen(currentTheme_.bg);

  // Splash message
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
  tft.setTextFont(4);
  tft.drawString("CYD Smart Clock", kWidth / 2, kHeight / 2 - 20);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.setTextFont(2);
  tft.drawString("Starting up...", kWidth / 2, kHeight / 2 + 20);
}

void UIManager::loop(TFT_eSPI& tft, XPT2046_Touchscreen& touch) {
  TouchCal::Sample sample;
  const bool pressed = touch.tirqTouched() && touch.touched();
  if (pressed) {
    const TS_Point raw = touch.getPoint();
    sample = TouchCal::mapRaw(raw.x, raw.y, raw.z, true);
  }
  render(tft, sample);
}

void UIManager::render(TFT_eSPI& tft, const TouchCal::Sample& touch) {
  handleTouch(tft, touch);

  if (menuVisible_) {
    if (screenNeedsRedraw_) {
      screenNeedsRedraw_ = false;
      renderMenu(tft);
    }
    return;
  }

  if (currentScreen_ == SCREEN_CLOCK) {
    int second = clock_.getSecond();
    if (screenNeedsRedraw_ || second != lastSecond_) {
      if (screenNeedsRedraw_) {
        tft.fillScreen(currentTheme_.bg);
        screenNeedsRedraw_ = false;
      }
      lastSecond_ = second;
      renderClock(tft, touch);
    }
    return;
  }

  // All other screens (SETTINGS, COUNTDOWNS, COUNTDOWN_EDIT, ABOUT, WIFI_SETUP)
  // are redrawn ONCE when screenNeedsRedraw_ is true, eliminating all flickering
  if (screenNeedsRedraw_) {
    screenNeedsRedraw_ = false;
    tft.fillScreen(currentTheme_.bg);
    switch (currentScreen_) {
      case SCREEN_SETTINGS:
        renderSettings(tft, touch);
        break;
      case SCREEN_COUNTDOWNS:
        renderCountdowns(tft, touch);
        break;
      case SCREEN_COUNTDOWN_EDIT:
        renderCountdownEdit(tft, touch);
        break;
      case SCREEN_ABOUT:
        renderAbout(tft);
        break;
      case SCREEN_WIFI_SETUP:
        renderWifiSetup(tft);
        break;
      default:
        renderClock(tft, touch);
        break;
    }
  }
}

void UIManager::showMenu() {
  menuVisible_ = true;
  menuShowTime_ = millis();
  screenNeedsRedraw_ = true;
}

void UIManager::hideMenu() {
  menuVisible_ = false;
  screenNeedsRedraw_ = true;
}

void UIManager::showCountdowns() {
  menuVisible_ = false;
  currentScreen_ = SCREEN_COUNTDOWNS;
  countdownPage_ = 0;
  screenNeedsRedraw_ = true;
}

void UIManager::showSettings() {
  menuVisible_ = false;
  currentScreen_ = SCREEN_SETTINGS;
  screenNeedsRedraw_ = true;
}

void UIManager::showAbout() {
  menuVisible_ = false;
  currentScreen_ = SCREEN_ABOUT;
  screenNeedsRedraw_ = true;
}

void UIManager::startWifiSetup() {
  menuVisible_ = false;
  currentScreen_ = SCREEN_WIFI_SETUP;
  screenNeedsRedraw_ = true;
  if (wifi_) {
    wifi_->startConfigPortal();
  }
}

void UIManager::backToClock() {
  menuVisible_ = false;
  currentScreen_ = SCREEN_CLOCK;
  screenNeedsRedraw_ = true;
}

void UIManager::setLayout(uint8_t layout) {
  settings_.setLayoutId(layout % LAYOUT_COUNT);
  screenNeedsRedraw_ = true;
}

void UIManager::setTheme(uint8_t theme) {
  settings_.setThemeId(theme % THEME_COUNT);
  applyTheme(theme % THEME_COUNT);
  screenNeedsRedraw_ = true;
}

void UIManager::cycleLayout() {
  uint8_t current = settings_.getSettings().layoutId;
  setLayout((current + 1) % LAYOUT_COUNT);
}

void UIManager::editCountdown(const char* id) {
  currentScreen_ = SCREEN_COUNTDOWN_EDIT;
  screenNeedsRedraw_ = true;

  if (id != nullptr && strlen(id) > 0) {
    const Countdown* cd = settings_.getCountdownById(id);
    if (cd != nullptr) {
      strlcpy(editingCountdownId_, cd->id, sizeof(editingCountdownId_));
      strlcpy(editName_, cd->name, sizeof(editName_));
      editTargetDate_ = cd->targetDate;
      editHasTime_ = cd->hasTime;
      editAccentColor_ = cd->accentColor;
      editShowOnMain_ = cd->showOnMain;
      editHideWhenExpired_ = cd->hideWhenExpired;

      struct tm tmVal;
      localtime_r(&editTargetDate_, &tmVal);
      editYear_ = tmVal.tm_year + 1900;
      editMonth_ = tmVal.tm_mon + 1;
      editDay_ = tmVal.tm_mday;
      editHour_ = tmVal.tm_hour;
      editMinute_ = tmVal.tm_min;
      return;
    }
  }

  // Create new countdown defaults
  editingCountdownId_[0] = '\0';
  presetNameIndex_ = 0;
  strlcpy(editName_, kPresetNames[0], sizeof(editName_));
  
  time_t now = time(nullptr);
  struct tm tmVal;
  localtime_r(&now, &tmVal);
  editYear_ = tmVal.tm_year + 1900;
  editMonth_ = 12;
  editDay_ = 25;
  editHour_ = 0;
  editMinute_ = 0;
  editHasTime_ = false;
  editAccentColor_ = 0x00FFFF;
  editShowOnMain_ = true;
  editHideWhenExpired_ = false;
}

void UIManager::deleteCountdown(const char* id) {
  if (id != nullptr && strlen(id) > 0) {
    settings_.deleteCountdown(id);
  }
  showCountdowns();
}

void UIManager::handleTouch(TFT_eSPI& tft, const TouchCal::Sample& touch) {
  if (touch.pressed) {
    if (!touchState_.pressed) {
      // Touch begins
      touchState_.pressed = true;
      touchState_.startX = touch.x;
      touchState_.startY = touch.y;
      touchState_.lastX = touch.x;
      touchState_.lastY = touch.y;
      touchState_.pressTime = millis();
      touchState_.sampleCount = 1;
      touchState_.moved = false;
    } else {
      // Touch ongoing: track stable coordinates
      touchState_.lastX = touch.x;
      touchState_.lastY = touch.y;
      touchState_.sampleCount++;
      if (abs(touch.x - touchState_.startX) > 40 || abs(touch.y - touchState_.startY) > 40) {
        touchState_.moved = true;
      }
    }
  } else {
    if (touchState_.pressed) {
      // Touch released: process tap using the stable position before release
      touchState_.pressed = false;
      int16_t dx = touchState_.lastX - touchState_.startX;
      int16_t dy = touchState_.lastY - touchState_.startY;

      // Check swipe left/right (only on main clock screen when menu not open)
      if (touchState_.moved && abs(dx) > 60 && abs(dx) > 2 * abs(dy)) {
        if (currentScreen_ == SCREEN_CLOCK && !menuVisible_) {
          cycleLayout();
          return;
        }
      }

      // Process tap with debounce
      if (touchState_.sampleCount >= 1) {
        uint32_t now = millis();
        if (now - lastTapTime_ > 180) {
          lastTapTime_ = now;
          TouchCal::Sample tap;
          tap.x = touchState_.lastX;
          tap.y = touchState_.lastY;
          tap.pressed = true;

          Serial.printf("[Touch] Tap at x=%d, y=%d (screen=%d, menu=%d)\n", tap.x, tap.y, currentScreen_, menuVisible_);

          if (menuVisible_) {
            handleMenuTouch(tap);
          } else {
            switch (currentScreen_) {
              case SCREEN_CLOCK:
                handleClockTouch(tap);
                break;
              case SCREEN_SETTINGS:
                handleSettingsTouch(tap);
                break;
              case SCREEN_COUNTDOWNS:
                handleCountdownsTouch(tap);
                break;
              case SCREEN_COUNTDOWN_EDIT:
                handleCountdownEditTouch(tap);
                break;
              case SCREEN_ABOUT:
                handleAboutTouch(tap);
                break;
              case SCREEN_WIFI_SETUP:
                handleWifiSetupTouch(tap);
                break;
              default:
                backToClock();
                break;
            }
          }
        }
      }
    }
  }
}

void UIManager::handleClockTouch(const TouchCal::Sample& touch) {
  showMenu();
}

void UIManager::handleMenuTouch(const TouchCal::Sample& touch) {
  // Close [X] button or tap outside menu box
  if (touch.x < 60 || touch.x > 420 || touch.y < 25 || touch.y > 295 || (touch.x >= 350 && touch.y <= 68)) {
    hideMenu();
    return;
  }

  // COUNTDOWNS button: y: 70..128
  if (touch.y >= 70 && touch.y <= 128) {
    showCountdowns();
    return;
  }

  // SETTINGS button: y: 129..186
  if (touch.y >= 129 && touch.y <= 186) {
    showSettings();
    return;
  }

  // SWITCH LAYOUT button: y: 187..242
  if (touch.y >= 187 && touch.y <= 242) {
    cycleLayout();
    hideMenu();
    return;
  }

  // CLOSE button: y: 243..295
  if (touch.y >= 243) {
    hideMenu();
    return;
  }
}

void UIManager::handleCountdownsTouch(const TouchCal::Sample& touch) {
  // Back button: top-left area
  if (touch.x <= 130 && touch.y <= 55) {
    backToClock();
    return;
  }

  // Add button: top-right area
  if (touch.x >= 350 && touch.y <= 55) {
    editCountdown(nullptr);
    return;
  }

  int total = settings_.getCountdownCount();
  const Countdown* cds = settings_.getCountdowns();

  int startIndex = countdownPage_ * 3;
  for (int i = 0; i < 3 && (startIndex + i) < total; i++) {
    int idx = startIndex + i;
    int y = 60 + i * 80;

    // Delete [X] button
    if (touch.x >= 390 && touch.y >= y && touch.y <= (y + 76)) {
      deleteCountdown(cds[idx].id);
      return;
    }

    // Card click -> edit
    if (touch.x < 390 && touch.y >= y && touch.y <= (y + 76)) {
      editCountdown(cds[idx].id);
      return;
    }
  }

  // Paging controls if more than 3
  if (total > 3) {
    if (touch.x <= 130 && touch.y >= 265) {
      if (countdownPage_ > 0) {
        countdownPage_--;
        screenNeedsRedraw_ = true;
      }
    } else if (touch.x >= 350 && touch.y >= 265) {
      if ((countdownPage_ + 1) * 3 < total) {
        countdownPage_++;
        screenNeedsRedraw_ = true;
      }
    }
  }
}

void UIManager::handleCountdownEditTouch(const TouchCal::Sample& touch) {
  // Cancel button: top-left area
  if (touch.x <= 130 && touch.y <= 55) {
    showCountdowns();
    return;
  }

  // Save button: top-right area
  if (touch.x >= 350 && touch.y <= 55) {
    struct tm targetTm = {};
    targetTm.tm_year = editYear_ - 1900;
    targetTm.tm_mon = editMonth_ - 1;
    targetTm.tm_mday = editDay_;
    targetTm.tm_hour = editHasTime_ ? editHour_ : 0;
    targetTm.tm_min = editHasTime_ ? editMinute_ : 0;
    targetTm.tm_sec = 0;
    targetTm.tm_isdst = -1;
    time_t targetTime = mktime(&targetTm);

    Countdown cd = {};
    strlcpy(cd.name, editName_, sizeof(cd.name));
    cd.targetDate = targetTime;
    cd.hasTime = editHasTime_;
    cd.accentColor = editAccentColor_;
    cd.showOnMain = editShowOnMain_;
    cd.hideWhenExpired = editHideWhenExpired_;

    if (strlen(editingCountdownId_) > 0) {
      settings_.updateCountdown(editingCountdownId_, cd);
    } else {
      settings_.addCountdown(cd);
    }
    showCountdowns();
    return;
  }

  // Preset name prev/next (row 1: y: 60..100)
  if (touch.y >= 60 && touch.y <= 100) {
    if (touch.x < 160) {
      presetNameIndex_ = (presetNameIndex_ - 1 + kNumPresetNames) % kNumPresetNames;
      strlcpy(editName_, kPresetNames[presetNameIndex_], sizeof(editName_));
      screenNeedsRedraw_ = true;
      return;
    }
    if (touch.x > 330) {
      presetNameIndex_ = (presetNameIndex_ + 1) % kNumPresetNames;
      strlcpy(editName_, kPresetNames[presetNameIndex_], sizeof(editName_));
      screenNeedsRedraw_ = true;
      return;
    }
  }

  // Date row (y: 101..148)
  if (touch.y >= 101 && touch.y <= 148) {
    // Year - / +
    if (touch.x >= 80 && touch.x <= 145) {
      if (editYear_ > 2024) editYear_--;
      screenNeedsRedraw_ = true;
      return;
    }
    if (touch.x >= 165 && touch.x <= 220) {
      if (editYear_ < 2050) editYear_++;
      screenNeedsRedraw_ = true;
      return;
    }
    // Month - / +
    if (touch.x >= 225 && touch.x <= 270) {
      editMonth_ = editMonth_ > 1 ? editMonth_ - 1 : 12;
      screenNeedsRedraw_ = true;
      return;
    }
    if (touch.x >= 295 && touch.x <= 340) {
      editMonth_ = editMonth_ < 12 ? editMonth_ + 1 : 1;
      screenNeedsRedraw_ = true;
      return;
    }
    // Day - / +
    if (touch.x >= 345 && touch.x <= 385) {
      editDay_ = editDay_ > 1 ? editDay_ - 1 : 31;
      screenNeedsRedraw_ = true;
      return;
    }
    if (touch.x >= 405) {
      editDay_ = editDay_ < 31 ? editDay_ + 1 : 1;
      screenNeedsRedraw_ = true;
      return;
    }
  }

  // Time row (y: 149..194)
  if (touch.y >= 149 && touch.y <= 194) {
    if (touch.x >= 80 && touch.x <= 135) {
      editHour_ = (editHour_ - 1 + 24) % 24;
      screenNeedsRedraw_ = true;
      return;
    }
    if (touch.x >= 155 && touch.x <= 200) {
      editHour_ = (editHour_ + 1) % 24;
      screenNeedsRedraw_ = true;
      return;
    }
    if (touch.x >= 201 && touch.x <= 245) {
      editMinute_ = (editMinute_ - 5 + 60) % 60;
      screenNeedsRedraw_ = true;
      return;
    }
    if (touch.x >= 265 && touch.x <= 310) {
      editMinute_ = (editMinute_ + 5) % 60;
      screenNeedsRedraw_ = true;
      return;
    }
    if (touch.x >= 311) {
      editHasTime_ = !editHasTime_;
      screenNeedsRedraw_ = true;
      return;
    }
  }

  // Show on Main toggle (y: 195..244)
  if (touch.y >= 195 && touch.y <= 244) {
    editShowOnMain_ = !editShowOnMain_;
    screenNeedsRedraw_ = true;
    return;
  }

  // Delete button (y >= 245)
  if (strlen(editingCountdownId_) > 0 && touch.y >= 245 && touch.x >= 100 && touch.x <= 380) {
    deleteCountdown(editingCountdownId_);
    return;
  }
}

void UIManager::handleSettingsTouch(const TouchCal::Sample& touch) {
  // Back button: top-left area (visual: 16, 12, 90, 36)
  if (touch.x <= 130 && touch.y <= 55) {
    backToClock();
    return;
  }

  // Row 1: Time format (visual: y=60, h=38)
  if (touch.y >= 50 && touch.y <= 95 && touch.x >= 180) {
    settings_.setUse24Hour(!settings_.getSettings().use24Hour);
    screenNeedsRedraw_ = true;
    return;
  }

  // Row 2: Layout cycle (visual: y=106, h=38)
  if (touch.y >= 96 && touch.y <= 140 && touch.x >= 180) {
    cycleLayout();
    screenNeedsRedraw_ = true;
    return;
  }

  // Row 3: Theme cycle (visual: y=152, h=38)
  if (touch.y >= 141 && touch.y <= 185 && touch.x >= 180) {
    setTheme((settings_.getSettings().themeId + 1) % THEME_COUNT);
    return;
  }

  // Row 4: Brightness (visual: y=198, h=38)
  if (touch.y >= 186 && touch.y <= 240) {
    if (touch.x >= 200 && touch.x <= 330) {
      uint8_t cur = settings_.getSettings().brightness;
      uint8_t next = cur > 35 ? cur - 35 : 20;
      settings_.setBrightness(next);
      analogWrite(Pins::kTftBl, next);
      screenNeedsRedraw_ = true;
      return;
    }
    if (touch.x >= 350 && touch.x <= 470) {
      uint8_t cur = settings_.getSettings().brightness;
      uint8_t next = cur < 220 ? cur + 35 : 255;
      settings_.setBrightness(next);
      analogWrite(Pins::kTftBl, next);
      screenNeedsRedraw_ = true;
      return;
    }
  }

  // Row 5: Bottom action buttons (visual: y=252, h=42)
  if (touch.y >= 241) {
    if (touch.x < 240) {
      startWifiSetup();
      return;
    } else {
      showAbout();
      return;
    }
  }
}

void UIManager::handleWifiSetupTouch(const TouchCal::Sample& touch) {
  // Back button: top-left area or tap anywhere
  showSettings();
}

void UIManager::handleAboutTouch(const TouchCal::Sample& touch) {
  // Back button or tap anywhere
  showSettings();
}

void UIManager::renderClock(TFT_eSPI& tft, const TouchCal::Sample& touch) {
  uint8_t layout = settings_.getSettings().layoutId;
  switch (layout) {
    case LAYOUT_MINIMAL:
      renderMinimalLayout(tft);
      break;
    case LAYOUT_ANALOG:
      renderAnalogLayout(tft);
      break;
    case LAYOUT_FOCUS:
      renderFocusLayout(tft);
      break;
    case LAYOUT_DIGITAL:
    default:
      renderDigitalLayout(tft);
      break;
  }
}

void UIManager::renderDigitalLayout(TFT_eSPI& tft) {
  // Status bar
  tft.fillRect(16, 8, kWidth - 32, 22, currentTheme_.bg);
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  bool connected = wifi_ && wifi_->isConnected();
  bool apActive = wifi_ && wifi_->isAPActive();

  if (connected) {
    tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
    tft.drawString(wifi_->getIP(), 18, 18);
  } else if (apActive) {
    tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
    tft.drawString("AP: CYD-Clock-Setup (192.168.4.1)", 18, 18);
  } else {
    tft.setTextColor(currentTheme_.danger, currentTheme_.bg);
    tft.drawString("WiFi: Offline", 18, 18);
  }

  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(clock_.isTimeSynced() ? currentTheme_.textMuted : currentTheme_.danger, currentTheme_.bg);
  tft.drawString(clock_.isTimeSynced() ? clock_.getTimezone() : "Time: Unsynced", kWidth - 18, 18);

  // Clock panel
  fillRoundedRect(tft, 16, 36, kWidth - 32, 138, 12, currentTheme_.panel);

  // Time display
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  const Settings& s = settings_.getSettings();

  char timeBuf[16];
  if (s.use24Hour) {
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", clock_.getHour(), clock_.getMinute(), clock_.getSecond());
    tft.setTextFont(7);
    tft.drawString(timeBuf, kWidth / 2, 86);
  } else {
    int h = clock_.getHour();
    bool isPm = h >= 12;
    h = h % 12;
    if (h == 0) h = 12;
    snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d:%02d", h, clock_.getMinute(), clock_.getSecond());
    tft.setTextFont(7);
    tft.drawString(timeBuf, kWidth / 2 - 25, 86);
    tft.setTextFont(4);
    tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
    tft.drawString(isPm ? "PM" : "AM", kWidth / 2 + 120, 78);
  }

  // Date display
  char dateBuf[48];
  char weekBuf[24];
  clock_.formatDate(dateBuf, sizeof(dateBuf));
  clock_.formatWeekday(weekBuf, sizeof(weekBuf));
  char fullDate[72];
  snprintf(fullDate, sizeof(fullDate), "%s, %s", weekBuf, dateBuf);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.setTextFont(4);
  tft.drawString(fullDate, kWidth / 2, 142);

  // Countdown card
  fillRoundedRect(tft, 16, 186, kWidth - 32, 118, 12, currentTheme_.panel);

  const Countdown* cd = settings_.getNextCountdown();
  if (cd != nullptr) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(cd->accentColor != 0 ? rgbTo565(cd->accentColor) : currentTheme_.accent, currentTheme_.panel);
    tft.setTextFont(4);
    tft.drawString(cd->name, kWidth / 2, 212);

    char remBuf[64];
    countdown_.formatRemaining(cd->targetDate, remBuf, sizeof(remBuf), true);
    tft.setTextColor(currentTheme_.text, currentTheme_.panel);
    tft.setTextFont(4);
    tft.drawString(remBuf, kWidth / 2, 250);

    struct tm targetTm;
    localtime_r(&cd->targetDate, &targetTm);
    char targetStr[48];
    snprintf(targetStr, sizeof(targetStr), "Target: %s %d, %d", kMonthNames[targetTm.tm_mon], targetTm.tm_mday, targetTm.tm_year + 1900);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.setTextFont(2);
    tft.drawString(targetStr, kWidth / 2, 282);
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.setTextFont(4);
    tft.drawString("No Active Countdowns", kWidth / 2, 232);
    tft.setTextFont(2);
    tft.drawString("Tap screen to open menu and add an event", kWidth / 2, 264);
  }
}

void UIManager::renderMinimalLayout(TFT_eSPI& tft) {
  fillRoundedRect(tft, 16, 16, kWidth - 32, 160, 12, currentTheme_.panel);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.setTextFont(7);

  char timeBuf[16];
  const Settings& s = settings_.getSettings();
  if (s.use24Hour) {
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", clock_.getHour(), clock_.getMinute(), clock_.getSecond());
    tft.drawString(timeBuf, kWidth / 2, 70);
  } else {
    int h = clock_.getHour();
    bool isPm = h >= 12;
    h = h % 12;
    if (h == 0) h = 12;
    snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d:%02d", h, clock_.getMinute(), clock_.getSecond());
    tft.drawString(timeBuf, kWidth / 2 - 25, 70);
    tft.setTextFont(4);
    tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
    tft.drawString(isPm ? "PM" : "AM", kWidth / 2 + 120, 62);
  }

  char dateBuf[48];
  char weekBuf[24];
  clock_.formatDate(dateBuf, sizeof(dateBuf));
  clock_.formatWeekday(weekBuf, sizeof(weekBuf));
  char fullDate[72];
  snprintf(fullDate, sizeof(fullDate), "%s  ·  %s", weekBuf, dateBuf);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.setTextFont(4);
  tft.drawString(fullDate, kWidth / 2, 136);

  // Divider
  tft.drawFastHLine(30, 188, kWidth - 60, currentTheme_.border);

  // Countdown card
  fillRoundedRect(tft, 16, 200, kWidth - 32, 104, 12, currentTheme_.panel);

  const Countdown* cd = settings_.getNextCountdown();
  if (cd != nullptr) {
    char remShort[32];
    countdown_.formatRemainingShort(cd->targetDate, remShort, sizeof(remShort));
    char line[72];
    snprintf(line, sizeof(line), "%s  ·  %s", cd->name, remShort);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
    tft.setTextFont(4);
    tft.drawString(line, kWidth / 2, 235);

    char remFull[64];
    countdown_.formatRemaining(cd->targetDate, remFull, sizeof(remFull), true);
    tft.setTextColor(currentTheme_.text, currentTheme_.panel);
    tft.setTextFont(2);
    tft.drawString(remFull, kWidth / 2, 272);
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.setTextFont(2);
    tft.drawString("Minimal Layout  ·  Tap to open menu", kWidth / 2, 252);
  }
}

void UIManager::renderAnalogLayout(TFT_eSPI& tft) {
  int cx = 135;
  int cy = 160;
  int r = 105;

  // Draw analog clock dial
  tft.fillCircle(cx, cy, r, currentTheme_.panel);
  tft.drawCircle(cx, cy, r, currentTheme_.border);
  tft.drawCircle(cx, cy, r - 1, currentTheme_.border);

  // Hour tick marks
  for (int i = 0; i < 12; i++) {
    float angle = i * 30.0f * DEG_TO_RAD;
    int x1 = cx + (r - 12) * sinf(angle);
    int y1 = cy - (r - 12) * cosf(angle);
    int x2 = cx + (r - 4) * sinf(angle);
    int y2 = cy - (r - 4) * cosf(angle);
    tft.drawLine(x1, y1, x2, y2, currentTheme_.text);
  }

  // Hands
  float hAngle = ((clock_.getHour() % 12) + clock_.getMinute() / 60.0f) * 30.0f * DEG_TO_RAD;
  int hx = cx + 55 * sinf(hAngle);
  int hy = cy - 55 * cosf(hAngle);
  tft.drawLine(cx, cy, hx, hy, currentTheme_.text);
  tft.drawLine(cx + 1, cy, hx + 1, hy, currentTheme_.text);

  float mAngle = (clock_.getMinute() + clock_.getSecond() / 60.0f) * 6.0f * DEG_TO_RAD;
  int mx = cx + 75 * sinf(mAngle);
  int my = cy - 75 * cosf(mAngle);
  tft.drawLine(cx, cy, mx, my, currentTheme_.text);
  tft.drawLine(cx, cy + 1, mx, my + 1, currentTheme_.text);

  float sAngle = clock_.getSecond() * 6.0f * DEG_TO_RAD;
  int sx = cx + 85 * sinf(sAngle);
  int sy = cy - 85 * cosf(sAngle);
  tft.drawLine(cx, cy, sx, sy, currentTheme_.accent);
  tft.fillCircle(cx, cy, 4, currentTheme_.accent);

  // Right side panel
  int px = 260;
  int pw = kWidth - px - 16;
  fillRoundedRect(tft, px, 16, pw, 288, 12, currentTheme_.panel);

  // Digital time
  char timeBuf[16];
  clock_.formatTime(timeBuf, sizeof(timeBuf));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.setTextFont(4);
  tft.drawString(timeBuf, px + pw / 2, 50);

  // Date
  char dateBuf[48];
  char weekBuf[24];
  clock_.formatDate(dateBuf, sizeof(dateBuf));
  clock_.formatWeekday(weekBuf, sizeof(weekBuf));
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.setTextFont(2);
  tft.drawString(weekBuf, px + pw / 2, 85);
  tft.drawString(dateBuf, px + pw / 2, 105);

  tft.drawFastHLine(px + 12, 130, pw - 24, currentTheme_.border);

  // Countdown
  const Countdown* cd = settings_.getNextCountdown();
  if (cd != nullptr) {
    tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
    tft.setTextFont(4);
    tft.drawString(cd->name, px + pw / 2, 160);

    char remBuf[64];
    countdown_.formatRemaining(cd->targetDate, remBuf, sizeof(remBuf), true);
    tft.setTextColor(currentTheme_.text, currentTheme_.panel);
    tft.setTextFont(2);
    tft.drawString(remBuf, px + pw / 2, 200);

    struct tm targetTm;
    localtime_r(&cd->targetDate, &targetTm);
    char targetStr[32];
    snprintf(targetStr, sizeof(targetStr), "%s %d, %d", kMonthNames[targetTm.tm_mon], targetTm.tm_mday, targetTm.tm_year + 1900);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.drawString(targetStr, px + pw / 2, 240);
  } else {
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.setTextFont(2);
    tft.drawString("No Countdowns", px + pw / 2, 180);
    tft.drawString("Tap to configure", px + pw / 2, 210);
  }
}

void UIManager::renderFocusLayout(TFT_eSPI& tft) {
  // Top mini bar
  char timeBuf[16];
  clock_.formatTime(timeBuf, sizeof(timeBuf));
  char dateBuf[48];
  char weekBuf[24];
  clock_.formatDate(dateBuf, sizeof(dateBuf));
  clock_.formatWeekday(weekBuf, sizeof(weekBuf));

  char topStr[96];
  snprintf(topStr, sizeof(topStr), "%s  ·  %s, %s", timeBuf, weekBuf, dateBuf);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.setTextFont(2);
  tft.drawString(topStr, kWidth / 2, 18);

  // Giant Countdown Card
  fillRoundedRect(tft, 16, 36, kWidth - 32, 268, 12, currentTheme_.panel);

  const Countdown* cd = settings_.getNextCountdown();
  if (cd != nullptr) {
    CountdownManager::RemainingTime rem = countdown_.getRemaining(cd->targetDate);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
    tft.setTextFont(4);
    tft.drawString(cd->name, kWidth / 2, 70);

    char daysStr[16];
    snprintf(daysStr, sizeof(daysStr), "%d", rem.days);
    tft.setTextColor(currentTheme_.text, currentTheme_.panel);
    tft.setTextFont(7);
    tft.drawString(daysStr, kWidth / 2, 130);

    tft.setTextFont(4);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.drawString("DAYS REMAINING", kWidth / 2, 185);

    char subStr[48];
    snprintf(subStr, sizeof(subStr), "%02d HOURS  ·  %02d MIN  ·  %02d SEC", rem.hours, rem.minutes, rem.seconds);
    tft.setTextColor(currentTheme_.text, currentTheme_.panel);
    tft.setTextFont(4);
    tft.drawString(subStr, kWidth / 2, 230);

    struct tm targetTm;
    localtime_r(&cd->targetDate, &targetTm);
    char targetStr[48];
    snprintf(targetStr, sizeof(targetStr), "Target Date: %s %d, %d", kMonthNames[targetTm.tm_mon], targetTm.tm_mday, targetTm.tm_year + 1900);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.setTextFont(2);
    tft.drawString(targetStr, kWidth / 2, 275);
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.setTextFont(4);
    tft.drawString("Focus Mode", kWidth / 2, 140);
    tft.setTextFont(2);
    tft.drawString("No upcoming countdown active. Tap to add one.", kWidth / 2, 180);
  }
}

void UIManager::renderMenu(TFT_eSPI& tft) {
  int mx = 60;
  int my = 25;
  int mw = 360;
  int mh = 270;

  fillRoundedRect(tft, mx, my, mw, mh, 12, currentTheme_.panel);
  drawRoundedRect(tft, mx, my, mw, mh, 12, currentTheme_.border);

  // Header
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.setTextFont(4);
  tft.drawString("MENU", mx + 20, my + 24);

  // Close X button
  drawButton(tft, mx + mw - 50, my + 10, 40, 36, "X", currentTheme_.panelAlt, currentTheme_.text);

  // Buttons
  drawButton(tft, 80, 78, 320, 48, "COUNTDOWNS", currentTheme_.panelAlt, currentTheme_.text);
  drawButton(tft, 80, 136, 320, 48, "SETTINGS", currentTheme_.panelAlt, currentTheme_.text);
  
  char layoutLabel[48];
  snprintf(layoutLabel, sizeof(layoutLabel), "LAYOUT: %s", kLayoutNames[settings_.getSettings().layoutId % LAYOUT_COUNT]);
  drawButton(tft, 80, 194, 320, 48, layoutLabel, currentTheme_.accent, currentTheme_.textOnAccent);

  drawButton(tft, 80, 248, 320, 36, "CLOSE", currentTheme_.panelAlt, currentTheme_.textMuted);
}

void UIManager::renderCountdowns(TFT_eSPI& tft, const TouchCal::Sample& touch) {
  // Top bar
  drawButton(tft, 16, 12, 90, 36, "< Back", currentTheme_.panel, currentTheme_.text);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.setTextFont(4);
  tft.drawString("COUNTDOWNS", kWidth / 2, 30);

  drawButton(tft, 374, 12, 90, 36, "+ Add", currentTheme_.accent, currentTheme_.textOnAccent);

  int total = settings_.getCountdownCount();
  const Countdown* cds = settings_.getCountdowns();

  if (total == 0) {
    fillRoundedRect(tft, 16, 60, kWidth - 32, 230, 12, currentTheme_.panel);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.setTextFont(4);
    tft.drawString("No Countdowns Saved", kWidth / 2, 150);
    tft.setTextFont(2);
    tft.drawString("Tap '+ Add' to create a countdown.", kWidth / 2, 190);
    return;
  }

  int startIndex = countdownPage_ * 3;
  for (int i = 0; i < 3 && (startIndex + i) < total; i++) {
    int idx = startIndex + i;
    const Countdown& cd = cds[idx];
    int y = 60 + i * 80;

    fillRoundedRect(tft, 16, y, kWidth - 32, 72, 8, currentTheme_.panel);
    drawRoundedRect(tft, 16, y, kWidth - 32, 72, 8, currentTheme_.border);

    // Accent pill
    uint16_t color = cd.accentColor != 0 ? rgbTo565(cd.accentColor) : currentTheme_.accent;
    tft.fillRect(20, y + 8, 4, 56, color);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(currentTheme_.text, currentTheme_.panel);
    tft.setTextFont(4);
    tft.drawString(cd.name, 34, y + 24);

    char remStr[64];
    countdown_.formatRemaining(cd.targetDate, remStr, sizeof(remStr), false);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.setTextFont(2);
    tft.drawString(remStr, 34, y + 52);

    // Delete [X] button
    drawButton(tft, 410, y + 14, 44, 44, "X", currentTheme_.danger, currentTheme_.text);
  }

  if (total > 3) {
    if (countdownPage_ > 0) {
      drawButton(tft, 16, 280, 80, 34, "< Prev", currentTheme_.panel, currentTheme_.text);
    }
    char pageStr[32];
    int maxPages = (total + 2) / 3;
    snprintf(pageStr, sizeof(pageStr), "Page %d / %d", countdownPage_ + 1, maxPages);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
    tft.setTextFont(2);
    tft.drawString(pageStr, kWidth / 2, 297);

    if ((countdownPage_ + 1) * 3 < total) {
      drawButton(tft, 384, 280, 80, 34, "Next >", currentTheme_.panel, currentTheme_.text);
    }
  }
}

void UIManager::renderCountdownEdit(TFT_eSPI& tft, const TouchCal::Sample& touch) {
  // Top bar
  drawButton(tft, 16, 12, 90, 36, "< Cancel", currentTheme_.panel, currentTheme_.text);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.setTextFont(4);
  tft.drawString(strlen(editingCountdownId_) > 0 ? "EDIT COUNTDOWN" : "NEW COUNTDOWN", kWidth / 2, 30);

  drawButton(tft, 374, 12, 90, 36, "Save", currentTheme_.accent, currentTheme_.textOnAccent);

  fillRoundedRect(tft, 16, 56, kWidth - 32, 252, 12, currentTheme_.panel);

  // Row 1: Event Name
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.setTextFont(4);
  tft.drawString("Event:", 28, 82);

  drawButton(tft, 110, 64, 36, 36, "<", currentTheme_.panelAlt, currentTheme_.text);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
  tft.setTextFont(4);
  tft.drawString(editName_, 245, 82);
  drawButton(tft, 380, 64, 36, 36, ">", currentTheme_.panelAlt, currentTheme_.text);

  // Row 2: Date
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.setTextFont(4);
  tft.drawString("Date:", 28, 127);

  drawButton(tft, 100, 110, 32, 34, "-", currentTheme_.panelAlt, currentTheme_.text);
  char yStr[8]; snprintf(yStr, sizeof(yStr), "%d", editYear_);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.setTextFont(2);
  tft.drawString(yStr, 155, 127);
  drawButton(tft, 190, 110, 32, 34, "+", currentTheme_.panelAlt, currentTheme_.text);

  drawButton(tft, 230, 110, 32, 34, "-", currentTheme_.panelAlt, currentTheme_.text);
  tft.drawString(kMonthNames[(editMonth_ - 1 + 12) % 12], 285, 127);
  drawButton(tft, 310, 110, 32, 34, "+", currentTheme_.panelAlt, currentTheme_.text);

  drawButton(tft, 350, 110, 32, 34, "-", currentTheme_.panelAlt, currentTheme_.text);
  char dStr[8]; snprintf(dStr, sizeof(dStr), "%02d", editDay_);
  tft.drawString(dStr, 395, 127);
  drawButton(tft, 420, 110, 32, 34, "+", currentTheme_.panelAlt, currentTheme_.text);

  // Row 3: Time
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.setTextFont(4);
  tft.drawString("Time:", 28, 173);

  drawButton(tft, 100, 156, 32, 34, "-", currentTheme_.panelAlt, currentTheme_.text);
  char hStr[8]; snprintf(hStr, sizeof(hStr), "%02d", editHour_);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.setTextFont(2);
  tft.drawString(hStr, 145, 173);
  drawButton(tft, 170, 156, 32, 34, "+", currentTheme_.panelAlt, currentTheme_.text);

  drawButton(tft, 210, 156, 32, 34, "-", currentTheme_.panelAlt, currentTheme_.text);
  char mStr[8]; snprintf(mStr, sizeof(mStr), "%02d", editMinute_);
  tft.drawString(mStr, 255, 173);
  drawButton(tft, 280, 156, 32, 34, "+", currentTheme_.panelAlt, currentTheme_.text);

  drawButton(tft, 320, 156, 130, 34, editHasTime_ ? "Time: YES" : "Time: NO", 
             editHasTime_ ? currentTheme_.accent : currentTheme_.panelAlt, 
             editHasTime_ ? currentTheme_.textOnAccent : currentTheme_.textMuted);

  // Row 4: Main Screen Toggle
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.setTextFont(4);
  tft.drawString("Show on Main:", 28, 221);
  drawButton(tft, 250, 204, 140, 34, editShowOnMain_ ? "YES" : "NO",
             editShowOnMain_ ? currentTheme_.accent : currentTheme_.panelAlt,
             editShowOnMain_ ? currentTheme_.textOnAccent : currentTheme_.textMuted);

  // Delete button if editing existing
  if (strlen(editingCountdownId_) > 0) {
    drawButton(tft, 140, 252, 200, 40, "DELETE COUNTDOWN", currentTheme_.danger, currentTheme_.text);
  }
}

void UIManager::renderSettings(TFT_eSPI& tft, const TouchCal::Sample& touch) {
  // Top bar
  drawButton(tft, 16, 12, 90, 36, "< Back", currentTheme_.panel, currentTheme_.text);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.setTextFont(4);
  tft.drawString("SETTINGS", kWidth / 2, 30);

  fillRoundedRect(tft, 16, 54, kWidth - 32, 254, 12, currentTheme_.panel);

  const Settings& s = settings_.getSettings();

  // Row 1: Time Format
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.setTextFont(4);
  tft.drawString("Time Format:", 32, 79);
  drawButton(tft, 250, 60, 200, 38, s.use24Hour ? "24-Hour" : "12-Hour (AM/PM)", currentTheme_.panelAlt, currentTheme_.text);

  // Row 2: Layout
  tft.drawString("Layout:", 32, 125);
  drawButton(tft, 250, 106, 200, 38, kLayoutNames[s.layoutId % LAYOUT_COUNT], currentTheme_.panelAlt, currentTheme_.text);

  // Row 3: Theme
  tft.drawString("Theme:", 32, 171);
  drawButton(tft, 250, 152, 200, 38, kThemeNames[s.themeId % THEME_COUNT], currentTheme_.panelAlt, currentTheme_.text);

  // Row 4: Brightness
  tft.drawString("Brightness:", 32, 217);
  drawButton(tft, 250, 198, 48, 38, "-", currentTheme_.panelAlt, currentTheme_.text);
  char bStr[8]; snprintf(bStr, sizeof(bStr), "%d", s.brightness);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.setTextFont(2);
  tft.drawString(bStr, 350, 217);
  drawButton(tft, 402, 198, 48, 38, "+", currentTheme_.panelAlt, currentTheme_.text);

  // Bottom action buttons
  drawButton(tft, 24, 252, 200, 42, "WiFi Setup", currentTheme_.panelAlt, currentTheme_.text);
  drawButton(tft, 256, 252, 200, 42, "About Device", currentTheme_.panelAlt, currentTheme_.text);
}

void UIManager::renderWifiSetup(TFT_eSPI& tft) {
  // Top bar
  drawButton(tft, 16, 12, 90, 36, "< Back", currentTheme_.panel, currentTheme_.text);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.setTextFont(4);
  tft.drawString("WI-FI SETUP", kWidth / 2, 30);

  fillRoundedRect(tft, 16, 54, kWidth - 32, 254, 12, currentTheme_.panel);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
  tft.setTextFont(4);
  tft.drawString("Hotspot Active!", kWidth / 2, 85);

  tft.setTextFont(2);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.drawString("1. Connect phone or PC to Wi-Fi network:", kWidth / 2, 125);

  tft.setTextFont(4);
  tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
  tft.drawString("CYD-Clock-Setup", kWidth / 2, 155);

  tft.setTextFont(2);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString("(Open network, no password required)", kWidth / 2, 185);

  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.drawString("2. Open browser and navigate to:", kWidth / 2, 220);

  tft.setTextFont(4);
  tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
  tft.drawString("http://192.168.4.1", kWidth / 2, 250);

  tft.setTextFont(2);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString("3. Select your home network and enter password.", kWidth / 2, 280);
}

void UIManager::renderAbout(TFT_eSPI& tft) {
  // Top bar
  drawButton(tft, 16, 12, 90, 36, "< Back", currentTheme_.panel, currentTheme_.text);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.setTextFont(4);
  tft.drawString("ABOUT CYD CLOCK", kWidth / 2, 30);

  fillRoundedRect(tft, 16, 54, kWidth - 32, 254, 12, currentTheme_.panel);

  tft.setTextFont(2);
  int y = 72;
  int dy = 22;

  auto drawInfoLine = [&](const char* label, const char* value) {
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.drawString(label, 32, y);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(currentTheme_.text, currentTheme_.panel);
    tft.drawString(value, kWidth - 32, y);
    y += dy;
  };

  drawInfoLine("Hardware:", "ESP32-32E (ST7796S + XPT2046)");
  drawInfoLine("Firmware:", "v1.0.0");

  bool connected = wifi_ && wifi_->isConnected();
  bool apActive = wifi_ && wifi_->isAPActive();
  drawInfoLine("WiFi Status:", connected ? "Connected" : (apActive ? "Hotspot Active" : "Offline"));
  if (connected) {
    drawInfoLine("SSID:", wifi_->getSSID());
    drawInfoLine("IP Address:", wifi_->getIP());
  } else {
    drawInfoLine("Hotspot:", "CYD-Clock-Setup");
    drawInfoLine("Portal IP:", "192.168.4.1");
  }

  drawInfoLine("Timezone:", clock_.getTimezone());
  drawInfoLine("NTP Time Sync:", clock_.isTimeSynced() ? "Synchronized" : "Pending Sync");

  char heapStr[32];
  snprintf(heapStr, sizeof(heapStr), "%u KB free", ESP.getFreeHeap() / 1024);
  drawInfoLine("Free RAM:", heapStr);

  char uptimeStr[32];
  uint32_t s = millis() / 1000;
  snprintf(uptimeStr, sizeof(uptimeStr), "%ud %02uh %02um", s / 86400, (s % 86400) / 3600, (s % 3600) / 60);
  drawInfoLine("Uptime:", uptimeStr);
}

void UIManager::drawRoundedRect(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  tft.drawRoundRect(x, y, w, h, r, color);
}

void UIManager::fillRoundedRect(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, r, color);
}

void UIManager::drawButton(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, const char* label, uint16_t bg, uint16_t textColor, bool pressed) {
  uint16_t fillBg = pressed ? blendColors(bg, 0xFFFF, 0.25f) : bg;
  tft.fillRoundRect(x, y, w, h, 8, fillBg);
  tft.drawRoundRect(x, y, w, h, 8, currentTheme_.border);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(textColor, fillBg);
  tft.setTextFont(2);
  tft.drawString(label, x + w / 2, y + h / 2);
}

bool UIManager::isInRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  return (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}

void UIManager::applyTheme(uint8_t themeId) {
  switch (themeId) {
    case THEME_NORD:
      currentTheme_.bg = 0x29A7;
      currentTheme_.panel = 0x3A2A;
      currentTheme_.panelAlt = 0x426C;
      currentTheme_.accent = 0x861A;
      currentTheme_.text = 0xEF7D;
      currentTheme_.textMuted = 0xD6FA;
      currentTheme_.textOnAccent = 0x29A7;
      currentTheme_.border = 0x4B0D;
      currentTheme_.danger = 0xBC8D;
      break;
    case THEME_DRACULA:
      currentTheme_.bg = 0x2947;
      currentTheme_.panel = 0x31C8;
      currentTheme_.panelAlt = 0x424B;
      currentTheme_.accent = 0xFBD8;
      currentTheme_.text = 0xFFBD;
      currentTheme_.textMuted = 0x6394;
      currentTheme_.textOnAccent = 0x2145;
      currentTheme_.border = 0x6274;
      currentTheme_.danger = 0xFAAA;
      break;
    case THEME_SOLARIZED_LIGHT:
      currentTheme_.bg = 0xFF58;
      currentTheme_.panel = 0xEF7A;
      currentTheme_.panelAlt = 0xE6D8;
      currentTheme_.accent = 0x245A;
      currentTheme_.text = 0x63EA;
      currentTheme_.textMuted = 0x9514;
      currentTheme_.textOnAccent = 0xFFFF;
      currentTheme_.border = 0xD699;
      currentTheme_.danger = 0xD985;
      break;
    case THEME_HIGH_CONTRAST:
      currentTheme_.bg = 0x0000;
      currentTheme_.panel = 0x1082;
      currentTheme_.panelAlt = 0x2104;
      currentTheme_.accent = 0xFFE0;
      currentTheme_.text = 0xFFFF;
      currentTheme_.textMuted = 0xCE59;
      currentTheme_.textOnAccent = 0x0000;
      currentTheme_.border = 0xFFFF;
      currentTheme_.danger = 0xF800;
      break;
    case THEME_WARM:
      currentTheme_.bg = 0x28C3;
      currentTheme_.panel = 0x3905;
      currentTheme_.panelAlt = 0x4987;
      currentTheme_.accent = 0xFD20;
      currentTheme_.text = 0xFFDC;
      currentTheme_.textMuted = 0xBA4B;
      currentTheme_.textOnAccent = 0x0000;
      currentTheme_.border = 0x5A48;
      currentTheme_.danger = 0xF986;
      break;
    case THEME_DEFAULT:
    default:
      currentTheme_.bg = 0x0841;
      currentTheme_.panel = 0x18E3;
      currentTheme_.panelAlt = 0x2124;
      currentTheme_.accent = 0x07FF;
      currentTheme_.text = 0xFFFF;
      currentTheme_.textMuted = 0x8410;
      currentTheme_.textOnAccent = 0x0000;
      currentTheme_.border = 0x2945;
      currentTheme_.danger = 0xF800;
      break;
  }
}

uint16_t UIManager::rgbTo565(uint32_t rgb) {
  uint8_t r = (rgb >> 16) & 0xFF;
  uint8_t g = (rgb >> 8) & 0xFF;
  uint8_t b = rgb & 0xFF;
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint32_t UIManager::blendColors(uint32_t c1, uint32_t c2, float ratio) {
  uint8_t r1 = (c1 >> 11) & 0x1F;
  uint8_t g1 = (c1 >> 5) & 0x3F;
  uint8_t b1 = c1 & 0x1F;

  uint8_t r2 = (c2 >> 11) & 0x1F;
  uint8_t g2 = (c2 >> 5) & 0x3F;
  uint8_t b2 = c2 & 0x1F;

  uint8_t r = r1 + (r2 - r1) * ratio;
  uint8_t g = g1 + (g2 - g1) * ratio;
  uint8_t b = b1 + (b2 - b1) * ratio;

  return (r << 11) | (g << 5) | b;
}

void UIManager::formatCountdownRemaining(const Countdown* cd, char* buffer, size_t size, bool shortFormat) {
  if (cd == nullptr) {
    snprintf(buffer, size, "NONE");
    return;
  }
  if (shortFormat) {
    countdown_.formatRemainingShort(cd->targetDate, buffer, size);
  } else {
    countdown_.formatRemaining(cd->targetDate, buffer, size, true);
  }
}
