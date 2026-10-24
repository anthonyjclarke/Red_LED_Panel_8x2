#pragma once

#include <Adafruit_GFX.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include "config.h"

// Single-colour HUB08 panel (74HC595 columns, 74HC138 row decode, 1/16 scan).
// Draw with any Adafruit_GFX call (colour 0 = off, non-zero = on), then call
// show() to push the frame to the panel. Refresh runs from an esp_timer
// callback (esp_timer task on core 0), one row per ROW_PERIOD_US.
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

 private:
  static constexpr int BYTES_PER_ROW = PANEL_WIDTH / 8;

  static void rowTimerCb(void* arg);
  static void oeOffTimerCb(void* arg);
  void scanRow();
  void setOE(bool on);

  uint8_t drawBuf_[PANEL_HEIGHT][BYTES_PER_ROW];
  uint8_t showBuf_[PANEL_HEIGHT][BYTES_PER_ROW];
  portMUX_TYPE bufMux_ = portMUX_INITIALIZER_UNLOCKED;

  esp_timer_handle_t rowTimer_   = nullptr;
  esp_timer_handle_t oeOffTimer_ = nullptr;
  volatile uint32_t onTimeUs_    = 0;
  uint8_t row_                   = 0;
};
