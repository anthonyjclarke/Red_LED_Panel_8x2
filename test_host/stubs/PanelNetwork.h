#pragma once
#include "Arduino.h"
#include "ClockSettings.h"
#include "config.h"
#include <functional>
#include <time.h>

// Host fixture only: networking/NVS are not simulated as physical hardware.
extern time_t fixtureEpoch;
class PanelNetwork {
public:
  bool online = true;
  bool synced = true;
  bool portal = false;
  bool changed = false;
  unsigned brightnessSaves = 0;
  ClockSettings values = defaultClockSettings(DEFAULT_BRIGHTNESS, DEFAULT_CLOCK_HOLD_S);
  void begin() {}
  void loop() {}
  void requestPortal() { portal = true; }
  void printStatus() const {}
  bool connected() const { return online; }
  String address() const { return "192.168.1.80"; }
  int32_t secondsSinceSync() const { return synced ? 42 : -1; }
  bool nightActive() const {
    tm local = {};
    return values.nightEnabled && localTime(local) &&
           inNightWindow(local.tm_hour * 60 + local.tm_min, values.nightStartMinutes,
                         values.nightEndMinutes);
  }
  bool timeValid() const { return synced; }
  bool portalActive() { return portal; }
  const ClockSettings& settings() const { return values; }
  int8_t displayRequest = -1;
  bool displayOn = true, displayManual = false;
  int8_t takeDisplayRequest() {
    const int8_t value = displayRequest;
    displayRequest = -1;
    return value;
  }
  void setDisplayState(bool on, bool manual) {
    displayOn = on;
    displayManual = manual;
  }
  bool takeSettingsChanged() {
    const bool value = changed;
    changed = false;
    return value;
  }
  std::function<void(int)> otaProgress;
  void onOtaProgress(std::function<void(int)> handler) { otaProgress = handler; }
  bool saveBrightness(uint8_t level) {
    values.brightness = level;
    ++brightnessSaves;
    return true;
  }
  bool localTime(tm& result, uint16_t* milliseconds = nullptr) const {
    if (!synced) return false;
    if (milliseconds) *milliseconds = fakeMillis % 1000;
    const time_t now = fixtureEpoch + fakeMillis / 1000;
    return localtime_r(&now, &result) != nullptr;
  }
};
