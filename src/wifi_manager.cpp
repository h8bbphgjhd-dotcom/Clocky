#include "wifi_manager.h"
#include <WiFiManager.h>
#include <WiFi.h>

WiFiManagerWrapper::WiFiManagerWrapper() {}

WiFiManagerWrapper::~WiFiManagerWrapper() {
  if (wifiManager_) {
    delete wifiManager_;
  }
}

void WiFiManagerWrapper::begin() {
  wifiManager_ = new WiFiManager();

  // Non-blocking mode: AP runs in the background without halting the clock or timing out
  wifiManager_->setConfigPortalBlocking(false);
  wifiManager_->setBreakAfterConfig(true);

  // Set callback for when config portal starts
  wifiManager_->setAPCallback([](WiFiManager* wm) {
    Serial.println("[WiFi] AP Mode started: SSID 'CYD-Clock-Setup' (IP: 192.168.4.1)");
  });

  // Set callback for when connected
  wifiManager_->setSaveConfigCallback([]() {
    Serial.println("[WiFi] Wi-Fi configuration saved! Connecting...");
  });

  // Try to auto-connect with saved credentials; if none, starts AP "CYD-Clock-Setup"
  bool res = wifiManager_->autoConnect("CYD-Clock-Setup");

  if (res) {
    Serial.println("[WiFi] Connected to saved network.");
    Serial.print("[WiFi] Local IP: ");
    Serial.println(WiFi.localIP());
    wasConnected_ = true;
    if (statusCallback_) statusCallback_(true);
  } else {
    Serial.println("[WiFi] Hotspot active: 'CYD-Clock-Setup' at 192.168.4.1");
    wasConnected_ = false;
    if (statusCallback_) statusCallback_(false);
  }
}

void WiFiManagerWrapper::loop() {
  checkConnection();

  // Process WiFiManager (handles DNS and HTTP for captive portal in non-blocking mode)
  if (wifiManager_) {
    wifiManager_->process();
  }
}

void WiFiManagerWrapper::checkConnection() {
  bool connected = WiFi.isConnected();

  if (connected != wasConnected_) {
    wasConnected_ = connected;
    Serial.printf("[WiFi] State changed: %s\n", connected ? "CONNECTED" : "DISCONNECTED");
    if (connected) {
      Serial.print("[WiFi] IP: ");
      Serial.println(WiFi.localIP());
    }
    if (statusCallback_) statusCallback_(connected);
  }

  // Retry connection periodically if not connected and not serving AP
  if (!connected && !isAPActive()) {
    uint32_t now = millis();
    if (now - lastRetry_ > RETRY_INTERVAL_MS) {
      lastRetry_ = now;
      WiFi.reconnect();
    }
  }
}

void WiFiManagerWrapper::startConfigPortal() {
  Serial.println("[WiFi] Starting AP config portal: CYD-Clock-Setup...");
  if (wifiManager_) {
    wifiManager_->setConfigPortalBlocking(false);
    wifiManager_->startConfigPortal("CYD-Clock-Setup");
  }
}

const char* WiFiManagerWrapper::getIP() const {
  static char ipStr[16];
  if (WiFi.isConnected()) {
    snprintf(ipStr, sizeof(ipStr), "%s", WiFi.localIP().toString().c_str());
    return ipStr;
  }
  return "0.0.0.0";
}

const char* WiFiManagerWrapper::getSSID() const {
  static char ssidStr[33];
  strlcpy(ssidStr, WiFi.SSID().c_str(), sizeof(ssidStr));
  return ssidStr;
}

int8_t WiFiManagerWrapper::getRSSI() const {
  return WiFi.RSSI();
}

void WiFiManagerWrapper::forgetCredentials() {
  if (wifiManager_) {
    wifiManager_->resetSettings();
  }
  WiFi.disconnect(true);
  wasConnected_ = false;
  startConfigPortal();
}
