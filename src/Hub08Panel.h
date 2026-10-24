#pragma once

#include <Adafruit_GFX.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"

// Single-colour HUB08 panel (74HC595 columns, 74HC138 row decode, 1/16 scan).
// Draw with any Adafruit_GFX call (colour 0 = off, non-zero = on), then call
// show() queues a complete frame for the next scan boundary. Hardware timer
// interrupts blank OE; a dedicated task shifts rows on the Arduino core.
class Hub08Panel : public Adafruit_GFX {
public:
  Hub08Panel();

  void begin();
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void fillScreen(uint16_t color) override;
  void show();
  void setBrightness(uint8_t level);
  // Raw A-D address corresponding to a logical top-to-bottom row number.
  static uint8_t rowAddressFor(uint8_t y);
  struct RefreshStats {
    uint32_t rows = 0, missedTicks = 0, lateRows = 0, maxWakeUs = 0, maxScanUs = 0;
  };
  RefreshStats refreshStats();

private:
  static constexpr int BYTES_PER_ROW = PANEL_WIDTH / 8;

  static bool IRAM_ATTR rowTimerCb(void* arg);
  static bool IRAM_ATTR oeOffTimerCb(void* arg);
  static void refreshTask(void* arg);
  void scanRow(uint32_t tick);
  void IRAM_ATTR setOE(bool on);

  uint8_t drawBuf_[PANEL_HEIGHT][BYTES_PER_ROW];
  uint8_t showBuf_[PANEL_HEIGHT][BYTES_PER_ROW];
  uint8_t pendingBuf_[PANEL_HEIGHT][BYTES_PER_ROW];
  portMUX_TYPE bufMux_ = portMUX_INITIALIZER_UNLOCKED;
  portMUX_TYPE timerMux_ = portMUX_INITIALIZER_UNLOCKED;
  bool framePending_ = false;
  uint32_t requestedOnTimeUs_ = 0, pendingOnTimeUs_ = 0, onTimeUs_ = 0;
  RefreshStats stats_;

  TaskHandle_t refreshTask_ = nullptr;
  volatile uint32_t rowTick_ = 0;
  uint8_t row_ = 0;
};
