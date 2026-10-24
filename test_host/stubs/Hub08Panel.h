#pragma once
#include "Adafruit_GFX.h"
#include "config.h"
class Hub08Panel : public GFXcanvas1 {
public:
  uint8_t level = 0;
  unsigned shows = 0;
  Hub08Panel() : GFXcanvas1(PANEL_WIDTH, PANEL_HEIGHT) {}
  void begin() {}
  void setBrightness(uint8_t value) { level = value; }
  void show() { ++shows; }
  struct RefreshStats {
    uint32_t rows = 0, missedTicks = 0, lateRows = 0, maxWakeUs = 0, maxScanUs = 0;
  };
  RefreshStats refreshStats() { return {}; }
};
