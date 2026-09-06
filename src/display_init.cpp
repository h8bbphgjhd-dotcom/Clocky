#include "display_init.h"

#include <Arduino.h>

void applyManufacturerSt7796Init(TFT_eSPI& tft) {
  tft.writecommand(0x11);
  delay(120);

  tft.writecommand(0x36);
  tft.writedata(0x48);

  tft.writecommand(0x3A);
  tft.writedata(0x55);

  tft.writecommand(0xF0);
  tft.writedata(0xC3);

  tft.writecommand(0xF0);
  tft.writedata(0x96);

  tft.writecommand(0xB4);
  tft.writedata(0x01);

  tft.writecommand(0xB7);
  tft.writedata(0xC6);

  tft.writecommand(0xC0);
  tft.writedata(0x80);
  tft.writedata(0x45);

  tft.writecommand(0xC1);
  tft.writedata(0x13);

  tft.writecommand(0xC2);
  tft.writedata(0xA7);

  tft.writecommand(0xC5);
  tft.writedata(0x20);

  tft.writecommand(0xE8);
  tft.writedata(0x40);
  tft.writedata(0x8A);
  tft.writedata(0x00);
  tft.writedata(0x00);
  tft.writedata(0x29);
  tft.writedata(0x19);
  tft.writedata(0xA5);
  tft.writedata(0x33);

  tft.writecommand(0xE0);
  tft.writedata(0xD0);
  tft.writedata(0x08);
  tft.writedata(0x0F);
  tft.writedata(0x06);
  tft.writedata(0x06);
  tft.writedata(0x33);
  tft.writedata(0x30);
  tft.writedata(0x33);
  tft.writedata(0x47);
  tft.writedata(0x17);
  tft.writedata(0x13);
  tft.writedata(0x13);
  tft.writedata(0x2B);
  tft.writedata(0x31);

  tft.writecommand(0xE1);
  tft.writedata(0xD0);
  tft.writedata(0x0A);
  tft.writedata(0x11);
  tft.writedata(0x0B);
  tft.writedata(0x09);
  tft.writedata(0x07);
  tft.writedata(0x2F);
  tft.writedata(0x33);
  tft.writedata(0x47);
  tft.writedata(0x38);
  tft.writedata(0x15);
  tft.writedata(0x16);
  tft.writedata(0x2C);
  tft.writedata(0x32);

  tft.writecommand(0xF0);
  tft.writedata(0x3C);

  tft.writecommand(0xF0);
  tft.writedata(0x69);

  delay(120);

  tft.writecommand(0x29);
}
