#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <time.h>

#include "ClockSettings.h"

class PanelNetwork {
public:
  void begin();
  void loop();
  void requestPortal();
  void printStatus() const;
  bool localTime(tm& result) const;
  bool connected() const;
  bool portalActive();
  bool timeValid() const { return timeValid_; }
  bool takeSettingsChanged();
  String address() const;
  const char* hostname() const { return hostname_; }
  const char* apName() const { return apName_; }
  const ClockSettings& settings() const { return settings_; }

private:
  void startPortal();
  void registerRoutes();
  void sendStatus();
  void saveSettings();
  void configureTime();

  WiFiManager manager_;
  Preferences preferences_;
  ClockSettings settings_ = {};
  char hostname_[28] = {};
  char apName_[28] = {};
  char formToken_[17] = {};
  bool preferencesReady_ = false;
  bool previousConnected_ = false;
  bool timeValid_ = false;
  bool portalRequested_ = false;
  bool settingsChanged_ = false;
  bool reconnectAttempt_ = false;
  uint32_t connectStarted_ = 0;
  uint32_t lastRetry_ = 0;
  uint32_t lastNtpRequest_ = 0;
  time_t lastSync_ = 0;
};
