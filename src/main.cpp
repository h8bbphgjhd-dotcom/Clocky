#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "clock_manager.h"
#include "countdown_manager.h"
#include "display_init.h"
#include "pins.h"
#include "settings_manager.h"
#include "touch_map.h"
#include "ui_manager.h"
#include "web_server.h"
#include "wifi_manager.h"

// Global hardware and manager instances
TFT_eSPI tft;
XPT2046_Touchscreen touch(Pins::kTouchCs, Pins::kTouchIrq);

SettingsManager settingsManager;
ClockManager clockManager(settingsManager);
CountdownManager countdownManager(settingsManager);
WiFiManagerWrapper wifiManager;
WebServerManager webServer(settingsManager, clockManager, wifiManager);
UIManager uiManager(settingsManager, clockManager, countdownManager);

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("==========================================");
  Serial.println("CYD Smart Clock & Countdown Display");
  Serial.println("Hardware: ESP32-32E + ST7796S + XPT2046");
  Serial.println("==========================================");

  // STEP 1: Pins & Chip Selects
  Serial.println("[STEP 1/9] Configuring GPIO pins & Chip Selects...");
  pinMode(Pins::kTftBl, OUTPUT);
  digitalWrite(Pins::kTftBl, HIGH);

  pinMode(Pins::kTftCs, OUTPUT);
  pinMode(Pins::kTouchCs, OUTPUT);
  digitalWrite(Pins::kTftCs, HIGH);
  digitalWrite(Pins::kTouchCs, HIGH);
  Serial.println("[STEP 1/9 OK] GPIO pins configured.");

  // STEP 2: SPI Bus
  Serial.println("[STEP 2/9] Initializing shared SPI bus...");
  SPI.begin(Pins::kSpiSck, Pins::kSpiMiso, Pins::kSpiMosi, Pins::kTftCs);
  Serial.println("[STEP 2/9 OK] SPI bus initialized.");

  // STEP 3: Display
  Serial.println("[STEP 3/9] Initializing ST7796S display...");
  tft.init();
  applyManufacturerSt7796Init(tft);
  tft.setRotation(1);
  tft.writecommand(0x36);
  tft.writedata(0x28);
  Serial.println("[STEP 3/9 OK] ST7796S display initialized.");

  // STEP 4: Touch Controller
  Serial.println("[STEP 4/9] Initializing XPT2046 touch controller...");
  touch.begin(SPI);
  Serial.println("[STEP 4/9 OK] XPT2046 touch controller initialized.");

  // STEP 5: Settings & NVS
  Serial.println("[STEP 5/9] Initializing SettingsManager (NVS)...");
  if (settingsManager.begin()) {
    Serial.println("[STEP 5/9 OK] Settings loaded successfully.");
  } else {
    Serial.println("[STEP 5/9 WARN] Settings initialization failed, using defaults.");
  }
  analogWrite(Pins::kTftBl, settingsManager.getSettings().brightness);

  // STEP 6: UI Manager & Splash Screen
  Serial.println("[STEP 6/9] Initializing UIManager & displaying splash...");
  uiManager.setWifiManager(&wifiManager);
  uiManager.begin(tft, touch);
  Serial.println("[STEP 6/9 OK] UIManager initialized.");

  // STEP 7: Wi-Fi Manager
  Serial.println("[STEP 7/9] Initializing Wi-Fi Manager...");
  wifiManager.setStatusCallback([](bool connected) {
    if (connected) {
      Serial.println("[System] Wi-Fi connected - starting Clock Web Server & syncing NTP...");
      webServer.begin();
      clockManager.forceSync();
    } else {
      Serial.println("[System] Wi-Fi disconnected - stopping Clock Web Server to free port 80 for AP...");
      webServer.stop();
    }
  });
  wifiManager.begin();
  Serial.printf("[STEP 7/9 OK] Wi-Fi status: %s (IP: %s)\n",
                wifiManager.isConnected() ? "Connected" : "Hotspot 'CYD-Clock-Setup' active",
                wifiManager.getIP());

  // STEP 8: Clock & NTP
  Serial.println("[STEP 8/9] Initializing ClockManager & NTP...");
  clockManager.begin();
  Serial.printf("[STEP 8/9 OK] ClockManager initialized (Timezone: %s).\n",
                clockManager.getTimezone());

  // STEP 9: Web Server & LittleFS
  Serial.println("[STEP 9/9] Initializing Web Server & LittleFS...");
  if (wifiManager.isConnected()) {
    webServer.begin();
    Serial.println("[STEP 9/9 OK] Web Server initialized.");
  } else {
    Serial.println("[STEP 9/9 OK] Web Server on standby (Wi-Fi setup portal active on port 80).");
  }

  Serial.println("==========================================");
  Serial.println("Setup completed successfully. Entering main loop.");
  Serial.println("==========================================");
}

void loop() {
  wifiManager.loop();
  clockManager.update();
  uiManager.loop(tft, touch);
  webServer.loop();
  delay(16);
}
