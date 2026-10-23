#include "ui_manager.h"
#include "timezones.h"
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
  "Dashboard", "Minimal", "Analog", "Focus", "Flip", "Pixel", "Smooth"
};

const char* const kThemeNames[] = {
  "Graphite", "Nord", "Dracula", "Paper", "Contrast", "Ember"
};

}  // namespace

UIManager::UIManager(SettingsManager& settings, ClockManager& clock, CountdownManager& countdown, FocusTimer& focus)
    : settings_(settings), clock_(clock), countdown_(countdown), focus_(focus) {
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
  displayedBrightness_ = brightnessTarget_ = settings_.getSettings().brightness;
  brightnessFrom_ = displayedBrightness_;
  renderedView_ = UINT32_MAX;

  tft.fillScreen(currentTheme_.bg);

  // Minimal boot mark. The progress line is completed as each subsystem starts.
  tft.drawCircle(kWidth / 2, 100, 25, currentTheme_.accent);
  tft.drawWideLine(kWidth / 2, 100, kWidth / 2, 83, 3, currentTheme_.accent,
                   currentTheme_.bg);
  tft.drawWideLine(kWidth / 2, 100, kWidth / 2 + 14, 100, 3, currentTheme_.accent,
                   currentTheme_.bg);
  tft.fillCircle(kWidth / 2, 100, 3, currentTheme_.accent);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.setTextFont(4);
  tft.drawString("clocky.", kWidth / 2, 153);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.setTextFont(2);
  tft.drawString("A little more present.", kWidth / 2, 184);
  tft.fillRoundRect(180, 216, 120, 3, 1, currentTheme_.panelAlt);
  for (int width = 8; width <= 120; width += 8) {
    tft.fillRoundRect(180, 216, width, 3, 1, currentTheme_.accent);
    delay(10);
  }
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

void UIManager::updateBacklight(uint32_t now, float visibility) {
  const uint8_t target = settings_.getSettings().brightness;
  if (target != brightnessTarget_) {
    brightnessFrom_ = displayedBrightness_;
    brightnessTarget_ = target;
    brightnessStarted_ = now;
  }
  float progress = fminf(1.0f, (now - brightnessStarted_) / 180.0f);
  progress = progress * progress * (3.0f - 2.0f * progress);
  displayedBrightness_ = brightnessFrom_ + (brightnessTarget_ - brightnessFrom_) * progress;
  const int level = static_cast<int>(displayedBrightness_ * visibility + 0.5f);
  if (level != lastBacklight_) {
    analogWrite(Pins::kTftBl, level);
    lastBacklight_ = level;
  }
}

void UIManager::render(TFT_eSPI& tft, const TouchCal::Sample& touch) {
  const uint32_t now = millis();
  if (motionPhase_ == MotionPhase::Idle) {
    if (waitForTouchRelease_) {
      if (!touch.pressed) waitForTouchRelease_ = false;
    } else {
      handleTouch(tft, touch);
    }
  }

  // Refresh connection details while this screen stays open, including DHCP changes.
  if (currentScreen_ == SCREEN_WIFI_SETUP && wifi_ && now - lastWifiCheck_ >= 1000) {
    lastWifiCheck_ = now;
    char details[64];
    snprintf(details, sizeof(details), "%d|%d|%s|%s", wifi_->isConnected(),
             wifi_->isAPActive(), wifi_->getIP(), wifi_->getSSID());
    if (strcmp(details, lastWifiDetails_) != 0) {
      strlcpy(lastWifiDetails_, details, sizeof(lastWifiDetails_));
      screenNeedsRedraw_ = true;
    }
  }

  const Settings& settings = settings_.getSettings();
  if (strcmp(lastTimezone_, settings.timezone) != 0) {
    strlcpy(lastTimezone_, settings.timezone, sizeof(lastTimezone_));
    screenNeedsRedraw_ = true;
  }
  const uint32_t view = static_cast<uint32_t>(currentScreen_) |
      (static_cast<uint32_t>(menuVisible_) << 3) |
      (static_cast<uint32_t>(settings.layoutId) << 4) |
      (static_cast<uint32_t>(settings.themeId) << 12) |
      (static_cast<uint32_t>(countdownPage_) << 20);

  if (motionPhase_ == MotionPhase::Idle && view != renderedView_) {
    motionPhase_ = MotionPhase::FadeOut;
    // The initial screen is revealed immediately from black.
    motionStarted_ = renderedView_ == UINT32_MAX ? now - 100 : now;
    touchState_ = TouchState{};
    waitForTouchRelease_ = true;
  }

  if (motionPhase_ == MotionPhase::FadeOut) {
    float progress = fminf(1.0f, (now - motionStarted_) / 100.0f);
    progress = progress * progress * (3.0f - 2.0f * progress);
    updateBacklight(now, 1.0f - progress);
    if (progress < 1.0f) return;
    // Redraw once while dark, then reveal; networking keeps running each frame.
    applyTheme(settings.themeId);
    screenNeedsRedraw_ = true;
    renderedView_ = view;
    renderContent(tft, TouchCal::Sample{});
    motionPhase_ = MotionPhase::FadeIn;
    motionStarted_ = millis();
    return;
  }

  if (motionPhase_ == MotionPhase::FadeIn) {
    float progress = fminf(1.0f, (now - motionStarted_) / 160.0f);
    progress = progress * progress * (3.0f - 2.0f * progress);
    updateBacklight(now, progress);
    if (progress >= 1.0f) motionPhase_ = MotionPhase::Idle;
    return;
  }

  updateBacklight(now, 1.0f);
  renderContent(tft, touch);
}

void UIManager::renderContent(TFT_eSPI& tft, const TouchCal::Sample& touch) {

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
      case SCREEN_TIMEZONE:
        renderTimezone(tft);
        break;
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
  if (wifi_ && !wifi_->isConnected()) {
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
              case SCREEN_TIMEZONE:
                handleTimezoneTouch(tap);
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
  if (settings_.getSettings().layoutId == LAYOUT_FOCUS) {
    if (touch.y >= 210 && touch.y <= 246) {
      const uint32_t seconds = touch.x < 165 ? 300 : touch.x < 315 ? 1500 : 3000;
      focus_.start(seconds, millis()); screenNeedsRedraw_ = true; return;
    }
    if (touch.y >= 253 && touch.y <= 287 && touch.x >= 80 && touch.x <= 400) {
      const auto state = focus_.snapshot(millis());
      if (touch.x >= 307) focus_.reset(millis());
      else if (state.running) focus_.pause(millis());
      else if (state.completed) focus_.start(state.durationSeconds, millis());
      else focus_.resume(millis());
      screenNeedsRedraw_ = true; return;
    }
  }
  showMenu();
}

void UIManager::handleMenuTouch(const TouchCal::Sample& touch) {
  if (touch.y < 70 || touch.y > 280) { hideMenu(); return; }
  if (touch.y <= 176) {
    if (touch.x < 240) showCountdowns(); else showSettings();
  } else {
    if (touch.x < 240) setLayout(LAYOUT_FOCUS); else cycleLayout();
    backToClock();
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
    int y = 60 + i * 70;

    // Delete [X] button
    if (touch.x >= 390 && touch.y >= y && touch.y <= (y + 66)) {
      deleteCountdown(cds[idx].id);
      return;
    }

    // Card click -> edit
    if (touch.x < 390 && touch.y >= y && touch.y <= (y + 66)) {
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

  if (isInRect(touch.x, touch.y, 24, 58, 432, 36)) {
    const int index = timezoneIndex(settings_.getSettings().timezone);
    timezonePage_ = index < 0 ? 0 : index / 5;
    currentScreen_ = SCREEN_TIMEZONE;
  } else if (isInRect(touch.x, touch.y, 24, 94, 432, 36)) {
    settings_.setUse24Hour(!settings_.getSettings().use24Hour);
  } else if (isInRect(touch.x, touch.y, 24, 130, 432, 36)) {
    cycleLayout();
  } else if (isInRect(touch.x, touch.y, 24, 166, 432, 36)) {
    setTheme((settings_.getSettings().themeId + 1) % THEME_COUNT);
  } else if (isInRect(touch.x, touch.y, 250, 205, 48, 36)) {
    const uint8_t cur = settings_.getSettings().brightness;
    settings_.setBrightness(cur > 35 ? cur - 35 : 20);
  } else if (isInRect(touch.x, touch.y, 402, 205, 48, 36)) {
    const uint8_t cur = settings_.getSettings().brightness;
    settings_.setBrightness(cur < 220 ? cur + 35 : 255);
  } else if (isInRect(touch.x, touch.y, 24, 250, 200, 36)) {
    startWifiSetup();
  } else if (isInRect(touch.x, touch.y, 256, 250, 200, 36)) {
    showAbout();
  }
  screenNeedsRedraw_ = true;
}

void UIManager::handleTimezoneTouch(const TouchCal::Sample& touch) {
  if (isInRect(touch.x, touch.y, 16, 11, 58, 34)) {
    showSettings();
    return;
  }
  for (size_t row = 0; row < 5; ++row) {
    const size_t index = timezonePage_ * 5 + row;
    if (index < kTimezoneCount && isInRect(touch.x, touch.y, 16, 60 + row * 40, 448, 36)) {
      settings_.setTimezone(kTimezoneTable[index].name);
      showSettings();
      return;
    }
  }
  if (isInRect(touch.x, touch.y, 16, 275, 90, 36) && timezonePage_ > 0) {
    --timezonePage_;
  } else if (isInRect(touch.x, touch.y, 374, 275, 90, 36) && (timezonePage_ + 1) * 5 < kTimezoneCount) {
    ++timezonePage_;
  }
  screenNeedsRedraw_ = true;
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
    case LAYOUT_FLIP:
      renderFlipLayout(tft);
      break;
    case LAYOUT_PIXEL:
      renderPixelLayout(tft);
      break;
    case LAYOUT_SMOOTH:
      renderSmoothLayout(tft);
      break;
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

void UIManager::drawClockChrome(TFT_eSPI& tft, const char* label) {
  tft.fillRect(0, 0, kWidth, 42, currentTheme_.bg);
  tft.fillRect(0, 291, kWidth, 29, currentTheme_.bg);
  tft.setTextSize(1);
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.drawString("clocky.", 22, 22);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.drawString(label, 458, 22);
  tft.drawFastHLine(22, 42, 436, currentTheme_.border);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.drawString("Swipe to explore", 22, 307);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("Tap for menu", 458, 307);
  tft.fillCircle(242, 307, 3, currentTheme_.accent);
}

void UIManager::drawNextEvent(TFT_eSPI& tft, int y) {
  fillRoundedRect(tft, 22, y, 436, 66, 10, currentTheme_.panel);
  const Countdown* cd = settings_.getNextCountdown();
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString(cd ? "ON THE HORIZON" : "MAKE ROOM FOR A GOOD THING", 38, y + 15);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  if (cd) {
    String title = cd->name;
    while (tft.textWidth(title) > 230 && title.length() > 0) title.remove(title.length() - 1);
    tft.drawString(title, 38, y + 42);
    char remaining[40];
    countdown_.formatRemainingShort(cd->targetDate, remaining, sizeof(remaining));
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
    tft.drawString(remaining, 440, y + 42);
  } else {
    tft.drawString("Add a moment worth looking forward to.", 38, y + 42);
  }
}

void UIManager::renderDigitalLayout(TFT_eSPI& tft) {
  drawClockChrome(tft, clock_.isTimeSynced() ? "IN GOOD TIME" : "WAITING FOR SYNC");
  tft.fillRect(0, 44, kWidth, 173, currentTheme_.bg);
  char weekday[24], date[40], value[16];
  clock_.formatWeekday(weekday, sizeof(weekday));
  clock_.formatDate(date, sizeof(date));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
  tft.setTextFont(2);
  tft.drawString(weekday, 240, 66);
  int hour = clock_.getHour();
  if (!settings_.getSettings().use24Hour) hour = hour % 12 == 0 ? 12 : hour % 12;
  snprintf(value, sizeof(value), "%02d:%02d", hour, clock_.getMinute());
  tft.setTextFont(4);
  tft.setTextSize(3);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.drawString(value, 222, 124);
  tft.setTextSize(1);
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
  snprintf(value, sizeof(value), "%02d", clock_.getSecond());
  tft.drawString(value, 366, 122);
  if (!settings_.getSettings().use24Hour) tft.drawString(clock_.getHour() < 12 ? "AM" : "PM", 366, 143);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.drawString(date, 240, 184);
  const int progress = (clock_.getHour() * 3600 + clock_.getMinute() * 60 + clock_.getSecond()) * 436 / 86400;
  tft.drawFastHLine(22, 205, 436, currentTheme_.border);
  tft.drawFastHLine(22, 205, progress, currentTheme_.accent);
  drawNextEvent(tft, 222);
}

void UIManager::renderMinimalLayout(TFT_eSPI& tft) {
  drawClockChrome(tft, "LESS, BUT BETTER");
  tft.fillRect(0, 44, kWidth, 246, currentTheme_.bg);
  char value[16], date[40];
  int hour = clock_.getHour();
  if (!settings_.getSettings().use24Hour) hour = hour % 12 == 0 ? 12 : hour % 12;
  snprintf(value, sizeof(value), "%02d:%02d", hour, clock_.getMinute());
  tft.setTextFont(4);
  tft.setTextSize(3);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.drawString(value, 240, 134);
  tft.setTextSize(1);
  clock_.formatDate(date, sizeof(date));
  tft.setTextFont(2);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.drawString(date, 240, 203);
  tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
  tft.drawString("A little more present.", 240, 248);
}

void UIManager::renderAnalogLayout(TFT_eSPI& tft) {
  drawClockChrome(tft, "AT YOUR OWN PACE");
  const int cx = 140, cy = 165, r = 105;
  tft.fillCircle(cx, cy, r + 1, currentTheme_.bg);
  tft.drawCircle(cx, cy, r, currentTheme_.border);
  for (int i = 0; i < 60; ++i) {
    const float angle = i * 6.0f * DEG_TO_RAD;
    const int inner = i % 5 == 0 ? r - 13 : r - 5;
    tft.drawLine(cx + inner * sinf(angle), cy - inner * cosf(angle), cx + (r - 2) * sinf(angle), cy - (r - 2) * cosf(angle), i % 5 == 0 ? currentTheme_.textMuted : currentTheme_.border);
  }
  const float hour = ((clock_.getHour() % 12) + clock_.getMinute() / 60.0f) * 30 * DEG_TO_RAD;
  const float minute = (clock_.getMinute() + clock_.getSecond() / 60.0f) * 6 * DEG_TO_RAD;
  const float second = clock_.getSecond() * 6 * DEG_TO_RAD;
  tft.drawWideLine(cx, cy, cx + 52 * sinf(hour), cy - 52 * cosf(hour), 4, currentTheme_.text, currentTheme_.bg);
  tft.drawWideLine(cx, cy, cx + 78 * sinf(minute), cy - 78 * cosf(minute), 3, currentTheme_.text, currentTheme_.bg);
  tft.drawLine(cx, cy, cx + 88 * sinf(second), cy - 88 * cosf(second), currentTheme_.accent);
  tft.fillCircle(cx, cy, 5, currentTheme_.accent);
  tft.fillRect(266, 52, 200, 228, currentTheme_.bg);
  char time[24], date[32], day[24];
  clock_.formatTime(time, sizeof(time)); clock_.formatDate(date, sizeof(date)); clock_.formatWeekday(day, sizeof(day));
  tft.setTextDatum(ML_DATUM); tft.setTextFont(2);
  tft.setTextColor(currentTheme_.accent, currentTheme_.bg); tft.drawString(day, 272, 96);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg); tft.drawString(time, 272, 130);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg); tft.drawString(date, 272, 162);
  tft.drawFastHLine(272, 191, 175, currentTheme_.border);
  tft.drawString("Time, beautifully simple.", 272, 222);
}

void UIManager::renderFocusLayout(TFT_eSPI& tft) {
  const auto state = focus_.snapshot(millis());
  drawClockChrome(tft, state.completed ? "NICELY DONE" : state.running ? "IN THE ZONE" : "ONE THING AT A TIME");
  tft.fillRect(0, 44, kWidth, 246, currentTheme_.bg);
  char time[16];
  snprintf(time, sizeof(time), "%02u:%02u", state.remainingSeconds / 60, state.remainingSeconds % 60);
  tft.setTextDatum(MC_DATUM); tft.setTextFont(4); tft.setTextSize(3);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg); tft.drawString(time, 240, 105);
  tft.setTextSize(1); tft.setTextFont(2);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.drawString(state.completed ? "Take a breath. You earned it." : "Make space for your next good idea.", 240, 161);
  tft.fillRoundRect(80, 185, 320, 4, 2, currentTheme_.panelAlt);
  const int progress = 320 * (state.durationSeconds - state.remainingSeconds) / state.durationSeconds;
  if (progress > 0) tft.fillRoundRect(80, 185, progress, 4, 2, currentTheme_.accent);
  drawButton(tft, 22, 210, 134, 36, "5 min break", currentTheme_.panel, currentTheme_.textMuted);
  drawButton(tft, 173, 210, 134, 36, "25 min focus", currentTheme_.panel, currentTheme_.textMuted);
  drawButton(tft, 324, 210, 134, 36, "50 min deep", currentTheme_.panel, currentTheme_.textMuted);
  drawButton(tft, 80, 253, 210, 34, state.running ? "Pause session" : state.completed ? "Start again" : "Start / resume", currentTheme_.accent, currentTheme_.textOnAccent);
  drawButton(tft, 307, 253, 93, 34, "Reset", currentTheme_.panelAlt, currentTheme_.textMuted);
}

void UIManager::renderFlipLayout(TFT_eSPI& tft) {
  char hourText[4], minuteText[4], dateText[40];
  int hour = clock_.getHour();
  const bool pm = hour >= 12;
  if (!settings_.getSettings().use24Hour) hour = hour % 12 == 0 ? 12 : hour % 12;
  snprintf(hourText, sizeof(hourText), "%02d", hour);
  snprintf(minuteText, sizeof(minuteText), "%02d", clock_.getMinute());
  clock_.formatDate(dateText, sizeof(dateText));

  tft.setTextSize(1);
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.drawString("clocky.", 22, 22);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.drawString("MECHANICAL FLIP", 458, 22);

  const int cardY = 54, cardW = 198, cardH = 194;
  fillRoundedRect(tft, 28, cardY, cardW, cardH, 12, currentTheme_.panel);
  fillRoundedRect(tft, 254, cardY, cardW, cardH, 12, currentTheme_.panel);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(7);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.setTextPadding(176);
  tft.drawString(hourText, 127, 147);
  tft.drawString(minuteText, 353, 147);
  tft.setTextPadding(0);
  tft.drawFastHLine(34, cardY + cardH / 2, cardW - 12, currentTheme_.border);
  tft.drawFastHLine(260, cardY + cardH / 2, cardW - 12, currentTheme_.border);
  tft.fillCircle(35, cardY + cardH / 2, 3, currentTheme_.panelAlt);
  tft.fillCircle(219, cardY + cardH / 2, 3, currentTheme_.panelAlt);
  tft.fillCircle(261, cardY + cardH / 2, 3, currentTheme_.panelAlt);
  tft.fillCircle(445, cardY + cardH / 2, 3, currentTheme_.panelAlt);

  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.setTextPadding(280);
  tft.drawString(dateText, 28, 274);
  tft.setTextPadding(0);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
  char detail[16];
  if (settings_.getSettings().use24Hour) {
    snprintf(detail, sizeof(detail), "%02d sec", clock_.getSecond());
  } else {
    snprintf(detail, sizeof(detail), "%s  %02d sec", pm ? "PM" : "AM", clock_.getSecond());
  }
  tft.drawString(detail, 452, 274);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.drawString("Swipe for another face  /  Tap for menu", 240, 307);
}

void UIManager::renderPixelLayout(TFT_eSPI& tft) {
  char timeText[8], seconds[4], dateText[40];
  int hour = clock_.getHour();
  if (!settings_.getSettings().use24Hour) hour = hour % 12 == 0 ? 12 : hour % 12;
  snprintf(timeText, sizeof(timeText), "%02d:%02d", hour, clock_.getMinute());
  snprintf(seconds, sizeof(seconds), "%02d", clock_.getSecond());
  clock_.formatDate(dateText, sizeof(dateText));

  tft.setTextSize(1); tft.setTextFont(2); tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
  tft.drawString("CLOCKY // PIXEL", 22, 22);
  tft.setTextDatum(MR_DATUM); tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.drawString(clock_.isTimeSynced() ? "ONLINE" : "SYNCING", 458, 22);

  // Four corner pixels frame the face without taking space from the clock.
  const int corners[][2] = {{22, 55}, {450, 55}, {22, 258}, {450, 258}};
  for (const auto& point : corners) tft.fillRect(point[0], point[1], 8, 8, currentTheme_.accent);
  tft.setTextFont(1); tft.setTextSize(10); tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.setTextPadding(390);
  tft.drawString(timeText, 240, 142);
  tft.setTextPadding(0); tft.setTextSize(1); tft.setTextFont(4);
  tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
  tft.setTextPadding(50); tft.drawString(seconds, 410, 203); tft.setTextPadding(0);
  tft.setTextFont(2); tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.setTextPadding(320); tft.drawString(dateText, 240, 241); tft.setTextPadding(0);
  tft.drawFastHLine(80, 274, 320, currentTheme_.border);
  tft.setTextFont(1); tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
  tft.drawString("SWIPE:CHANGE   TAP:MENU", 240, 299);
}

void UIManager::renderSmoothLayout(TFT_eSPI& tft) {
  char timeText[8], seconds[4], weekday[24], dateText[40];
  int hour = clock_.getHour();
  const bool pm = hour >= 12;
  if (!settings_.getSettings().use24Hour) hour = hour % 12 == 0 ? 12 : hour % 12;
  snprintf(timeText, sizeof(timeText), "%02d:%02d", hour, clock_.getMinute());
  snprintf(seconds, sizeof(seconds), "%02d", clock_.getSecond());
  clock_.formatWeekday(weekday, sizeof(weekday));
  clock_.formatDate(dateText, sizeof(dateText));

  tft.setTextSize(1); tft.setTextFont(2); tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
  tft.setTextPadding(250); tft.drawString(weekday, 240, 50); tft.setTextPadding(0);
  tft.setTextFont(7); tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.setTextPadding(350); tft.drawString(timeText, 224, 133); tft.setTextPadding(0);
  tft.setTextFont(4); tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
  tft.setTextPadding(48); tft.drawString(seconds, 422, 135); tft.setTextPadding(0);
  if (!settings_.getSettings().use24Hour) {
    tft.setTextFont(2); tft.drawString(pm ? "PM" : "AM", 422, 163);
  }
  tft.setTextFont(2); tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.setTextPadding(340); tft.drawString(dateText, 240, 211); tft.setTextPadding(0);
  const int dayProgress = (clock_.getHour() * 3600 + clock_.getMinute() * 60 + clock_.getSecond()) * 400 / 86400;
  tft.fillRoundRect(40, 246, 400, 3, 1, currentTheme_.panelAlt);
  if (dayProgress > 0) tft.fillRoundRect(40, 246, dayProgress, 3, 1, currentTheme_.accent);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.drawString("Today, at your pace", 240, 275);
  tft.drawString("Swipe for another face  /  Tap for menu", 240, 307);
}

void UIManager::renderMenu(TFT_eSPI& tft) {
  tft.fillScreen(currentTheme_.bg);
  tft.setTextSize(1);
  tft.setTextFont(4); tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg); tft.drawString("Make yourself at home.", 24, 36);
  drawButton(tft, 410, 16, 46, 40, "X", currentTheme_.panel, currentTheme_.textMuted);
  const char* titles[] = {"Your moments", "Make it yours", "Find your focus", "A fresh view"};
  const char* subtitles[] = {"Countdowns & milestones", "Display & device settings", "Start a focus session", "Switch clock layout"};
  for (int i = 0; i < 4; ++i) {
    const int x = 24 + (i % 2) * 224, y = 78 + (i / 2) * 108;
    tft.fillRoundRect(x, y, 208, 94, 12, currentTheme_.panel);
    tft.fillRoundRect(x + 16, y + 17, 22, 3, 1, currentTheme_.accent);
    tft.setTextDatum(ML_DATUM); tft.setTextFont(4); tft.setTextColor(currentTheme_.text, currentTheme_.panel);
    tft.drawString(titles[i], x + 16, y + 44);
    tft.setTextFont(2); tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.drawString(subtitles[i], x + 16, y + 72);
  }
}

void UIManager::drawPageHeader(TFT_eSPI& tft, const char* eyebrow, const char* title,
                               const char* action) {
  tft.setTextSize(1);
  tft.fillRect(0, 0, kWidth, 55, currentTheme_.bg);
  drawButton(tft, 16, 11, 58, 34, "<", currentTheme_.panel, currentTheme_.textMuted);
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(currentTheme_.accent, currentTheme_.bg);
  tft.drawString(eyebrow, 88, 15);
  tft.setTextFont(4);
  tft.setTextColor(currentTheme_.text, currentTheme_.bg);
  tft.drawString(title, 88, 36);
  if (action != nullptr) {
    drawButton(tft, 374, 11, 90, 34, action, currentTheme_.accent,
               currentTheme_.textOnAccent);
  }
}

void UIManager::drawSettingValue(TFT_eSPI& tft, int y, const char* label,
                                 const char* value) {
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString(label, 34, y + 20);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.drawString(value, 434, y + 20);
  tft.drawFastHLine(34, y + 35, 400, currentTheme_.border);
}

void UIManager::renderCountdowns(TFT_eSPI& tft, const TouchCal::Sample& touch) {
  drawPageHeader(tft, "GOOD THINGS TAKE TIME", "Your moments", "+ Add");

  int total = settings_.getCountdownCount();
  const Countdown* cds = settings_.getCountdowns();

  if (total == 0) {
    fillRoundedRect(tft, 16, 64, kWidth - 32, 218, 12, currentTheme_.panel);
    tft.fillCircle(kWidth / 2, 123, 22, currentTheme_.panelAlt);
    tft.drawCircle(kWidth / 2, 123, 10, currentTheme_.accent);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(currentTheme_.text, currentTheme_.panel);
    tft.setTextFont(4);
    tft.drawString("Make room for a good thing.", kWidth / 2, 174);
    tft.setTextFont(2);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.drawString("Add a milestone or a moment worth counting down to.", kWidth / 2, 207);
    tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
    tft.drawString("TAP + ADD TO BEGIN", kWidth / 2, 246);
    return;
  }

  int startIndex = countdownPage_ * 3;
  for (int i = 0; i < 3 && (startIndex + i) < total; i++) {
    int idx = startIndex + i;
    const Countdown& cd = cds[idx];
    int y = 60 + i * 70;

    fillRoundedRect(tft, 16, y, kWidth - 32, 64, 10, currentTheme_.panel);

    // Accent pill
    uint16_t color = cd.accentColor != 0 ? rgbTo565(cd.accentColor) : currentTheme_.accent;
    tft.fillRoundRect(28, y + 13, 4, 38, 2, color);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(currentTheme_.text, currentTheme_.panel);
    tft.setTextFont(4);
    String title = cd.name;
    while (tft.textWidth(title) > 300 && title.length()) title.remove(title.length() - 1);
    tft.drawString(title, 46, y + 23);

    char remStr[64];
    countdown_.formatRemaining(cd.targetDate, remStr, sizeof(remStr), false);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.setTextFont(2);
    tft.drawString(remStr, 46, y + 45);

    // Delete [X] button
    drawButton(tft, 410, y + 12, 36, 40, "x", currentTheme_.panelAlt, currentTheme_.textMuted);
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
  drawPageHeader(tft, "MAKE IT A MOMENT",
                 strlen(editingCountdownId_) > 0 ? "Edit your moment" : "New countdown",
                 "Save");
  fillRoundedRect(tft, 16, 58, kWidth - 32, 244, 12, currentTheme_.panel);

  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString("EVENT NAME", 30, 72);
  drawButton(tft, 30, 85, 42, 34, "<", currentTheme_.panelAlt, currentTheme_.textMuted);
  drawButton(tft, 408, 85, 42, 34, ">", currentTheme_.panelAlt, currentTheme_.textMuted);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  String eventName = editName_;
  while (tft.textWidth(eventName) > 300 && eventName.length()) eventName.remove(eventName.length() - 1);
  tft.drawString(eventName, 240, 102);

  tft.drawFastHLine(30, 126, 420, currentTheme_.border);
  tft.setTextDatum(ML_DATUM); tft.setTextFont(2);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString("DATE", 30, 145);
  drawButton(tft, 100, 130, 32, 30, "-", currentTheme_.panelAlt, currentTheme_.textMuted);
  char year[8]; snprintf(year, sizeof(year), "%d", editYear_);
  tft.setTextDatum(MC_DATUM); tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.drawString(year, 160, 145);
  drawButton(tft, 190, 130, 32, 30, "+", currentTheme_.panelAlt, currentTheme_.textMuted);
  drawButton(tft, 230, 130, 32, 30, "-", currentTheme_.panelAlt, currentTheme_.textMuted);
  tft.drawString(kMonthNames[(editMonth_ - 1 + 12) % 12], 285, 145);
  drawButton(tft, 310, 130, 32, 30, "+", currentTheme_.panelAlt, currentTheme_.textMuted);
  drawButton(tft, 350, 130, 32, 30, "-", currentTheme_.panelAlt, currentTheme_.textMuted);
  char day[4]; snprintf(day, sizeof(day), "%02d", editDay_);
  tft.drawString(day, 400, 145);
  drawButton(tft, 420, 130, 32, 30, "+", currentTheme_.panelAlt, currentTheme_.textMuted);

  tft.drawFastHLine(30, 166, 420, currentTheme_.border);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString("TIME", 30, 184);
  drawButton(tft, 100, 169, 32, 30, "-", currentTheme_.panelAlt, currentTheme_.textMuted);
  char hour[4]; snprintf(hour, sizeof(hour), "%02d", editHour_);
  tft.setTextDatum(MC_DATUM); tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.drawString(hour, 145, 184);
  drawButton(tft, 170, 169, 32, 30, "+", currentTheme_.panelAlt, currentTheme_.textMuted);
  drawButton(tft, 210, 169, 32, 30, "-", currentTheme_.panelAlt, currentTheme_.textMuted);
  char minute[4]; snprintf(minute, sizeof(minute), "%02d", editMinute_);
  tft.drawString(minute, 255, 184);
  drawButton(tft, 280, 169, 32, 30, "+", currentTheme_.panelAlt, currentTheme_.textMuted);
  drawButton(tft, 320, 169, 122, 30, editHasTime_ ? "Time on" : "No time",
             editHasTime_ ? currentTheme_.accent : currentTheme_.panelAlt,
             editHasTime_ ? currentTheme_.textOnAccent : currentTheme_.textMuted);

  tft.drawFastHLine(30, 207, 420, currentTheme_.border);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString("FEATURE ON CLOCK DISPLAY", 30, 227);
  drawButton(tft, 360, 211, 82, 31, editShowOnMain_ ? "Yes" : "No",
             editShowOnMain_ ? currentTheme_.accent : currentTheme_.panelAlt,
             editShowOnMain_ ? currentTheme_.textOnAccent : currentTheme_.textMuted);

  if (strlen(editingCountdownId_) > 0) {
    drawButton(tft, 140, 263, 200, 29, "Remove countdown", currentTheme_.panelAlt, currentTheme_.danger);
  } else {
    tft.setTextDatum(MC_DATUM); tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.drawString("A little something to look forward to.", 240, 278);
  }
}

void UIManager::renderSettings(TFT_eSPI& tft, const TouchCal::Sample& touch) {
  drawPageHeader(tft, "A CLOCK THAT FEELS LIKE YOU", "Make it yours");
  fillRoundedRect(tft, 16, 58, kWidth - 32, 238, 12, currentTheme_.panel);
  const Settings& s = settings_.getSettings();
  drawSettingValue(tft, 56, "TIMEZONE", s.timezone);
  drawSettingValue(tft, 92, "TIME FORMAT", s.use24Hour ? "24 hour  >" : "12 hour / AM PM  >");
  drawSettingValue(tft, 128, "CLOCK LAYOUT", kLayoutNames[s.layoutId % LAYOUT_COUNT]);
  drawSettingValue(tft, 164, "COLOR PALETTE", kThemeNames[s.themeId % THEME_COUNT]);

  tft.setTextDatum(ML_DATUM); tft.setTextFont(2);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString("BRIGHTNESS", 34, 223);
  drawButton(tft, 250, 205, 48, 36, "-", currentTheme_.panelAlt, currentTheme_.textMuted);
  char brightness[8]; snprintf(brightness, sizeof(brightness), "%d", s.brightness);
  tft.setTextDatum(MC_DATUM); tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.drawString(brightness, 350, 223);
  drawButton(tft, 402, 205, 48, 36, "+", currentTheme_.panelAlt, currentTheme_.textMuted);

  drawButton(tft, 24, 250, 200, 36, "Wi-Fi & connection", currentTheme_.panelAlt, currentTheme_.text);
  drawButton(tft, 256, 250, 200, 36, "About this Clocky", currentTheme_.panelAlt, currentTheme_.text);
}

void UIManager::renderTimezone(TFT_eSPI& tft) {
  drawPageHeader(tft, "TIME & PLACE", "Choose timezone");
  for (size_t row = 0; row < 5; ++row) {
    const size_t index = timezonePage_ * 5 + row;
    if (index >= kTimezoneCount) break;
    const bool selected = strcmp(settings_.getSettings().timezone, kTimezoneTable[index].name) == 0;
    drawButton(tft, 16, 60 + row * 40, 448, 36, kTimezoneTable[index].name,
               selected ? currentTheme_.accent : currentTheme_.panel,
               selected ? currentTheme_.textOnAccent : currentTheme_.text);
  }
  if (timezonePage_ > 0) drawButton(tft, 16, 275, 90, 36, "< Prev", currentTheme_.panel, currentTheme_.text);
  if ((timezonePage_ + 1) * 5 < kTimezoneCount) drawButton(tft, 374, 275, 90, 36, "Next >", currentTheme_.panel, currentTheme_.text);
  char page[24];
  snprintf(page, sizeof(page), "Page %u / %u", timezonePage_ + 1, static_cast<unsigned>((kTimezoneCount + 4) / 5));
  tft.setTextFont(2); tft.setTextDatum(MC_DATUM);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.bg);
  tft.drawString(page, 240, 293);
}

void UIManager::renderWifiSetup(TFT_eSPI& tft) {
  drawPageHeader(tft, "03 / CONNECTION", "Stay connected");
  fillRoundedRect(tft, 16, 58, kWidth - 32, 238, 12, currentTheme_.panel);
  tft.setTextDatum(MC_DATUM); tft.setTextFont(2);
  tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
  if (wifi_ && wifi_->isConnected()) {
    tft.drawString("CONNECTED TO WI-FI", 240, 82);
    tft.setTextColor(currentTheme_.text, currentTheme_.panel);
    String ssid = wifi_->getSSID();
    while (tft.textWidth(ssid) > 392 && ssid.length()) ssid.remove(ssid.length() - 1);
    tft.drawString(ssid, 240, 112);
    tft.drawFastHLine(44, 134, 392, currentTheme_.border);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.drawString("DEVICE IP ADDRESS", 240, 159);
    tft.setTextFont(4);
    tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
    tft.drawString(wifi_->getIP(), 240, 191);
    tft.setTextFont(2);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.drawString("Open this address on the same Wi-Fi network", 240, 226);
    char url[24];
    snprintf(url, sizeof(url), "http://%s", wifi_->getIP());
    tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
    tft.drawString(url, 240, 253);
    tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
    tft.drawString("Tap anywhere to go back", 240, 282);
    return;
  }
  tft.drawString("CLOCKY SETUP HOTSPOT IS READY", 240, 82);
  tft.setTextFont(4); tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.drawString("Let's get you online.", 240, 112);
  tft.drawFastHLine(44, 134, 392, currentTheme_.border);
  tft.setTextFont(2); tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString("1  Join this open Wi-Fi network", 240, 155);
  fillRoundedRect(tft, 100, 172, 280, 36, 7, currentTheme_.panelAlt);
  tft.setTextColor(currentTheme_.text, currentTheme_.panelAlt);
  tft.drawString("CYD-Clock-Setup", 240, 190);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString("2  Then visit this address", 240, 226);
  tft.setTextColor(currentTheme_.accent, currentTheme_.panel);
  tft.setTextFont(4); tft.drawString("192.168.4.1", 240, 254);
  tft.setTextFont(2); tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString("Tap anywhere to go back", 240, 282);
}

void UIManager::renderAbout(TFT_eSPI& tft) {
  drawPageHeader(tft, "04 / YOUR DEVICE", "Small clock. Big heart.");
  fillRoundedRect(tft, 16, 58, kWidth - 32, 238, 12, currentTheme_.panel);
  bool connected = wifi_ && wifi_->isConnected();
  bool apActive = wifi_ && wifi_->isAPActive();
  tft.fillCircle(40, 84, 5, connected ? currentTheme_.accent : currentTheme_.danger);
  tft.setTextDatum(ML_DATUM); tft.setTextFont(2);
  tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.drawString(connected ? "Your Clocky is online" : (apActive ? "Setup hotspot is active" : "Your Clocky is offline"), 56, 84);
  tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString(connected ? wifi_->getIP() : "192.168.4.1", 56, 103);

  char memory[24]; snprintf(memory, sizeof(memory), "%u KB free", ESP.getFreeHeap() / 1024);
  char uptime[32];
  uint32_t elapsed = millis() / 1000;
  snprintf(uptime, sizeof(uptime), "%ud %02uh %02um", elapsed / 86400,
           (elapsed % 86400) / 3600, (elapsed % 3600) / 60);
  drawSettingValue(tft, 118, "FIRMWARE", "Clocky 1.1");
  drawSettingValue(tft, 158, "TIME SYNC", clock_.isTimeSynced() ? "Synchronized" : "Waiting");
  drawSettingValue(tft, 198, "UP AND RUNNING", uptime);
  tft.setTextDatum(ML_DATUM); tft.setTextColor(currentTheme_.textMuted, currentTheme_.panel);
  tft.drawString("FREE MEMORY", 34, 278);
  tft.setTextDatum(MR_DATUM); tft.setTextColor(currentTheme_.text, currentTheme_.panel);
  tft.drawString(memory, 434, 278);
}

void UIManager::drawRoundedRect(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  tft.drawRoundRect(x, y, w, h, r, color);
}

void UIManager::fillRoundedRect(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, r, color);
}

void UIManager::drawButton(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, const char* label, uint16_t bg, uint16_t textColor, bool pressed) {
  tft.setTextSize(1);
  uint16_t fillBg = pressed ? blendColors(bg, 0xFFFF, 0.25f) : bg;
  tft.fillRoundRect(x, y, w, h, 8, fillBg);
  if (bg != currentTheme_.accent) {
    tft.drawRoundRect(x, y, w, h, 8, currentTheme_.border);
  }
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(textColor, fillBg);
  tft.setTextFont(2);
  tft.drawString(label, x + w / 2, y + h / 2);
}

bool UIManager::isInRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  return (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}

void UIManager::applyTheme(uint8_t themeId) {
  const uint32_t palettes[6][6] = {
    {0x242824, 0x30352F, 0x41473E, 0xEDAA7F, 0xF5F4E9, 0x9EA69B},
    {0x2E3440, 0x3B4252, 0x4C566A, 0x88C0D0, 0xECEFF4, 0xAEB9CC},
    {0x282A36, 0x353746, 0x44475A, 0xBD93F9, 0xF8F8F2, 0xB1ACCB},
    {0xF4F0E3, 0xE6E1D2, 0xD5D2C5, 0x376B74, 0x34494B, 0x647C7B},
    {0x000000, 0x181818, 0x242424, 0xFFFF00, 0xFFFFFF, 0xCCCCCC},
    {0x30231E, 0x413129, 0x594539, 0xE9A56B, 0xF8E9D9, 0xBDA692}
  };
  const auto& colors = palettes[themeId % THEME_COUNT];
  currentTheme_.bg = rgbTo565(colors[0]);
  currentTheme_.panel = rgbTo565(colors[1]);
  currentTheme_.panelAlt = rgbTo565(colors[2]);
  currentTheme_.accent = rgbTo565(colors[3]);
  currentTheme_.text = rgbTo565(colors[4]);
  currentTheme_.textMuted = rgbTo565(colors[5]);
  currentTheme_.textOnAccent = themeId == THEME_SOLARIZED_LIGHT ? 0xFFFF : rgbTo565(0x242824);
  currentTheme_.border = rgbTo565(colors[2]);
  currentTheme_.danger = rgbTo565(0xC75B50);
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
