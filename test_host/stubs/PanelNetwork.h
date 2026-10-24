#pragma once
#include "Arduino.h"
#include "ClockSettings.h"
#include "config.h"
#include <time.h>

// Host fixture only: networking/NVS are not simulated as physical hardware.
extern time_t fixtureEpoch;
class PanelNetwork {
public:
  bool online = true;
  bool synced = true;
  bool portal = false;
  bool changed = false;
  ClockSettings values = defaultClockSettings(DEFAULT_BRIGHTNESS);
  void begin() {}
  void loop() {}
  void requestPortal() { portal = true; }
  void printStatus() const {}
  bool connected() const { return online; }
  bool timeValid() const { return synced; }
  bool portalActive() { return portal; }
  const ClockSettings& settings() const { return values; }
  bool takeSettingsChanged() {
    const bool value = changed;
    changed = false;
    return value;
  }
  bool localTime(tm& result) const {
    if (!synced) return false;
    const time_t now = fixtureEpoch + fakeMillis / 1000;
    return localtime_r(&now, &result) != nullptr;
  }
};
