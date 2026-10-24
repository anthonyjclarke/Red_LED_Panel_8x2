#include "Hub08Panel.h"

#include <soc/gpio_reg.h>
#include <soc/soc.h>
#include <driver/timer.h>
#include <esp_intr_alloc.h>
#include "debug.h"

// Direct register writes with explicit signal setup / pulse timing below.
#define PIN_HIGH(p) REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << (p))
#define PIN_LOW(p) REG_WRITE(GPIO_OUT_W1TC_REG, 1UL << (p))
#define PIN_SET(p, v) ((v) ? PIN_HIGH(p) : PIN_LOW(p))

static_assert(PIN_A < 32 && PIN_B < 32 && PIN_C < 32 && PIN_D < 32 && PIN_OE < 32 && PIN_R1 < 32 &&
                  PIN_LAT < 32 && PIN_CLK < 32,
              "HUB08 pins must be GPIO 0–31 for W1TS/W1TC register writes");
static_assert((3UL * PANEL_WIDTH + 3) * SIGNAL_DELAY_US + ROW_GUARD_US < ROW_PERIOD_US,
              "Shift/latch timing must leave time for row refresh overhead");
static_assert(ROW_ADDRESS_Y_BITS[0] < 4 && ROW_ADDRESS_Y_BITS[1] < 4 && ROW_ADDRESS_Y_BITS[2] < 4 &&
                  ROW_ADDRESS_Y_BITS[3] < 4,
              "Each A-D output must select logical y bit 0-3");
static_assert(((1U << ROW_ADDRESS_Y_BITS[0]) | (1U << ROW_ADDRESS_Y_BITS[1]) |
               (1U << ROW_ADDRESS_Y_BITS[2]) | (1U << ROW_ADDRESS_Y_BITS[3])) == 0x0F,
              "Row address mapping must use each logical y bit exactly once");
static_assert(ROW_ADDRESS_XOR < 16, "Row address inversion mask must use A-D only");

Hub08Panel::Hub08Panel() : Adafruit_GFX(PANEL_WIDTH, PANEL_HEIGHT) {
  memset(drawBuf_, 0, sizeof(drawBuf_));
  memset(showBuf_, 0, sizeof(showBuf_));
  memset(pendingBuf_, 0, sizeof(pendingBuf_));
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

  // Reserve timer group 0's two timers. Allocate interrupts on the same core
  // as the shifting task (setup runs on core 1 on this ESP32 DevKit target).
  // No shifting, logging or allocation occurs in an interrupt handler.
  timer_config_t timer = {};
  timer.divider = 80; // 80 MHz APB / 80 = 1 MHz, microsecond counter units.
  timer.counter_dir = TIMER_COUNT_UP;
  timer.counter_en = TIMER_PAUSE;
  timer.alarm_en = TIMER_ALARM_DIS;
  timer.auto_reload = TIMER_AUTORELOAD_EN;
  timer.intr_type = TIMER_INTR_LEVEL;
  const char* stage = "row timer";
  esp_err_t error = timer_init(TIMER_GROUP_0, TIMER_0, &timer);
  const bool rowReady = error == ESP_OK;
  bool offReady = false, rowInterrupt = false, offInterrupt = false;
  if (error == ESP_OK) {
    stage = "OE timer";
    timer.auto_reload = TIMER_AUTORELOAD_DIS;
    error = timer_init(TIMER_GROUP_0, TIMER_1, &timer);
    offReady = error == ESP_OK;
  }
  if (error == ESP_OK) stage = "row counter/alarm";
  if (error == ESP_OK) error = timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
  if (error == ESP_OK) error = timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, ROW_PERIOD_US);
  if (error == ESP_OK) {
    stage = "scan task";
    if (xTaskCreatePinnedToCore(refreshTask, "hub08_scan", 4096, this, configMAX_PRIORITIES - 2,
                                &refreshTask_, xPortGetCoreID()) != pdPASS)
      error = ESP_ERR_NO_MEM;
  }
  // Both timer sources can share a CPU interrupt. Requiring two exclusive
  // level-3 vectors can exhaust the ESP32's available vectors at boot.
  // Keep IRAM safety, but let the allocator choose an available C-callable
  // level (1-3). The IDF timer dispatcher checks each source's status bit.
  constexpr int flags = ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_LOWMED;
  if (error == ESP_OK) {
    stage = "OE interrupt";
    error = timer_isr_callback_add(TIMER_GROUP_0, TIMER_1, oeOffTimerCb, this, flags);
    offInterrupt = error == ESP_OK;
  }
  if (error == ESP_OK) {
    stage = "row interrupt";
    error = timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, rowTimerCb, this, flags);
    rowInterrupt = error == ESP_OK;
  }
  if (error == ESP_OK) stage = "row timer start";
  if (error == ESP_OK) error = timer_set_alarm(TIMER_GROUP_0, TIMER_0, TIMER_ALARM_EN);
  if (error == ESP_OK) error = timer_start(TIMER_GROUP_0, TIMER_0);
  if (error != ESP_OK) {
    // Unwind only acquired resources, on the same core as allocation. Leave
    // OE blanked and no dormant task or half-registered timer interrupt.
    if (rowReady) {
      timer_pause(TIMER_GROUP_0, TIMER_0);
      timer_set_alarm(TIMER_GROUP_0, TIMER_0, TIMER_ALARM_DIS);
    }
    if (offReady) {
      timer_pause(TIMER_GROUP_0, TIMER_1);
      timer_set_alarm(TIMER_GROUP_0, TIMER_1, TIMER_ALARM_DIS);
    }
    if (rowInterrupt) timer_isr_callback_remove(TIMER_GROUP_0, TIMER_0);
    if (offInterrupt) timer_isr_callback_remove(TIMER_GROUP_0, TIMER_1);
    if (refreshTask_) {
      vTaskDelete(refreshTask_);
      refreshTask_ = nullptr;
    }
    if (offReady) timer_deinit(TIMER_GROUP_0, TIMER_1);
    if (rowReady) timer_deinit(TIMER_GROUP_0, TIMER_0);
    setOE(false);
    DBG_ERROR("HUB08 refresh setup failed at %s: %s (output blanked)", stage,
              esp_err_to_name(error));
    return;
  }
  DBG_INFO("HUB08 panel %dx%d started, row period %u us (%u Hz frame)", PANEL_WIDTH, PANEL_HEIGHT,
           ROW_PERIOD_US, 1000000UL / (ROW_PERIOD_US * PANEL_HEIGHT));
  DBG_INFO("Refresh: hardware timers TG0/T0+T1, shared IRAM interrupts, scan task on core %d",
           xPortGetCoreID());
}

void Hub08Panel::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= _width || y >= _height) return;
  // Honour Adafruit_GFX setRotation()
  int16_t t;
  switch (rotation) {
  case 1:
    t = x;
    x = PANEL_WIDTH - 1 - y;
    y = t;
    break;
  case 2:
    x = PANEL_WIDTH - 1 - x;
    y = PANEL_HEIGHT - 1 - y;
    break;
  case 3:
    t = x;
    x = y;
    y = PANEL_HEIGHT - 1 - t;
    break;
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
  memcpy(pendingBuf_, drawBuf_, sizeof(pendingBuf_));
  pendingOnTimeUs_ = requestedOnTimeUs_;
  framePending_ = true;
  portEXIT_CRITICAL(&bufMux_);
}

void Hub08Panel::setBrightness(uint8_t level) {
  const uint32_t onTime = (uint32_t)ROW_PERIOD_US * level / 255;
  portENTER_CRITICAL(&bufMux_);
  const bool changed = requestedOnTimeUs_ != onTime;
  requestedOnTimeUs_ = onTime;
  // Also support brightness changes without a following show().
  pendingOnTimeUs_ = onTime;
  portEXIT_CRITICAL(&bufMux_);
  if (changed) DBG_VERBOSE("Brightness %u -> on-time %u us", level, onTime);
}

Hub08Panel::RefreshStats Hub08Panel::refreshStats() {
  portENTER_CRITICAL(&bufMux_);
  const auto result = stats_;
  portEXIT_CRITICAL(&bufMux_);
  return result;
}

uint8_t Hub08Panel::rowAddressFor(uint8_t y) {
  uint8_t address = 0;
  for (uint8_t bit = 0; bit < 4; ++bit) {
    address |= ((y >> ROW_ADDRESS_Y_BITS[bit]) & 1U) << bit;
  }
  return address ^ ROW_ADDRESS_XOR;
}

bool IRAM_ATTR Hub08Panel::rowTimerCb(void* arg) {
  auto* self = static_cast<Hub08Panel*>(arg);
  self->setOE(false); // Never depend on a scheduled task to end a light pulse.
  ++self->rowTick_;
  BaseType_t awakened = pdFALSE;
  vTaskNotifyGiveFromISR(self->refreshTask_, &awakened);
  return awakened == pdTRUE;
}

bool IRAM_ATTR Hub08Panel::oeOffTimerCb(void* arg) {
  static_cast<Hub08Panel*>(arg)->setOE(false);
  return false;
}

void IRAM_ATTR Hub08Panel::setOE(bool on) {
  PIN_SET(PIN_OE, on != OE_ACTIVE_LOW);
}

void Hub08Panel::refreshTask(void* arg) {
  auto* self = static_cast<Hub08Panel*>(arg);
  uint32_t lastTick = 0;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    const uint32_t tick = self->rowTick_;
    // An interrupt can arrive between taking a notification and reading its
    // generation. Its queued notification must not scan this same slot twice.
    if (tick == lastTick) continue;
    portENTER_CRITICAL(&self->bufMux_);
    self->stats_.missedTicks += tick - lastTick - 1;
    portEXIT_CRITICAL(&self->bufMux_);
    lastTick = tick;
    self->scanRow(tick); // Skip a backlog instead of replaying old rows.
  }
}

void Hub08Panel::scanRow(uint32_t tick) {
  const int64_t rowStart = esp_timer_get_time();
  setOE(false);
  timer_pause(TIMER_GROUP_0, TIMER_1);
  timer_set_alarm(TIMER_GROUP_0, TIMER_1, TIMER_ALARM_DIS);
  uint64_t wakeUs = 0;
  timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &wakeUs);

  uint8_t rowData[BYTES_PER_ROW];
  portENTER_CRITICAL(&bufMux_);
  if (row_ == 0) {
    if (framePending_) {
      memcpy(showBuf_, pendingBuf_, sizeof(showBuf_));
      framePending_ = false;
    }
    onTimeUs_ = pendingOnTimeUs_;
  }
  memcpy(rowData, showBuf_[row_], BYTES_PER_ROW);
  portEXIT_CRITICAL(&bufMux_);

  // Shift while blanked. Reverse the entire stream (bytes AND bits) when the
  // panel's chain runs opposite to logical left-to-right coordinates.
  for (int i = 0; i < BYTES_PER_ROW; i++) {
    const uint8_t bits = rowData[REVERSE_SHIFT_COLUMNS ? BYTES_PER_ROW - 1 - i : i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const uint8_t m = REVERSE_SHIFT_COLUMNS ? (1U << bit) : (0x80U >> bit);
      PIN_SET(PIN_R1, ((bits & m) != 0) != DATA_ACTIVE_LOW);
      delayMicroseconds(SIGNAL_DELAY_US); // Data setup before rising SK
      PIN_HIGH(PIN_CLK);
      delayMicroseconds(SIGNAL_DELAY_US); // Minimum SK high time
      PIN_LOW(PIN_CLK);
      delayMicroseconds(SIGNAL_DELAY_US); // SK low / cascade settling time
    }
  }

  // Blank, switch row address, latch the new columns, unblank
  const uint8_t address = rowAddressFor(row_);
  PIN_SET(PIN_A, address & 0x01);
  PIN_SET(PIN_B, address & 0x02);
  PIN_SET(PIN_C, address & 0x04);
  PIN_SET(PIN_D, address & 0x08);
  delayMicroseconds(SIGNAL_DELAY_US); // Row address and last shift settle
  PIN_HIGH(PIN_LAT);
  delayMicroseconds(SIGNAL_DELAY_US);
  PIN_LOW(PIN_LAT);
  delayMicroseconds(SIGNAL_DELAY_US); // Latch outputs settle before enabling

  // Prepare the off timer while dark. Protect only the short arm sequence,
  // never the 64-bit shift. A missed deadline stays dark; it cannot spill an
  // overlong pulse into the following row. Use the hardware counter, not task
  // start time, because a late wake has already consumed part of this slot.
  timer_set_counter_value(TIMER_GROUP_0, TIMER_1, 0);
  portENTER_CRITICAL(&timerMux_);
  uint64_t elapsed = 0;
  timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &elapsed);
  const bool late = tick != rowTick_ || elapsed >= ROW_PERIOD_US - ROW_GUARD_US;
  if (!late && onTimeUs_ > 0) {
    const uint32_t remaining = ROW_PERIOD_US - elapsed - ROW_GUARD_US;
    const uint32_t onTime = onTimeUs_ < remaining ? onTimeUs_ : remaining;
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_1, onTime);
    timer_set_alarm(TIMER_GROUP_0, TIMER_1, TIMER_ALARM_EN);
    setOE(true);
    timer_start(TIMER_GROUP_0, TIMER_1);
  }
  portEXIT_CRITICAL(&timerMux_);

  const uint32_t scanUs = esp_timer_get_time() - rowStart;
  portENTER_CRITICAL(&bufMux_);
  ++stats_.rows;
  stats_.lateRows += late;
  if (wakeUs > stats_.maxWakeUs) stats_.maxWakeUs = wakeUs;
  if (scanUs > stats_.maxScanUs) stats_.maxScanUs = scanUs;
  portEXIT_CRITICAL(&bufMux_);

  row_ = (row_ + 1) % PANEL_HEIGHT;
}
