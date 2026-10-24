// Exercise the actual scan driver with deterministic timer/GPIO fixtures.
// This verifies decisions and pulse sequencing, not ESP32 interrupt latency.
#include <Adafruit_GFX.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/timer.h>
#include <esp_intr_alloc.h>
#include <esp32-hal-spi.h>
#include <cassert>
#include <iostream>
#include <vector>
#define private public
#include "../src/Hub08Panel.h"
#undef private

uint32_t fakeMillis = 0;
SerialPort Serial;
unsigned notifications = 0;
unsigned activeTasks = 0;
bool failTask = false;
static uint64_t nowUs = 0;
static uint32_t pins = 0;
static unsigned latches = 0;
static bool stallShift = false;
static int failInit = -1;
static int failCallback = -1;
struct Timer {
  bool initialized = false;
  bool running = false, alarm = false, reload = false;
  int interruptFlags = 0;
  uint64_t base = 0, threshold = 0;
  bool (*callback)(void*) = nullptr;
  void* arg = nullptr;
};
static Timer timers[2];
static std::vector<uint64_t> lightStarts, lightEnds;
static bool lit() {
  return ((pins >> PIN_OE) & 1) != OE_ACTIVE_LOW;
}
void fixtureWrite(int reg, uint32_t mask) {
  const bool before = lit();
  if (reg)
    pins |= mask;
  else
    pins &= ~mask;
  if (mask == (1UL << PIN_LAT) && reg) ++latches;
  if (!before && lit()) lightStarts.push_back(nowUs);
  if (before && !lit()) lightEnds.push_back(nowUs);
}
static void advance(uint64_t amount) {
  const auto end = nowUs + amount;
  for (;;) {
    int selected = -1;
    uint64_t earliest = end + 1;
    for (int i = 0; i < 2; ++i) {
      const auto& t = timers[i];
      if (t.running && t.alarm && t.base + t.threshold < earliest) {
        selected = i;
        earliest = t.base + t.threshold;
      }
    }
    if (selected < 0) break;
    nowUs = earliest;
    auto& t = timers[selected];
    if (t.reload)
      t.base = nowUs;
    else
      t.alarm = false;
    if (t.callback) t.callback(t.arg);
  }
  nowUs = end;
}
int64_t esp_timer_get_time() {
  return nowUs;
}
void delayMicroseconds(uint32_t us) {
  if (stallShift) {
    stallShift = false;
    advance(ROW_PERIOD_US + 10); // Interrupt/task preemption during a shift.
  }
  advance(us);
}
esp_err_t timer_init(timer_group_t, timer_idx_t id, const timer_config_t* config) {
  if (failInit == id) return ESP_ERR_NO_MEM;
  timers[id] = {};
  timers[id].initialized = true;
  timers[id].reload = config->auto_reload == TIMER_AUTORELOAD_EN;
  return ESP_OK;
}
esp_err_t timer_set_counter_value(timer_group_t, timer_idx_t id, uint64_t value) {
  timers[id].base = nowUs - value;
  return ESP_OK;
}
esp_err_t timer_get_counter_value(timer_group_t, timer_idx_t id, uint64_t* value) {
  *value = nowUs - timers[id].base;
  return ESP_OK;
}
esp_err_t timer_set_alarm_value(timer_group_t, timer_idx_t id, uint64_t value) {
  timers[id].threshold = value;
  return ESP_OK;
}
esp_err_t timer_isr_callback_add(timer_group_t, timer_idx_t id, bool (*fn)(void*), void* arg,
                                 int flags) {
  assert(timers[id].initialized);
  if (failCallback == id) return ESP_ERR_NOT_FOUND;
  // Model a constrained CPU with one available interrupt vector. Two
  // exclusive requests fail; compatible shared requests fit the same slot.
  const auto& other = timers[1 - id];
  if (other.callback &&
      (!(flags & ESP_INTR_FLAG_SHARED) || !(other.interruptFlags & ESP_INTR_FLAG_SHARED) ||
       !(flags & other.interruptFlags & ESP_INTR_FLAG_LOWMED) ||
       ((flags ^ other.interruptFlags) & ESP_INTR_FLAG_IRAM)))
    return ESP_ERR_NOT_FOUND;
  timers[id].callback = fn;
  timers[id].arg = arg;
  timers[id].interruptFlags = flags;
  return ESP_OK;
}
esp_err_t timer_isr_callback_remove(timer_group_t, timer_idx_t id) {
  assert(timers[id].initialized && timers[id].callback);
  timers[id].callback = nullptr;
  timers[id].arg = nullptr;
  timers[id].interruptFlags = 0;
  return ESP_OK;
}
esp_err_t timer_deinit(timer_group_t, timer_idx_t id) {
  assert(timers[id].initialized && !timers[id].running && !timers[id].callback);
  timers[id] = {};
  return ESP_OK;
}
esp_err_t timer_set_alarm(timer_group_t, timer_idx_t id, timer_alarm_t alarm) {
  timers[id].alarm = alarm == TIMER_ALARM_EN;
  return ESP_OK;
}
esp_err_t timer_start(timer_group_t, timer_idx_t id) {
  timers[id].running = true;
  return ESP_OK;
}
esp_err_t timer_pause(timer_group_t, timer_idx_t id) {
  timers[id].running = false;
  return ESP_OK;
}
// SPI fixture: records each shifted row and models its transfer time.
static int spiBuses = 0;
static bool failSpi = false;
static unsigned shifts = 0, litShifts = 0;
static uint8_t lastShift[PANEL_WIDTH / 8];
static spi_t* const fakeBus = reinterpret_cast<spi_t*>(&spiBuses);
uint32_t spiFrequencyToClockDiv(uint32_t freq) {
  assert(freq == SHIFT_CLOCK_HZ);
  return 1;
}
spi_t* spiStartBus(uint8_t num, uint32_t, uint8_t mode, uint8_t order) {
  assert(num == VSPI && mode == SPI_MODE0 && order == SPI_MSBFIRST);
  if (failSpi) return nullptr;
  ++spiBuses;
  return fakeBus;
}
void spiStopBus(spi_t* bus) {
  assert(bus == fakeBus && spiBuses > 0);
  --spiBuses;
}
void spiAttachSCK(spi_t*, int8_t pin) {
  assert(pin == PIN_CLK);
}
void spiAttachMOSI(spi_t*, int8_t pin) {
  assert(pin == PIN_R1);
}
void spiWriteNL(spi_t* bus, const void* data, uint32_t len) {
  assert(bus == fakeBus && len == sizeof(lastShift));
  memcpy(lastShift, data, len);
  ++shifts;
  litShifts += lit();
  advance(len * 8ULL * 1000000 / SHIFT_CLOCK_HZ);
}
static bool shifted(uint8_t value) {
  for (auto byte : lastShift)
    if (byte != value) return false;
  return true;
}
static void nextRow(Hub08Panel& panel, uint32_t delayUs = 0) {
  advance(ROW_PERIOD_US - (nowUs - timers[0].base));
  advance(delayUs);
  panel.scanRow(panel.rowTick_);
}
struct TaskDone {};
uint32_t ulTaskNotifyTake(BaseType_t, uint32_t) {
  static unsigned wake = 0;
  switch (wake++) {
  case 0:
    advance(ROW_PERIOD_US);
    return 1;
  case 1:
    return 1; // Queued duplicate: generation already consumed by the task.
  case 2:
    advance(4 * ROW_PERIOD_US - (nowUs - timers[0].base));
    return 4; // Several ticks accumulated: scan once, count three skipped slots.
  default:
    throw TaskDone{};
  }
}
int main() {
  // Reproduce the previous boot failure before checking the real driver's fix.
  timer_config_t config = {};
  timer_init(TIMER_GROUP_0, TIMER_0, &config);
  timer_init(TIMER_GROUP_0, TIMER_1, &config);
  constexpr int oldFlags = ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3;
  auto noop = [](void*) { return false; };
  assert(timer_isr_callback_add(TIMER_GROUP_0, TIMER_1, noop, nullptr, oldFlags) == ESP_OK);
  assert(timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, noop, nullptr, oldFlags) ==
         ESP_ERR_NOT_FOUND);
  timer_isr_callback_remove(TIMER_GROUP_0, TIMER_1);
  timer_deinit(TIMER_GROUP_0, TIMER_1);
  timer_deinit(TIMER_GROUP_0, TIMER_0);
  for (int fail = 0; fail < 2; ++fail) {
    timers[0] = timers[1] = {};
    failInit = fail;
    Hub08Panel failed;
    failed.begin();
    assert(!lit() && !timers[0].running && !timers[1].running);
    assert(!timers[0].initialized && !timers[1].initialized && activeTasks == 0);
    assert(spiBuses == 0);
  }
  failInit = -1;
  failSpi = true;
  {
    Hub08Panel failed;
    Serial.output.clear();
    failed.begin();
    assert(!lit() && activeTasks == 0 && !timers[0].initialized && spiBuses == 0);
    assert(Serial.output.find("failed at shift SPI") != std::string::npos);
  }
  failSpi = false;
  for (int fail = 0; fail < 2; ++fail) {
    failCallback = fail;
    Hub08Panel failed;
    Serial.output.clear();
    failed.begin();
    assert(!lit() && activeTasks == 0 && !failed.refreshTask_);
    assert(!timers[0].initialized && !timers[1].initialized && spiBuses == 0);
    assert(Serial.output.find(fail == 0 ? "failed at row interrupt" : "failed at OE interrupt") !=
           std::string::npos);
  }
  failCallback = -1;
  failTask = true;
  Hub08Panel noTask;
  noTask.begin();
  assert(!lit() && activeTasks == 0 && !timers[0].initialized && !timers[1].initialized);
  assert(spiBuses == 0);
  failTask = false;
  shifts = latches = 0;
  Hub08Panel panel;
  panel.begin();
  // Random power-up 595 contents are replaced by blank row-0 data before scanning.
  assert(!lit() && timers[0].running && notifications == 0 && spiBuses == 1);
  assert(shifts == 1 && shifted(DATA_ACTIVE_LOW ? 0xFF : 0x00));
  panel.fillScreen(1);
  panel.setBrightness(90);
  panel.show();
  nextRow(panel);
  // Row 0 latches the preloaded data; row 1 shifts while row 0 is lit.
  assert(lit() && latches == 1 && notifications == 1 && shifts == 2 && litShifts == 1);
  const auto start = lightStarts.back();
  advance(180);
  assert(!lit() && lightEnds.back() - start == ROW_PERIOD_US * 90 / 255);
  // The picture is committed only when row 0 of the next scan is prepared.
  for (int row = 1; row < PANEL_HEIGHT - 1; ++row) {
    nextRow(panel);
    assert(panel.showBuf_[row][0] == 0 && panel.row_ == row + 1);
  }
  nextRow(panel); // Row 15 lit, row 0 of the new picture shifted.
  assert(panel.row_ == 0 && panel.showBuf_[0][0] == 255 && shifted(DATA_ACTIVE_LOW ? 0 : 0xFF));
  // Submitting a picture midway through a scan must not tear that scan.
  panel.fillScreen(0);
  panel.show();
  for (int row = 0; row < PANEL_HEIGHT - 1; ++row) {
    nextRow(panel);
    assert(panel.showBuf_[row + 1][0] == 255 && shifted(DATA_ACTIVE_LOW ? 0 : 0xFF));
  }
  nextRow(panel);
  for (const auto& row : panel.showBuf_)
    for (auto byte : row)
      assert(byte == 0);
  // An OE deadline runs even when no row-shifting task is scheduled.
  assert(lit());
  advance(ROW_PERIOD_US * 4);
  assert(!lit());
  // The row ISR provides a second blanking deadline if the off alarm is lost.
  nextRow(panel);
  assert(lit());
  timers[1].alarm = false;
  advance(ROW_PERIOD_US);
  assert(!lit());
  // Full brightness now uses nearly the whole row: shifting no longer eats the window.
  panel.setBrightness(255);
  panel.row_ = 0;
  nextRow(panel);
  const auto fullStart = lightStarts.back();
  advance(ROW_PERIOD_US - (nowUs - timers[0].base) - 1);
  assert(lightEnds.back() - fullStart >= ROW_PERIOD_US - ROW_GUARD_US - 5 * SIGNAL_DELAY_US);
  // Wakeup delay consumes the available light window, rather than extending it.
  nextRow(panel, 180);
  assert(lit());
  advance(ROW_PERIOD_US - (nowUs - timers[0].base) - ROW_GUARD_US);
  assert(!lit());
  const auto litCount = lightStarts.size();
  nextRow(panel, ROW_PERIOD_US - ROW_GUARD_US); // Woken after the last usable moment.
  assert(!lit() && lightStarts.size() == litCount && panel.refreshStats().lateRows > 0);
  // A tick arriving before the light is armed invalidates the old slot: remain blank.
  stallShift = true;
  nextRow(panel);
  assert(!lit() && lightStarts.size() == litCount);
  // Zero brightness and changes without show() take effect at row zero.
  panel.setBrightness(0);
  panel.row_ = 0;
  nextRow(panel);
  assert(!lit() && panel.onTimeUs_ == 0);
  panel.setBrightness(90);
  panel.row_ = 0;
  nextRow(panel);
  assert(lit() && panel.onTimeUs_ > 0);
  assert(shifts == panel.refreshStats().rows + 1 && latches == panel.refreshStats().rows);
  // Column mapping: logical x=63 is the first bit on the wire when reversed.
  memset(panel.showBuf_, 0, sizeof(panel.showBuf_));
  panel.showBuf_[1][7] = 0x01;
  panel.shiftRow(panel.showBuf_[1]);
  const uint8_t on = DATA_ACTIVE_LOW ? 0x7F : 0x80, off = DATA_ACTIVE_LOW ? 0xFF : 0x00;
  assert(lastShift[REVERSE_SHIFT_COLUMNS ? 0 : 7] == (REVERSE_SHIFT_COLUMNS ? on : (off ^ 0x01)));
  memset(panel.showBuf_, 0, sizeof(panel.showBuf_));
  for (unsigned y = 0; y < 16; ++y)
    assert(panel.rowAddressFor(y) == (y ^ 15));
  Hub08Panel scheduled;
  scheduled.begin();
  try {
    Hub08Panel::refreshTask(&scheduled);
    assert(false);
  } catch (const TaskDone&) {
  }
  assert(scheduled.refreshStats().rows == 2 && scheduled.refreshStats().missedTicks == 3);
  std::cout << "Refresh: shared interrupt allocation, setup failure cleanup, frame boundaries, "
               "OE deadlines, delayed/missed slots and brightness passed\n";
}
