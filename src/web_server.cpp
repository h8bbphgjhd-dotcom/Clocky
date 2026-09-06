#include "web_server.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include <esp_system.h>

// Static instance pointers for callbacks
static WebServerManager* g_instance = nullptr;

WebServerManager::WebServerManager(SettingsManager& settings, ClockManager& clock, WiFiManagerWrapper& wifi)
  : settings_(settings), clock_(clock), wifi_(wifi) {
  g_instance = this;
}

WebServerManager::~WebServerManager() {
  stop();
  g_instance = nullptr;
}

void WebServerManager::begin() {
  if (server_) return;
  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return;
  }
  
  server_ = new AsyncWebServer(80);
  ws_ = new AsyncWebSocket("/ws");
  
  setupRoutes();
  setupStaticFiles();
  setupWebSocket();
  
  server_->begin();
  Serial.println("Web server started on port 80");
}

void WebServerManager::loop() {
  if (ws_) {
    ws_->cleanupClients();

    static uint32_t lastBroadcast = 0;
    uint32_t now = millis();
    if (now - lastBroadcast >= 1000) {
      lastBroadcast = now;
      if (ws_->count() > 0) {
        broadcastTime();
      }
    }
  }
}

void WebServerManager::stop() {
  if (server_) {
    server_->end();
    delete server_;
    server_ = nullptr;
  }
  if (ws_) {
    delete ws_;
    ws_ = nullptr;
  }
}

void WebServerManager::setupRoutes() {
  // CORS headers
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
  
  // API: Settings
  server_->on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* request) {
    handleGetSettings(request);
  });
  
  server_->on("/api/settings", HTTP_PUT, [](AsyncWebServerRequest* request, JsonVariant& json) {
    handlePutSettings(request, json);
  });
  
  server_->on("/api/settings", HTTP_OPTIONS, [](AsyncWebServerRequest* request) {
    request->send(200);
  });
  
  // API: Countdowns
  server_->on("/api/countdowns", HTTP_GET, [](AsyncWebServerRequest* request) {
    handleGetCountdowns(request);
  });
  
  server_->on("/api/countdowns", HTTP_POST, [](AsyncWebServerRequest* request, JsonVariant& json) {
    handlePostCountdown(request, json);
  });
  
  server_->on("/api/countdowns", HTTP_OPTIONS, [](AsyncWebServerRequest* request) {
    request->send(200);
  });
  
  // Countdown by ID
  server_->on(AsyncURIMatcher::prefix("/api/countdowns/"), HTTP_PUT, [](AsyncWebServerRequest* request, JsonVariant& json) {
    handlePutCountdown(request, json);
  });
  
  server_->on(AsyncURIMatcher::prefix("/api/countdowns/"), HTTP_DELETE, [](AsyncWebServerRequest* request) {
    handleDeleteCountdown(request);
  });
  
  server_->on(AsyncURIMatcher::prefix("/api/countdowns/"), HTTP_OPTIONS, [](AsyncWebServerRequest* request) {
    request->send(200);
  });
  
  // API: Status
  server_->on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    handleGetStatus(request);
  });
  
  // API: WiFi
  server_->on("/api/wifi/scan", HTTP_POST, [](AsyncWebServerRequest* request) {
    handlePostWifiScan(request);
  });
  
  server_->on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest* request, JsonVariant& json) {
    handlePostWifiConnect(request, json);
  });
  
  server_->on("/api/wifi/disconnect", HTTP_POST, [](AsyncWebServerRequest* request) {
    handlePostWifiDisconnect(request);
  });
  
  // API: Reboot
  server_->on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* request) {
    handlePostReboot(request);
  });
  
  // 404 handler for API
  server_->onNotFound([](AsyncWebServerRequest* request) {
    if (request->url().startsWith("/api/")) {
      request->send(404, "application/json", "{\"error\":\"Not found\"}");
    } else {
      request->send(404, "text/html", "<h1>Not Found</h1>");
    }
  });
}

void WebServerManager::setupStaticFiles() {
  // Serve static files from LittleFS
  server_->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  
  // Cache control for static assets
  server_->on("/style.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(LittleFS, "/style.css", "text/css");
    response->addHeader("Cache-Control", "max-age=86400");
    request->send(response);
  });
  
  server_->on("/app.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(LittleFS, "/app.js", "application/javascript");
    response->addHeader("Cache-Control", "max-age=86400");
    request->send(response);
  });
  
  server_->on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(LittleFS, "/manifest.json", "application/json");
    request->send(response);
  });
}

void WebServerManager::setupWebSocket() {
  ws_->onEvent([](AsyncWebSocket* server, AsyncWebSocketClient* client, 
                  AwsEventType type, void* arg, uint8_t* data, size_t len) {
    onWsEvent(server, client, type, arg, data, len);
  });
  server_->addHandler(ws_);
}

void WebServerManager::handleGetSettings(AsyncWebServerRequest* request) {
  JsonDocument doc;
  const Settings& s = g_instance->settings_.getSettings();
  
  doc["timezone"] = s.timezone;
  doc["use24Hour"] = s.use24Hour;
  doc["layoutId"] = s.layoutId;
  doc["themeId"] = s.themeId;
  doc["accentColor"] = s.accentColor;
  doc["autoBrightness"] = s.autoBrightness;
  doc["brightness"] = s.brightness;
  
  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json);
}

void WebServerManager::handlePutSettings(AsyncWebServerRequest* request, JsonVariant& json) {
  JsonObject obj = json.as<JsonObject>();
  Settings s = g_instance->settings_.getSettings();
  bool changed = false;
  
  if (!obj["timezone"].isNull()) {
    strlcpy(s.timezone, obj["timezone"] | "", sizeof(s.timezone));
    g_instance->settings_.setTimezone(s.timezone);
    g_instance->clock_.applyTimezone();
    g_instance->clock_.forceSync();
    changed = true;
  }
  if (!obj["use24Hour"].isNull()) {
    s.use24Hour = obj["use24Hour"] | false;
    g_instance->settings_.setUse24Hour(s.use24Hour);
    changed = true;
  }
  if (!obj["layoutId"].isNull()) {
    s.layoutId = obj["layoutId"] | 0;
    g_instance->settings_.setLayoutId(s.layoutId);
    changed = true;
  }
  if (!obj["themeId"].isNull()) {
    s.themeId = obj["themeId"] | 0;
    g_instance->settings_.setThemeId(s.themeId);
    changed = true;
  }
  if (!obj["accentColor"].isNull()) {
    s.accentColor = obj["accentColor"] | 0x00FFFF;
    g_instance->settings_.setAccentColor(s.accentColor);
    changed = true;
  }
  if (!obj["brightness"].isNull()) {
    s.brightness = obj["brightness"] | 200;
    g_instance->settings_.setBrightness(s.brightness);
    changed = true;
  }
  
  if (changed) {
    g_instance->settings_.saveSettings();
  }
  
  request->send(200, "application/json", "{\"success\":true}");
}

void WebServerManager::handleGetCountdowns(AsyncWebServerRequest* request) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  const Countdown* cds = g_instance->settings_.getCountdowns();
  int count = g_instance->settings_.getCountdownCount();
  
  for (int i = 0; i < count; i++) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = cds[i].id;
    obj["name"] = cds[i].name;
    obj["targetDate"] = cds[i].targetDate;
    obj["hasTime"] = cds[i].hasTime;
    obj["accentColor"] = cds[i].accentColor;
    obj["showOnMain"] = cds[i].showOnMain;
    obj["priority"] = cds[i].priority;
    obj["hideWhenExpired"] = cds[i].hideWhenExpired;
    
    // Add formatted remaining time
    time_t now = time(nullptr);
    if (cds[i].targetDate > now) {
      time_t diff = cds[i].targetDate - now;
      int days = diff / 86400;
      diff %= 86400;
      int hours = diff / 3600;
      diff %= 3600;
      int minutes = diff / 60;
      int seconds = diff % 60;
      
      char remaining[64];
      if (days > 0) {
        snprintf(remaining, sizeof(remaining), "%d DAYS %02d HOURS %02d MIN", days, hours, minutes);
      } else if (hours > 0) {
        snprintf(remaining, sizeof(remaining), "%02d HOURS %02d MIN", hours, minutes);
      } else {
        snprintf(remaining, sizeof(remaining), "%02d MIN %02d SEC", minutes, seconds);
      }
      obj["remaining"] = remaining;
      obj["expired"] = false;
    } else {
      obj["remaining"] = "EXPIRED";
      obj["expired"] = true;
    }
  }
  
  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json);
}

void WebServerManager::handlePostCountdown(AsyncWebServerRequest* request, JsonVariant& json) {
  JsonObject obj = json.as<JsonObject>();
  Countdown cd = {};
  
  strlcpy(cd.name, obj["name"] | "", sizeof(cd.name));
  cd.targetDate = obj["targetDate"] | 0;
  cd.hasTime = obj["hasTime"] | false;
  cd.accentColor = obj["accentColor"] | 0x00FFFF;
  cd.showOnMain = obj["showOnMain"] | true;
  cd.hideWhenExpired = obj["hideWhenExpired"] | false;
  
  if (strlen(cd.name) == 0 || cd.targetDate == 0) {
    request->send(400, "application/json", "{\"error\":\"Name and targetDate required\"}");
    return;
  }
  
  if (g_instance->settings_.addCountdown(cd)) {
    JsonDocument doc;
    doc["success"] = true;
    doc["id"] = cd.id;
    String json;
    serializeJson(doc, json);
    request->send(201, "application/json", json);
    
    // Broadcast to WebSocket clients
    g_instance->broadcastCountdowns();
  } else {
    request->send(400, "application/json", "{\"error\":\"Max countdowns reached\"}");
  }
}

void WebServerManager::handlePutCountdown(AsyncWebServerRequest* request, JsonVariant& json) {
  // Extract ID from URL path
  String url = request->url();
  int lastSlash = url.lastIndexOf('/');
  String id = url.substring(lastSlash + 1);
  
  JsonObject obj = json.as<JsonObject>();
  Countdown cd = {};
  
  strlcpy(cd.name, obj["name"] | "", sizeof(cd.name));
  cd.targetDate = obj["targetDate"] | 0;
  cd.hasTime = obj["hasTime"] | false;
  cd.accentColor = obj["accentColor"] | 0x00FFFF;
  cd.showOnMain = obj["showOnMain"] | true;
  cd.hideWhenExpired = obj["hideWhenExpired"] | false;
  
  if (g_instance->settings_.updateCountdown(id.c_str(), cd)) {
    request->send(200, "application/json", "{\"success\":true}");
    g_instance->broadcastCountdowns();
  } else {
    request->send(404, "application/json", "{\"error\":\"Not found\"}");
  }
}

void WebServerManager::handleDeleteCountdown(AsyncWebServerRequest* request) {
  String url = request->url();
  int lastSlash = url.lastIndexOf('/');
  String id = url.substring(lastSlash + 1);
  
  if (g_instance->settings_.deleteCountdown(id.c_str())) {
    request->send(200, "application/json", "{\"success\":true}");
    g_instance->broadcastCountdowns();
  } else {
    request->send(404, "application/json", "{\"error\":\"Not found\"}");
  }
}

void WebServerManager::handleGetStatus(AsyncWebServerRequest* request) {
  JsonDocument doc;
  
  doc["ip"] = g_instance->wifi_.getIP();
  doc["ssid"] = g_instance->wifi_.getSSID();
  doc["rssi"] = g_instance->wifi_.getRSSI();
  doc["connected"] = g_instance->wifi_.isConnected();
  doc["uptime"] = millis() / 1000;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["timeSynced"] = g_instance->clock_.isTimeSynced();
  doc["timezone"] = g_instance->clock_.getTimezone();
  doc["firmwareVersion"] = "1.0.0";
  
  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json);
}

void WebServerManager::handlePostWifiScan(AsyncWebServerRequest* request) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < n; i++) {
    JsonObject obj = arr.add<JsonObject>();
    obj["ssid"] = WiFi.SSID(i);
    obj["rssi"] = WiFi.RSSI(i);
    obj["encryption"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured";
  }
  WiFi.scanDelete();
  
  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json);
}

void WebServerManager::handlePostWifiConnect(AsyncWebServerRequest* request, JsonVariant& json) {
  JsonObject obj = json.as<JsonObject>();
  const char* ssid = obj["ssid"] | "";
  const char* password = obj["password"] | "";
  
  if (strlen(ssid) == 0) {
    request->send(400, "application/json", "{\"error\":\"SSID required\"}");
    return;
  }
  
  WiFi.begin(ssid, password);
  
  // Wait for connection (max 15 seconds)
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(100);
  }
  
  if (WiFi.isConnected()) {
    JsonDocument doc;
    doc["success"] = true;
    doc["ip"] = WiFi.localIP().toString();
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  } else {
    request->send(400, "application/json", "{\"error\":\"Connection failed\"}");
  }
}

void WebServerManager::handlePostWifiDisconnect(AsyncWebServerRequest* request) {
  g_instance->wifi_.forgetCredentials();
  request->send(200, "application/json", "{\"success\":true}");
}

void WebServerManager::handlePostReboot(AsyncWebServerRequest* request) {
  request->send(200, "application/json", "{\"success\":true}");
  delay(500);
  ESP.restart();
}

void WebServerManager::onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                                 AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WS client #%u connected\n", client->id());
    // Send initial state
    g_instance->broadcastTime();
    g_instance->broadcastCountdowns();
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WS client #%u disconnected\n", client->id());
  }
}

void WebServerManager::broadcastTime() {
  if (!ws_ || ws_->count() == 0) return;
  
  JsonDocument doc;
  doc["type"] = "time";
  
  char timeBuf[32], dateBuf[32], weekdayBuf[32];
  g_instance->clock_.formatTime(timeBuf, sizeof(timeBuf));
  g_instance->clock_.formatDate(dateBuf, sizeof(dateBuf));
  g_instance->clock_.formatWeekday(weekdayBuf, sizeof(weekdayBuf));
  
  doc["time"] = timeBuf;
  doc["date"] = dateBuf;
  doc["weekday"] = weekdayBuf;
  
  String json;
  serializeJson(doc, json);
  ws_->textAll(json);
}

void WebServerManager::broadcastCountdowns() {
  if (!ws_ || ws_->count() == 0) return;
  
  JsonDocument doc;
  doc["type"] = "countdowns";
  JsonArray arr = doc["countdowns"].to<JsonArray>();
  
  const Countdown* cds = g_instance->settings_.getCountdowns();
  int count = g_instance->settings_.getCountdownCount();
  time_t now = time(nullptr);
  
  for (int i = 0; i < count; i++) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = cds[i].id;
    obj["name"] = cds[i].name;
    obj["targetDate"] = cds[i].targetDate;
    obj["hasTime"] = cds[i].hasTime;
    obj["accentColor"] = cds[i].accentColor;
    obj["showOnMain"] = cds[i].showOnMain;
    obj["priority"] = cds[i].priority;
    obj["hideWhenExpired"] = cds[i].hideWhenExpired;
    
    if (cds[i].targetDate > now) {
      time_t diff = cds[i].targetDate - now;
      int days = diff / 86400;
      diff %= 86400;
      int hours = diff / 3600;
      diff %= 3600;
      int minutes = diff / 60;
      int seconds = diff % 60;
      
      char remaining[64];
      if (days > 0) {
        snprintf(remaining, sizeof(remaining), "%d DAYS %02d HOURS %02d MIN", days, hours, minutes);
      } else if (hours > 0) {
        snprintf(remaining, sizeof(remaining), "%02d HOURS %02d MIN", hours, minutes);
      } else {
        snprintf(remaining, sizeof(remaining), "%02d MIN %02d SEC", minutes, seconds);
      }
      obj["remaining"] = remaining;
      obj["expired"] = false;
    } else {
      obj["remaining"] = "EXPIRED";
      obj["expired"] = true;
    }
  }
  
  String json;
  serializeJson(doc, json);
  ws_->textAll(json);
}
