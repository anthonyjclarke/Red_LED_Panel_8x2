#include "PanelNetwork.h"

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <atomic>
#include <esp_sntp.h>
#include <sys/time.h>

#include "ConfigPage.h"
#include "config.h"
#include "debug.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h" // Shared default setup password; no page login.
#endif
#ifdef OTA_UPLOAD_BUILD
// An OTA upload without a password would install firmware that refuses further OTA uploads.
static_assert(sizeof(SECRET_OTA_PASSWORD) > 1,
              "esp32dev_ota needs SECRET_OTA_PASSWORD in include/secrets.h (same as the panel's)");
#endif

namespace {
std::atomic<bool> syncPending{false};

void timeSynced(timeval*) {
  // SNTP calls from a network task; keep logging and application work in loop().
  syncPending.store(true);
}

String jsonString(const char* text) {
  String result = "\"";
  for (const uint8_t* p = reinterpret_cast<const uint8_t*>(text); *p; ++p) {
    if (*p == '"' || *p == '\\') result += '\\';
    if (*p < 0x20) {
      char escaped[7];
      snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
      result += escaped;
    } else {
      result += static_cast<char>(*p);
    }
  }
  return result + '"';
}

// Restyles WiFiManager's own pages (/wifi, /info, ...) to match the panel UI and
// adds a navigation bar back to it. Injected into each page's <head>.
const char WIFI_PAGE_HEAD[] PROGMEM =
    "<style>body{margin:0;background:#fcfbf8;color:#1d2933;text-align:left;"
    "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif}"
    ".wrap{display:block;max-width:560px;margin:24px auto;padding:0 16px}"
    "h1,h2,h3{color:#14232d}a{color:#a51f30}input,select{border:1px solid #b9c7cd;"
    "border-radius:6px;padding:10px}button,input[type=submit]{background:#b92535;"
    "border-radius:6px;font-size:1rem;line-height:2.6rem}"
    ".rp-bar{display:flex;flex-wrap:wrap;align-items:center;gap:6px 18px;padding:12px 16px;"
    "background:#14232d}.rp-bar b{color:#fff;font-size:18px;margin-right:6px}"
    ".rp-bar a{color:#d1dde4;font-size:14px;font-weight:600}</style>"
    "<script>document.addEventListener('DOMContentLoaded',function(){"
    "var n=document.createElement('nav');n.className='rp-bar';"
    "n.innerHTML=\"<b>RED / 8x2</b><a href='/'>Panel settings</a>"
    "<a href='/wifi'>Wi-Fi network</a><a href='/info'>System info</a>\";"
    "document.body.insertBefore(n,document.body.firstChild);});</script>";

void reply(WebServer& server, int code, const char* message) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", "{\"message\":" + jsonString(message) + "}");
}
} // namespace

void PanelNetwork::begin() {
  settings_ = defaultClockSettings(DEFAULT_BRIGHTNESS);
  preferencesReady_ = preferences_.begin("redpanel", false);
  if (preferencesReady_ && preferences_.getBytesLength("clock") == sizeof(ClockSettings)) {
    ClockSettings saved = {};
    if (preferences_.getBytes("clock", &saved, sizeof(saved)) == sizeof(saved) &&
        validClockSettings(saved)) {
      settings_ = saved;
      DBG_INFO("[settings] Loaded location, time zone and display defaults from NVS");
    } else {
      DBG_WARN("[settings] Invalid stored settings; using defaults");
    }
  } else if (!preferencesReady_) {
    DBG_ERROR("[settings] NVS unavailable; settings will not survive a reset");
  }
  const auto* zone = findTimeZone(settings_.zone);
  setenv("TZ", zone->rule, 1);
  tzset();
  snprintf(hostname_, sizeof(hostname_), "red-panel-%06x",
           static_cast<uint32_t>(ESP.getEfuseMac()) & 0xffffff);
  snprintf(apName_, sizeof(apName_), "%s%06X", WIFI_AP_PREFIX,
           static_cast<uint32_t>(ESP.getEfuseMac()) & 0xffffff);
  snprintf(formToken_, sizeof(formToken_), "%08x%08x", esp_random(), esp_random());
  manager_.setDebugOutput(false); // Library debug can include Wi-Fi passwords.
  manager_.setConfigPortalBlocking(false);
  manager_.setConfigPortalTimeout(0);
  manager_.setConnectTimeout(WIFI_CONNECT_TIMEOUT_MS / 1000);
  manager_.setSaveConnectTimeout(WIFI_SAVE_CONNECT_TIMEOUT_MS / 1000);
  manager_.setWiFiAutoReconnect(true);
  manager_.setTitle("Red LED Panel");
  std::vector<const char*> menu = {"wifi", "custom", "info", "exit"};
  manager_.setMenu(menu);
  manager_.setCustomMenuHTML(
      "<form action='/panel' method='get'><button>Panel settings</button></form>");
  manager_.setShowInfoErase(false);
  manager_.setCustomHeadElement(WIFI_PAGE_HEAD);
  manager_.setHostname(hostname_);
  manager_.setWebServerCallback([this]() { registerRoutes(); });
  manager_.setAPCallback([this](WiFiManager*) {
    DBG_INFO("[wifi] Setup hotspot: %s", apName_);
    DBG_INFO("[wifi] Join the hotspot, then open http://192.168.4.1/ (panel settings: /panel)");
  });
  sntp_set_time_sync_notification_cb(timeSynced);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname_);
  WiFi.setAutoReconnect(true);
  WiFi.begin(); // Asynchronous attempt with credentials saved by WiFiManager.
  connectStarted_ = lastRetry_ = millis();
  reconnectAttempt_ = true;
  DBG_INFO("[wifi] Connecting to saved network; setup hotspot opens after %lu seconds if needed",
           static_cast<unsigned long>(WIFI_CONNECT_TIMEOUT_MS / 1000));
  if (!*SECRET_OTA_PASSWORD)
    DBG_INFO("[ota] Disabled: set SECRET_OTA_PASSWORD in include/secrets.h");
}

bool PanelNetwork::connected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool PanelNetwork::portalActive() {
  return manager_.getConfigPortalActive();
}

String PanelNetwork::address() const {
  return connected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
}

bool PanelNetwork::localTime(tm& result, uint16_t* milliseconds) const {
  if (!timeValid_) return false;
  timeval now = {};
  gettimeofday(&now, nullptr);
  if (milliseconds) *milliseconds = now.tv_usec / 1000;
  const time_t seconds = now.tv_sec;
  return seconds >= 1704067200 && localtime_r(&seconds, &result) != nullptr;
}

bool PanelNetwork::takeSettingsChanged() {
  const bool changed = settingsChanged_;
  settingsChanged_ = false;
  return changed;
}

bool PanelNetwork::saveBrightness(uint8_t level) {
  if (level == settings_.brightness) return true;
  ClockSettings next = settings_;
  next.brightness = level;
  if (!preferencesReady_ || preferences_.putBytes("clock", &next, sizeof(next)) != sizeof(next)) {
    DBG_ERROR("[settings] Could not save startup brightness to NVS");
    return false;
  }
  settings_ = next;
  DBG_INFO("[settings] Startup brightness saved: %u/255", level);
  return true;
}

void PanelNetwork::onOtaProgress(std::function<void(int)> handler) {
  otaProgress_ = handler;
}

void PanelNetwork::startOta() {
  // Network uploads are only accepted with a password from secrets.h.
  if (otaStarted_ || !*SECRET_OTA_PASSWORD) return;
  ArduinoOTA.setHostname(hostname_);
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setPassword(SECRET_OTA_PASSWORD);
  ArduinoOTA.setMdnsEnabled(false); // mDNS is owned by loop() so it survives reconnects.
  ArduinoOTA.onStart([this]() {
    otaPercent_ = -1;
    DBG_INFO("[ota] Firmware upload started");
    if (otaProgress_) otaProgress_(0);
  });
  ArduinoOTA.onProgress([this](unsigned int done, unsigned int total) {
    const int percent = total ? static_cast<int>(100ULL * done / total) : 0;
    if (percent == otaPercent_) return;
    otaPercent_ = percent;
    if (percent % 10 == 0) DBG_VERBOSE("[ota] %d%%", percent);
    if (otaProgress_) otaProgress_(percent);
  });
  ArduinoOTA.onEnd([]() { DBG_INFO("[ota] Upload complete; restarting"); });
  ArduinoOTA.onError([this](ota_error_t error) {
    DBG_ERROR("[ota] Upload failed (error %u); continuing with current firmware",
              static_cast<unsigned>(error));
    if (otaProgress_) otaProgress_(-1);
  });
  ArduinoOTA.begin();
  MDNS.enableArduino(OTA_PORT, true);
  otaStarted_ = true;
  DBG_INFO("[ota] Ready: %s.local port %u (password required)", hostname_, OTA_PORT);
}

bool PanelNetwork::authorised() {
  // Optional HTTP Basic login for the panel page and API. WiFiManager's own
  // pages are unaffected; the setup hotspot is protected by its own password.
  if (!*SECRET_PANEL_PASSWORD) return true;
  if (manager_.server->authenticate(PANEL_ADMIN_USER, SECRET_PANEL_PASSWORD)) return true;
  manager_.server->requestAuthentication();
  return false;
}

void PanelNetwork::configureTime() {
  const auto* zone = findTimeZone(settings_.zone);
  configTzTime(zone->rule, NTP_SERVER_1, NTP_SERVER_2);
  lastNtpRequest_ = millis();
  DBG_INFO("[clock] NTP requested; location=%s zone=%s", settings_.location, settings_.zone);
}

void PanelNetwork::requestPortal() {
  portalRequested_ = true;
}

void PanelNetwork::startPortal() {
  reconnectAttempt_ = false;
  lastRetry_ = millis();
  if (manager_.getConfigPortalActive()) return;
  if (manager_.getWebPortalActive()) manager_.stopWebPortal();
  manager_.startConfigPortal(apName_, SECRET_AP_PASSWORD);
}

void PanelNetwork::loop() {
  manager_.process();
  if (portalRequested_) {
    portalRequested_ = false;
    startPortal();
  }
  const uint32_t now = millis();
  const bool online = connected();
  if (online) {
    if (!previousConnected_) {
      if (manager_.getConfigPortalActive()) manager_.stopConfigPortal();
      DBG_INFO("[wifi] Connected: SSID=%s IP=%s RSSI=%d dBm", WiFi.SSID().c_str(),
               WiFi.localIP().toString().c_str(), WiFi.RSSI());
      if (MDNS.begin(hostname_)) {
        MDNS.addService("http", "tcp", 80);
        if (otaStarted_) MDNS.enableArduino(OTA_PORT, true);
      } else {
        DBG_WARN("[wifi] mDNS unavailable; use the IP address");
      }
      startOta();
      DBG_INFO("[web] Settings: http://%s/panel or http://%s.local/panel", address().c_str(),
               hostname_);
      configureTime();
    }
    reconnectAttempt_ = false;
    if (!manager_.getConfigPortalActive() && !manager_.getWebPortalActive())
      manager_.startWebPortal();
    // Fresh millis(): a request made above, after `now` was read, must not wrap to a retry.
    if (!timeValid_ && millis() - lastNtpRequest_ >= NTP_RETRY_MS) configureTime();
  } else {
    if (previousConnected_) {
      DBG_WARN("[wifi] Disconnected; %s",
               timeValid_ ? "clock continues from last sync" : "waiting for network time");
      MDNS.end();
      connectStarted_ = lastRetry_ = now;
      reconnectAttempt_ = true;
      WiFi.reconnect();
    }
    if (reconnectAttempt_ && now - connectStarted_ >= WIFI_CONNECT_TIMEOUT_MS) startPortal();
    if (!reconnectAttempt_ && now - lastRetry_ >= WIFI_RETRY_MS &&
        WiFi.softAPgetStationNum() == 0) {
      WiFi.begin();
      connectStarted_ = lastRetry_ = now;
      reconnectAttempt_ = true;
      DBG_VERBOSE("[wifi] Retrying saved network");
    }
  }
  previousConnected_ = online;
  if (otaStarted_) ArduinoOTA.handle(); // Blocks for the duration of an accepted upload.
  if (syncPending.exchange(false)) {
    lastSync_ = time(nullptr);
    lastSyncMs_ = now;
    timeValid_ = lastSync_ >= 1704067200;
    if (timeValid_) {
      tm local = {};
      localtime_r(&lastSync_, &local);
      char text[48];
      strftime(text, sizeof(text), "%d-%b-%Y %H:%M:%S %Z", &local);
      DBG_INFO("[clock] Synchronised: %s", text);
    }
  } else if (timeValid_ && now - lastSyncMs_ >= CLOCK_HOLDOVER_MAX_MS) {
    timeValid_ = false;
    DBG_WARN("[clock] No NTP sync for %lu hours; holdover ended, waiting for network time",
             static_cast<unsigned long>(CLOCK_HOLDOVER_MAX_MS / 3600000UL));
  }
}

void PanelNetwork::registerRoutes() {
  auto& server = *manager_.server;
  // Registered before WiFiManager's own routes, so "/" opens the panel UI on the
  // LAN and in the setup hotspot; its Wi-Fi pages stay linked from the sidebar.
  const auto page = [this]() {
    if (!authorised()) return;
    manager_.server->sendHeader("Cache-Control", "no-store");
    manager_.server->send_P(200, "text/html; charset=utf-8", CONFIG_PAGE);
  };
  server.on("/", HTTP_GET, page);
  server.on("/panel", HTTP_GET, page);
  server.on("/api/status", HTTP_GET, [this]() {
    if (authorised()) sendStatus();
  });
  server.on("/api/settings", HTTP_POST, [this]() {
    if (authorised()) saveSettings();
  });
  server.on("/api/sync", HTTP_POST, [this]() {
    if (!authorised()) return;
    if (manager_.server->arg("token") != formToken_) {
      reply(*manager_.server, 403, "Reload the settings page and try again.");
      return;
    }
    if (!connected()) {
      reply(*manager_.server, 409, "Connect Wi-Fi before requesting network time.");
      return;
    }
    configureTime();
    reply(*manager_.server, 200, "Network time requested.");
  });
  server.on("/api/wifi", HTTP_POST, [this]() {
    if (!authorised()) return;
    if (manager_.server->arg("token") != formToken_) {
      reply(*manager_.server, 403, "Reload the settings page and try again.");
      return;
    }
    reply(*manager_.server, 200,
          "Setup hotspot requested; use the Wi-Fi network button to change credentials.");
    requestPortal(); // Recreate the server only after this handler has returned.
  });
}

void PanelNetwork::saveSettings() {
  auto& server = *manager_.server;
  if (server.arg("token") != formToken_) {
    reply(server, 403, "Reload the settings page and try again.");
    return;
  }
  String location = server.arg("location");
  location.trim();
  const String zone = server.arg("zone");
  const String view = server.arg("view");
  uint8_t requestedBrightness = 0;
  if (!validLocation(location.c_str()) || !findTimeZone(zone.c_str()) ||
      !parseBrightness(server.arg("brightness").c_str(), requestedBrightness) ||
      (view != "clock" && view != "playlist")) {
    reply(server, 400,
          "Enter a location of 1-32 bytes, a listed time zone, brightness 0-255, and a startup "
          "view.");
    return;
  }
  ClockSettings next = {};
  next.schema = 1;
  strlcpy(next.location, location.c_str(), sizeof(next.location));
  strlcpy(next.zone, zone.c_str(), sizeof(next.zone));
  next.brightness = requestedBrightness;
  next.startClock = view == "clock";
  if (!preferencesReady_ || preferences_.putBytes("clock", &next, sizeof(next)) != sizeof(next)) {
    DBG_ERROR("[settings] Could not save settings to NVS");
    reply(server, 500, "Settings could not be saved; existing settings are unchanged.");
    return;
  }
  settings_ = next;
  setenv("TZ", findTimeZone(settings_.zone)->rule, 1);
  tzset();
  settingsChanged_ = true;
  DBG_INFO("[settings] Saved: location=%s zone=%s brightness=%u view=%s", settings_.location,
           settings_.zone, settings_.brightness, settings_.startClock ? "clock" : "playlist");
  reply(server, 200, "Saved and applied. These settings will also be used after restart.");
}

void PanelNetwork::sendStatus() {
  String json;
  json.reserve(2400);
  json = "{\"version\":" + jsonString(FIRMWARE_VERSION);
  json += ",\"token\":" + jsonString(formToken_);
  json += ",\"location\":" + jsonString(settings_.location);
  json += ",\"zone\":" + jsonString(settings_.zone);
  json += ",\"brightness\":" + String(settings_.brightness);
  json += ",\"startClock\":" + String(settings_.startClock ? "true" : "false");
  json += ",\"connected\":" + String(connected() ? "true" : "false");
  json += ",\"timeValid\":" + String(timeValid_ ? "true" : "false");
  json += ",\"ssid\":" + jsonString(connected() ? WiFi.SSID().c_str() : "");
  json += ",\"rssi\":" + String(connected() ? WiFi.RSSI() : 0);
  json += ",\"url\":" + jsonString(("http://" + address() + "/panel").c_str());
  json += ",\"uptime\":" + String(millis() / 1000);
  json += ",\"ota\":" + jsonString(otaStarted_            ? "ready"
                                   : *SECRET_OTA_PASSWORD ? "pending"
                                                          : "disabled");
  json += ",\"ntpServers\":[" + jsonString(NTP_SERVER_1) + ',' + jsonString(NTP_SERVER_2) + ']';
  tm local = {};
  char clock[12] = "";
  char date[48] = "";
  if (localTime(local)) {
    strftime(clock, sizeof(clock), "%H:%M:%S", &local);
    strftime(date, sizeof(date), "%a %d %b %Y %Z", &local);
  }
  json += ",\"time\":" + jsonString(clock) + ",\"date\":" + jsonString(date);
  json += ",\"lastSyncAge\":";
  if (lastSync_) {
    const time_t current = time(nullptr);
    json += String(static_cast<unsigned long>(current >= lastSync_ ? current - lastSync_ : 0));
  } else
    json += "null";
  json += ",\"zones\":[";
  for (size_t i = 0; i < TIME_ZONE_COUNT; ++i) {
    if (i) json += ',';
    json += jsonString(TIME_ZONES[i].id);
  }
  char hardware[520];
  snprintf(hardware, sizeof(hardware),
           "],\"device\":{\"panel\":\"64x16 HUB08\",\"board\":\"esp32dev\",\"host\":%s,"
           "\"rowPeriodUs\":%lu,\"signalDelayUs\":%lu,\"guardUs\":%lu,\"dataActiveLow\":%s,"
           "\"oeActiveLow\":%s,"
           "\"pins\":{\"LA\":%u,\"LB\":%u,\"LC\":%u,\"LD\":%u,\"EN\":%u,\"R1\":%u,\"LT\":%u,\"SK\":"
           "%u}}}",
           jsonString(hostname_).c_str(), static_cast<unsigned long>(ROW_PERIOD_US),
           static_cast<unsigned long>(SIGNAL_DELAY_US), static_cast<unsigned long>(ROW_GUARD_US),
           DATA_ACTIVE_LOW ? "true" : "false", OE_ACTIVE_LOW ? "true" : "false", PIN_A, PIN_B,
           PIN_C, PIN_D, PIN_OE, PIN_R1, PIN_LAT, PIN_CLK);
  json += hardware;
  manager_.server->sendHeader("Cache-Control", "no-store");
  manager_.server->send(200, "application/json; charset=utf-8", json);
}

void PanelNetwork::printStatus() const {
  DBG_INFO("[settings] Location=%s zone=%s rule=%s", settings_.location, settings_.zone,
           findTimeZone(settings_.zone)->rule);
  DBG_INFO("[settings] Startup brightness=%u view=%s", settings_.brightness,
           settings_.startClock ? "clock" : "playlist");
  DBG_INFO("[wifi] State=%s SSID=%s host=%s", connected() ? "connected" : "offline / connecting",
           connected() ? WiFi.SSID().c_str() : "(not connected)", hostname_);
  if (connected()) {
    DBG_INFO("[web] http://%s/panel; host=http://%s.local/panel", address().c_str(), hostname_);
  } else {
    DBG_INFO("[web] Waiting for network; setup hotspot=%s at http://192.168.4.1/", apName_);
  }
  DBG_INFO("[ota] %s", otaStarted_            ? "Ready for network uploads (port 3232)"
                       : *SECRET_OTA_PASSWORD ? "Starts when Wi-Fi connects"
                                              : "Disabled; no SECRET_OTA_PASSWORD");
  DBG_INFO("[clock] State=%s NTP=%s / %s",
           timeValid_ ? "synchronised / holding time" : "waiting for first sync", NTP_SERVER_1,
           NTP_SERVER_2);
}
