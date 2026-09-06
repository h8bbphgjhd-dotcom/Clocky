# E32R40T clock

Landscape clock for the **LCDWiki/Hosyond 4.0-inch ESP32-32E E32R40T**. ST7796S + XPT2046 on one SPI bus. No LVGL.

Bring-up is unchanged: backlight GPIO 27 HIGH, `SPI.begin(14, 12, 13, 15)`, manufacturer ST7796 init, rotation 1, MADCTL `0x28`. Touch stays on CS 33 / IRQ 36.

Tune mapping in **`include/touch_map.h`** from serial `raw x=… y=…`. Press corners; the red marker and `mapped x,y` should land near `(0,0)`, `(479,0)`, `(0,319)`, `(479,319)`.

```bash
pio run -t upload
pio device monitor
```
