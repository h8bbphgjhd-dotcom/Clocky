#pragma once

// LCDWiki/Hosyond 4.0-inch ESP32-32E E32R40T
// Display and XPT2046 share one SPI bus; each device has its own CS.

namespace Pins {

constexpr int kSpiSck = 14;
constexpr int kSpiMosi = 13;
constexpr int kSpiMiso = 12;

constexpr int kTftCs = 15;
constexpr int kTftDc = 2;
constexpr int kTftRst = -1;  // tied to ESP32 EN; do not drive GPIO 12 as reset
constexpr int kTftBl = 27;

constexpr int kTouchCs = 33;
constexpr int kTouchIrq = 36;  // active-low, input-only

}  // namespace Pins
