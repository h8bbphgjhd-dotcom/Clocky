#pragma once

#include <Arduino.h>
#include <WiFi.h>

class WiFiManagerWrapper {
 public:
  WiFiManagerWrapper();
  ~WiFiManagerWrapper();
  
  void begin();
  void loop();
  
  // Start configuration portal (AP mode)
  void startConfigPortal();
  
  // Get status
  bool isConnected() const { return WiFi.isConnected(); }
  bool isAPActive() const { return (WiFi.getMode() & WIFI_AP); }
  const char* getIP() const;
  const char* getSSID() const;
  int8_t getRSSI() const;
  
  // Disconnect and forget credentials
  void forgetCredentials();
  
  // Callback for status changes
  using StatusCallback = void (*)(bool connected);
  void setStatusCallback(StatusCallback cb) { statusCallback_ = cb; }

 private:
  class WiFiManager* wifiManager_ = nullptr;
  StatusCallback statusCallback_ = nullptr;
  bool wasConnected_ = false;
  uint32_t lastRetry_ = 0;
  static constexpr uint32_t RETRY_INTERVAL_MS = 30000;
  
  void checkConnection();
};

