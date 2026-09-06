#include "clock_ui.h"

#include <cstdio>

namespace {

constexpr uint16_t kBg = 0x0841;
constexpr uint16_t kPanel = 0x10A3;
constexpr uint16_t kAccent = 0x5DFF;
constexpr uint16_t kMuted = 0x8410;
constexpr uint16_t kText = TFT_WHITE;
constexpr uint16_t kMarker = TFT_RED;

constexpr int16_t kWidth = TouchCal::kScreenWidth;
constexpr int16_t kHeight = TouchCal::kScreenHeight;

}  // namespace

void ClockUi::begin(TFT_eSPI& tft) {
  lastSecond_ = -1;
  lastPressed_ = false;
  lastMappedX_ = -1;
  lastMappedY_ = -1;
  drawFrame(tft);
  drawTouchDebug(tft, TouchCal::Sample{});
}

void ClockUi::render(TFT_eSPI& tft, const ClockTime& clock, const TouchCal::Sample& touch) {
  const bool touchChanged =
      touch.pressed != lastPressed_ ||
      (touch.pressed && (touch.x != lastMappedX_ || touch.y != lastMappedY_));

  if (touchChanged && lastPressed_ && lastMappedX_ >= 0) {
    drawFrame(tft);
    drawTime(tft, clock, true);
    drawTouchDebug(tft, touch);
  } else {
    drawTime(tft, clock, false);
    if (touchChanged) {
      drawTouchDebug(tft, touch);
    }
  }

  if (touch.pressed) {
    tft.fillCircle(touch.x, touch.y, 6, kMarker);
  }

  lastPressed_ = touch.pressed;
  lastMappedX_ = touch.x;
  lastMappedY_ = touch.y;
}

void ClockUi::drawFrame(TFT_eSPI& tft) {
  tft.fillScreen(kBg);
  tft.fillRoundRect(16, 16, kWidth - 32, 40, 8, kPanel);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(kAccent, kPanel);
  tft.setTextFont(2);
  tft.drawString("E32R40T  ·  CLOCK", kWidth / 2, 36);

  tft.fillRoundRect(16, 68, kWidth - 32, 168, 12, kPanel);
  tft.fillRoundRect(16, 248, kWidth - 32, 56, 8, kPanel);
}

void ClockUi::drawTime(TFT_eSPI& tft, const ClockTime& clock, bool force) {
  const int second = clock.second();
  if (!force && second == lastSecond_) {
    return;
  }
  lastSecond_ = second;

  char timeBuf[16];
  char dateBuf[16];
  clock.formatTime(timeBuf, sizeof(timeBuf));
  clock.formatDate(dateBuf, sizeof(dateBuf));

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(kText, kPanel);
  tft.setTextFont(7);
  tft.drawString(timeBuf, kWidth / 2, 130);

  tft.setTextColor(kMuted, kPanel);
  tft.setTextFont(4);
  tft.drawString(dateBuf, kWidth / 2, 196);
}

void ClockUi::drawTouchDebug(TFT_eSPI& tft, const TouchCal::Sample& touch) {
  tft.fillRoundRect(16, 248, kWidth - 32, 56, 8, kPanel);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);

  if (!touch.pressed) {
    tft.setTextColor(kMuted, kPanel);
    tft.drawString("Touch a corner to check mapping", kWidth / 2, 264);
    tft.drawString("raw + mapped shown here  |  edit include/touch_map.h", kWidth / 2, 284);
    return;
  }

  char line1[72];
  char line2[72];
  snprintf(line1, sizeof(line1), "raw  x=%d  y=%d  z=%d", touch.rawX, touch.rawY, touch.rawZ);
  snprintf(line2, sizeof(line2), "mapped  x=%d  y=%d   (0,0 top-left  %dx%d)", touch.x, touch.y,
           kWidth, kHeight);
  tft.setTextColor(kAccent, kPanel);
  tft.drawString(line1, kWidth / 2, 264);
  tft.drawString(line2, kWidth / 2, 284);
}
