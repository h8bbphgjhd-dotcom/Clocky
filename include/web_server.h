#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "settings_manager.h"
#include "clock_manager.h"
#include "wifi_manager.h"

class WebServerManager {
 public:
  WebServerManager(SettingsManager& settings, ClockManager& clock, WiFiManagerWrapper& wifi);
  ~WebServerManager();
  
  void begin();
  void stop();
  void loop();
  
 private:
  SettingsManager& settings_;
  ClockManager& clock_;
  WiFiManagerWrapper& wifi_;
  AsyncWebServer* server_ = nullptr;
  AsyncWebSocket* ws_ = nullptr;
  
  void setupRoutes();
  void setupStaticFiles();
  void setupWebSocket();
  
  // API handlers
  static void handleGetSettings(AsyncWebServerRequest* request);
  static void handlePutSettings(AsyncWebServerRequest* request, JsonVariant& json);
  static void handleGetCountdowns(AsyncWebServerRequest* request);
  static void handlePostCountdown(AsyncWebServerRequest* request, JsonVariant& json);
  static void handlePutCountdown(AsyncWebServerRequest* request, JsonVariant& json);
  static void handleDeleteCountdown(AsyncWebServerRequest* request);
  static void handleGetStatus(AsyncWebServerRequest* request);
  static void handlePostWifiScan(AsyncWebServerRequest* request);
  static void handlePostWifiConnect(AsyncWebServerRequest* request, JsonVariant& json);
  static void handlePostWifiDisconnect(AsyncWebServerRequest* request);
  static void handlePostReboot(AsyncWebServerRequest* request);
  
  // WebSocket
  static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                        AwsEventType type, void* arg, uint8_t* data, size_t len);
  
  // Helpers
  void broadcastTime();
  void broadcastCountdowns();
};

