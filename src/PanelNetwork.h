#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <functional>
#include <time.h>

#include "ClockSettings.h"

class PanelNetwork {
public:
  void begin();
  void loop();
  void requestPortal();
  void printStatus() const;
  // Optional milliseconds within the current second, for second-aligned effects.
  bool localTime(tm& result, uint16_t* milliseconds = nullptr) const;
  bool connected() const;
  bool portalActive();
  bool timeValid() const { return timeValid_; }
  bool takeSettingsChanged();
  // Seconds since the last NTP sync, or -1 before the first one.
  int32_t secondsSinceSync() const;
  // Persist a new startup brightness without signalling a settings change.
  bool saveBrightness(uint8_t level);
  // Called with 0-100 during a network firmware upload, or -1 if it fails.
  void onOtaProgress(std::function<void(int)> handler);
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
  bool authorised();
  void startOta();

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
  bool otaStarted_ = false;
  int otaPercent_ = -1;
  std::function<void(int)> otaProgress_;
  uint32_t connectStarted_ = 0;
  uint32_t lastRetry_ = 0;
  uint32_t lastNtpRequest_ = 0;
  time_t lastSync_ = 0;
  uint32_t lastSyncMs_ = 0;
};
