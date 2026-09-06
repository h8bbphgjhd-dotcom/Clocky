#pragma once

#include <TFT_eSPI.h>

#include "clock_time.h"
#include "touch_map.h"

class ClockUi {
 public:
  void begin(TFT_eSPI& tft);
  void render(TFT_eSPI& tft, const ClockTime& clock, const TouchCal::Sample& touch);

 private:
  void drawFrame(TFT_eSPI& tft);
  void drawTime(TFT_eSPI& tft, const ClockTime& clock, bool force);
  void drawTouchDebug(TFT_eSPI& tft, const TouchCal::Sample& touch);

  int lastSecond_ = -1;
  bool lastPressed_ = false;
  int16_t lastMappedX_ = -1;
  int16_t lastMappedY_ = -1;
};
