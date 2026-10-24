#include "Hub08Panel.h"

#include <soc/gpio_reg.h>
#include <soc/soc.h>
#include "debug.h"

// Direct register writes with explicit signal setup / pulse timing below.
#define PIN_HIGH(p) REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << (p))
#define PIN_LOW(p)  REG_WRITE(GPIO_OUT_W1TC_REG, 1UL << (p))
#define PIN_SET(p, v) ((v) ? PIN_HIGH(p) : PIN_LOW(p))

static_assert(PIN_A < 32 && PIN_B < 32 && PIN_C < 32 && PIN_D < 32 && PIN_OE < 32 &&
              PIN_R1 < 32 && PIN_LAT < 32 && PIN_CLK < 32,
              "HUB08 pins must be GPIO 0–31 for W1TS/W1TC register writes");
static_assert(LATCH_TEST_PULSE_US >= SIGNAL_DELAY_US, "Latch test pulse must be at least the normal pulse");
static_assert((3UL * PANEL_WIDTH + 2) * SIGNAL_DELAY_US + LATCH_TEST_PULSE_US + ROW_GUARD_US < ROW_PERIOD_US,
              "Shift/latch timing must leave time for row refresh overhead");
static_assert(ROW_ADDRESS_Y_BITS[0] < 4 && ROW_ADDRESS_Y_BITS[1] < 4 &&
              ROW_ADDRESS_Y_BITS[2] < 4 && ROW_ADDRESS_Y_BITS[3] < 4,
              "Each A-D output must select logical y bit 0-3");
static_assert(((1U << ROW_ADDRESS_Y_BITS[0]) | (1U << ROW_ADDRESS_Y_BITS[1]) |
               (1U << ROW_ADDRESS_Y_BITS[2]) | (1U << ROW_ADDRESS_Y_BITS[3])) == 0x0F,
              "Row address mapping must use each logical y bit exactly once");
static_assert(ROW_ADDRESS_XOR < 16, "Row address inversion mask must use A-D only");

Hub08Panel::Hub08Panel() : Adafruit_GFX(PANEL_WIDTH, PANEL_HEIGHT) {
  memset(drawBuf_, 0, sizeof(drawBuf_));
  memset(showBuf_, 0, sizeof(showBuf_));
}

void Hub08Panel::begin() {
  // OE first so the panel stays dark while the rest of the pins settle
  pinMode(PIN_OE, OUTPUT);
  setOE(false);

  const uint8_t pins[] = {PIN_A, PIN_B, PIN_C, PIN_D, PIN_R1, PIN_LAT, PIN_CLK};
  for (uint8_t p : pins) {
    pinMode(p, OUTPUT);
    PIN_LOW(p);
  }

  setBrightness(DEFAULT_BRIGHTNESS);

  esp_timer_create_args_t rowArgs = {};
  rowArgs.callback = &Hub08Panel::rowTimerCb;
  rowArgs.arg = this;
  rowArgs.name = "hub08_row";
  rowArgs.skip_unhandled_events = true;  // Don't burst through overdue rows
  esp_timer_create(&rowArgs, &rowTimer_);

  esp_timer_create_args_t oeArgs = {};
  oeArgs.callback = &Hub08Panel::oeOffTimerCb;
  oeArgs.arg = this;
  oeArgs.name = "hub08_oe";
  esp_timer_create(&oeArgs, &oeOffTimer_);

  esp_timer_start_periodic(rowTimer_, ROW_PERIOD_US);
  DBG_INFO("HUB08 panel %dx%d started, row period %u us (%u Hz frame)",
           PANEL_WIDTH, PANEL_HEIGHT, ROW_PERIOD_US,
           1000000UL / (ROW_PERIOD_US * PANEL_HEIGHT));
}

void Hub08Panel::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= _width || y >= _height) return;
  // Honour Adafruit_GFX setRotation()
  int16_t t;
  switch (rotation) {
    case 1: t = x; x = PANEL_WIDTH - 1 - y; y = t; break;
    case 2: x = PANEL_WIDTH - 1 - x; y = PANEL_HEIGHT - 1 - y; break;
    case 3: t = x; x = y; y = PANEL_HEIGHT - 1 - t; break;
  }
  const uint8_t mask = 0x80 >> (x & 7);
  if (color) {
    drawBuf_[y][x >> 3] |= mask;
  } else {
    drawBuf_[y][x >> 3] &= ~mask;
  }
}

void Hub08Panel::fillScreen(uint16_t color) {
  memset(drawBuf_, color ? 0xFF : 0x00, sizeof(drawBuf_));
}

void Hub08Panel::show() {
  portENTER_CRITICAL(&bufMux_);
  memcpy(showBuf_, drawBuf_, sizeof(showBuf_));
  portEXIT_CRITICAL(&bufMux_);
}

void Hub08Panel::setBrightness(uint8_t level) {
  onTimeUs_ = (uint32_t)ROW_PERIOD_US * level / 255;
  DBG_VERBOSE("Brightness %u -> on-time %u us", level, onTimeUs_);
}

void Hub08Panel::setOEActiveLow(bool activeLow) {
  // The refresh task applies this on its next OE write (within one row).
  // Keep GPIO writes in the refresh task to avoid racing its timer callbacks.
  oeActiveLow_ = activeLow;
}

uint8_t Hub08Panel::rowAddressFor(uint8_t y) {
  uint8_t address = 0;
  for (uint8_t bit = 0; bit < 4; ++bit) {
    address |= ((y >> ROW_ADDRESS_Y_BITS[bit]) & 1U) << bit;
  }
  return address ^ ROW_ADDRESS_XOR;
}

void Hub08Panel::rowTimerCb(void* arg) {
  static_cast<Hub08Panel*>(arg)->scanRow();
}

void Hub08Panel::oeOffTimerCb(void* arg) {
  static_cast<Hub08Panel*>(arg)->setOE(false);
}

void Hub08Panel::setOE(bool on) {
  PIN_SET(PIN_OE, on != oeActiveLow_);
}

void Hub08Panel::scanRow() {
  const int64_t rowStart = esp_timer_get_time();
  // Always blank before shifting, even if the previous OE callback was late.
  // Both callbacks run on the same esp_timer task, so stop any old off event
  // before enabling a new row; it must not shorten the next row's light pulse.
  setOE(false);
  esp_timer_stop(oeOffTimer_);

  uint8_t rowData[BYTES_PER_ROW];
  portENTER_CRITICAL(&bufMux_);
  memcpy(rowData, showBuf_[row_], BYTES_PER_ROW);
  portEXIT_CRITICAL(&bufMux_);

  // Shift while blanked. Reverse the entire stream (bytes AND bits) when the
  // panel's chain runs opposite to logical left-to-right coordinates.
  for (int i = 0; i < BYTES_PER_ROW; i++) {
    const uint8_t bits = rowData[REVERSE_SHIFT_COLUMNS ? BYTES_PER_ROW - 1 - i : i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const uint8_t m = REVERSE_SHIFT_COLUMNS ? (1U << bit) : (0x80U >> bit);
      PIN_SET(PIN_R1, ((bits & m) != 0) != DATA_ACTIVE_LOW);
      delayMicroseconds(SIGNAL_DELAY_US);  // Data setup before rising SK
      PIN_HIGH(PIN_CLK);
      delayMicroseconds(SIGNAL_DELAY_US);  // Minimum SK high time
      PIN_LOW(PIN_CLK);
      delayMicroseconds(SIGNAL_DELAY_US);  // SK low / cascade settling time
    }
  }

  // Blank, switch row address, latch the new columns, unblank
  const uint8_t address = rowAddressFor(row_);
  PIN_SET(PIN_A, address & 0x01);
  PIN_SET(PIN_B, address & 0x02);
  PIN_SET(PIN_C, address & 0x04);
  PIN_SET(PIN_D, address & 0x08);
  delayMicroseconds(SIGNAL_DELAY_US);  // Row address and last shift settle
  const uint32_t latchHighUs = latchPulseUs();
  PIN_HIGH(PIN_LAT);
  delayMicroseconds(latchHighUs);
  PIN_LOW(PIN_LAT);
  delayMicroseconds(SIGNAL_DELAY_US);  // Latch outputs settle before enabling

  uint32_t onTime = onTimeUs_;
  const int64_t remaining = ROW_PERIOD_US - (esp_timer_get_time() - rowStart) - ROW_GUARD_US;
  if (onTime > 0 && remaining > 0) {
    if (onTime > static_cast<uint64_t>(remaining)) onTime = remaining;
    setOE(true);
    // Even level 255 gets an off event: shifting consumed part of this row's
    // period. Leave a guard interval before the next row callback is due.
    esp_timer_start_once(oeOffTimer_, onTime);
  }

  row_ = (row_ + 1) % PANEL_HEIGHT;
}
