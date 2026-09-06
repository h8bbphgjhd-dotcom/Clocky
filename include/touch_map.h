#pragma once

#include <Arduino.h>
#include <cstdint>

// =============================================================================
// TOUCH CALIBRATION — landscape 480×320 (setRotation(1), MADCTL 0x28)
//
// Edit ONLY this block. Use serial/on-screen "raw x=… y=…" from the last test:
//   1. Press the LEFT edge  → put that axis reading in kRawXMin (after swap).
//   2. Press the RIGHT edge → kRawXMax.
//   3. Press the TOP edge   → kRawYMin.
//   4. Press the BOTTOM edge→ kRawYMax.
//
// If left/right follow raw Y instead of raw X, set kSwapAxes = true.
// If an axis is backwards, set kInvertX or kInvertY (do not change GPIOs).
// Starting values are typical XPT2046 12-bit edges (~200–3900).
// =============================================================================

namespace TouchCal {

constexpr int16_t kScreenWidth = 480;
constexpr int16_t kScreenHeight = 320;

constexpr int16_t kRawXMin = 250;   // landscape left
constexpr int16_t kRawXMax = 3850;  // landscape right
constexpr int16_t kRawYMin = 250;   // landscape top
constexpr int16_t kRawYMax = 3850;  // landscape bottom

constexpr bool kSwapAxes = false;
constexpr bool kInvertX = true;
constexpr bool kInvertY = true;

// =============================================================================

struct Sample {
  int16_t rawX = 0;
  int16_t rawY = 0;
  int16_t rawZ = 0;
  int16_t x = 0;
  int16_t y = 0;
  bool pressed = false;
};

inline Sample mapRaw(int16_t rawX, int16_t rawY, int16_t rawZ, bool pressed) {
  Sample sample;
  sample.rawX = rawX;
  sample.rawY = rawY;
  sample.rawZ = rawZ;
  sample.pressed = pressed;
  if (!pressed) {
    return sample;
  }

  int16_t axisX = kSwapAxes ? rawY : rawX;
  int16_t axisY = kSwapAxes ? rawX : rawY;

  long x = map(axisX, kRawXMin, kRawXMax, 0, kScreenWidth - 1);
  long y = map(axisY, kRawYMin, kRawYMax, 0, kScreenHeight - 1);
  if (kInvertX) {
    x = (kScreenWidth - 1) - x;
  }
  if (kInvertY) {
    y = (kScreenHeight - 1) - y;
  }

  sample.x = static_cast<int16_t>(constrain(x, 0L, static_cast<long>(kScreenWidth - 1)));
  sample.y = static_cast<int16_t>(constrain(y, 0L, static_cast<long>(kScreenHeight - 1)));
  return sample;
}

}  // namespace TouchCal
