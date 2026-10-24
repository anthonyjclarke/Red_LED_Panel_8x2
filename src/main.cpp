// RGB_Panel_8x2 — serial-guided wiring tests, then a text scroller
#include <Arduino.h>
#include "config.h"
#include "debug.h"
#include "Hub08Panel.h"

Hub08Panel panel;

enum class Stage : uint8_t {
  AllOn, AllOff, RowWalk, ColWalk, AddressBits, RowStripes, ColStripes,
  Checkerboard, Corners, Modules, Brightness, Scroll, StaticLetters
};

struct Test {
  const char* name;
  uint16_t frames;
  uint32_t intervalMs;
};

static const Test tests[] = {
  {"ALL ON - every LED should light", 1, TEST_FILL_MS},
  {"ALL OFF - every LED should be dark", 1, TEST_FILL_MS},
  {"ROW WALK - one row, top to bottom; check A/B/C/D", PANEL_HEIGHT, TEST_ROW_STEP_MS},
  {"COLUMN WALK - one column, left to right; check R1/SK/LT and bit order", PANEL_WIDTH, TEST_COL_STEP_MS},
  {"ROW ADDRESS BITS - alternating row groups, then their inverse", 8, TEST_PATTERN_MS},
  {"ROW STRIPES - alternating single rows, then inverse", 2, TEST_PATTERN_MS},
  {"COLUMN STRIPES - alternating single columns, then inverse", 2, TEST_PATTERN_MS},
  {"CHECKERBOARD - alternating pixels, then inverse; look for ghosting", 2, TEST_PATTERN_MS},
  {"CORNERS - one pixel at each corner; check orientation", 4, TEST_CORNER_MS},
  {"MODULE WALK - one 8x8 block, left to right, top then bottom", (PANEL_WIDTH / 8) * (PANEL_HEIGHT / 8), TEST_MODULE_MS},
  {"BRIGHTNESS - full pattern at increasing OE levels", 4, TEST_BRIGHTNESS_MS},
};
static constexpr uint8_t TEST_COUNT = sizeof(tests) / sizeof(tests[0]);
static_assert(TEST_COUNT == static_cast<uint8_t>(Stage::Scroll), "One description per wiring test");

static Stage stage = Stage::AllOn;
static uint32_t lastStep = 0;
static uint16_t step = 0;
static bool paused = false;
static uint32_t pausedAt = 0;
static int16_t textWidth = 0;
static int16_t scrollX = PANEL_WIDTH;
static bool scrollWaiting = false;

static void printHelp() {
  Serial.println("Commands: r = restart tests, p = pause/resume, n = next test (resumes), ?/h = help");
  Serial.println("Held diagnostics: f = ALL OFF data, o = ALL ON data, b = OE blank; r/n exits hold");
  Serial.println("s = hold the exact pre-scroll black state; compare with f (same frame and brightness)");
  Serial.println("t = hold static letters: ABCDEFGH on top, IJKLMNOP below (one letter per 8x8 module)");
  Serial.println("l = hold current pattern and toggle LT high time (normal/long); try f, then l");
  Serial.println("e = flip EN/OE polarity and hold brightness 0; compare b then e for complete darkness");
  Serial.println("Compare each visible pattern with its serial description; these tests cannot automatically detect wiring faults.");
}

static void printBlackState() {
  Serial.printf("BLACK CHECK: frame=%d zero bytes, brightness=%u/255, OE_ACTIVE_LOW=%s, DATA_ACTIVE_LOW=%s\n",
                (PANEL_WIDTH / 8) * PANEL_HEIGHT, DEFAULT_BRIGHTNESS, panel.oeActiveLow() ? "true" : "false",
                DATA_ACTIVE_LOW ? "true" : "false");
  Serial.printf("  R1 GPIO %u should stay %s; each row still gets %d SK clocks and one LT pulse.\n",
                PIN_R1, DATA_ACTIVE_LOW ? "HIGH" : "LOW", PANEL_WIDTH);
  Serial.printf("  LT high time: %lu us\n", static_cast<unsigned long>(panel.latchPulseUs()));
}

static void renderTestFrame() {
  panel.fillScreen(0);
  switch (stage) {
    case Stage::AllOn:
      panel.fillScreen(1);
      break;
    case Stage::AllOff:
      break;
    case Stage::RowWalk: {
      panel.drawFastHLine(0, step, PANEL_WIDTH, 1);
      const uint8_t address = Hub08Panel::rowAddressFor(step);
      Serial.printf("  Row y=%u/%d; raw D C B A = %u %u %u %u\n", step, PANEL_HEIGHT - 1,
                    (address >> 3) & 1, (address >> 2) & 1, (address >> 1) & 1, address & 1);
      break;
    }
    case Stage::ColWalk:
      panel.drawFastVLine(step, 0, PANEL_HEIGHT, 1);
      Serial.printf("  Column x=%u/%d (8-column block %u, offset %u)\n",
                    step, PANEL_WIDTH - 1, step / 8, step % 8);
      break;
    case Stage::AddressBits: {
      const uint8_t bit = step / 2;
      const uint8_t value = step % 2;
      const uint8_t pins[] = {PIN_A, PIN_B, PIN_C, PIN_D};
      uint8_t outputBit = 0;
      while (ROW_ADDRESS_Y_BITS[outputBit] != bit) ++outputBit;
      for (int16_t y = 0; y < PANEL_HEIGHT; ++y) {
        if (((y >> bit) & 1) == value) panel.drawFastHLine(0, y, PANEL_WIDTH, 1);
      }
      const bool rawHigh = value ^ ((ROW_ADDRESS_XOR >> outputBit) & 1U);
      Serial.printf("  Logical y bit %c=%u lit, groups of %u rows; address %c GPIO %u is %s for lit rows\n",
                    'A' + bit, value, 1U << bit, 'A' + outputBit, pins[outputBit], rawHigh ? "HIGH" : "LOW");
      break;
    }
    case Stage::RowStripes:
    case Stage::ColStripes:
    case Stage::Checkerboard:
      for (int16_t y = 0; y < PANEL_HEIGHT; ++y) {
        for (int16_t x = 0; x < PANEL_WIDTH; ++x) {
          const int16_t index = stage == Stage::RowStripes ? y :
                                stage == Stage::ColStripes ? x : x + y;
          if ((index & 1) == step) panel.drawPixel(x, y, 1);
        }
      }
      Serial.printf("  Phase %u/2: %s parity lit\n", step + 1, step ? "odd" : "even");
      break;
    case Stage::Corners: {
      const char* names[] = {"top left", "top right", "bottom right", "bottom left"};
      const int16_t x = (step == 1 || step == 2) ? PANEL_WIDTH - 1 : 0;
      const int16_t y = step >= 2 ? PANEL_HEIGHT - 1 : 0;
      panel.drawPixel(x, y, 1);
      Serial.printf("  %s only: x=%d, y=%d\n", names[step], x, y);
      break;
    }
    case Stage::Modules: {
      const int16_t x = (step % (PANEL_WIDTH / 8)) * 8;
      const int16_t y = (step / (PANEL_WIDTH / 8)) * 8;
      panel.fillRect(x, y, 8, 8, 1);
      Serial.printf("  Module %u/%u: x=%d..%d, y=%d..%d\n",
                    step + 1, tests[static_cast<uint8_t>(stage)].frames, x, x + 7, y, y + 7);
      break;
    }
    case Stage::Brightness: {
      const uint8_t level = static_cast<uint16_t>(DEFAULT_BRIGHTNESS) * step / 3;
      panel.fillScreen(1);
      panel.setBrightness(level);
      Serial.printf("  EN/OE GPIO %u: brightness %u/255 (%s)\n",
                    PIN_OE, level, step == 0 ? "dark" : "increasing light");
      break;
    }
    case Stage::StaticLetters:
      for (int16_t row = 0; row < PANEL_HEIGHT / 8; ++row) {
        for (int16_t col = 0; col < PANEL_WIDTH / 8; ++col) {
          const unsigned char letter = 'A' + row * (PANEL_WIDTH / 8) + col;
          panel.drawChar(col * 8 + 1, row * 8, letter, 1, 0, 1);
        }
      }
      break;
    case Stage::Scroll:
      return;
  }
  panel.show();
}

static void enterStage(Stage next) {
  stage = next;
  step = 0;
  paused = false;
  scrollWaiting = false;
  panel.setBrightness(DEFAULT_BRIGHTNESS);
  if (stage == Stage::StaticLetters) {
    renderTestFrame();
    paused = true;
    pausedAt = millis();
    Serial.println("\nSTATIC LETTERS - held indefinitely; no scrolling, panel refresh continues");
    Serial.println("  Top:    A B C D E F G H");
    Serial.println("  Bottom: I J K L M N O P");
    Serial.println("One small letter per 8x8 module. Rightmost three columns: F/G/H above N/O/P.");
    Serial.println("Press r or n to return to the wiring tests.");
  } else if (stage == Stage::Scroll) {
    panel.fillScreen(0);
    panel.show();
    scrollX = PANEL_WIDTH;
    scrollWaiting = true;
    Serial.println("\nWiring sequence complete. Visual inspection is required to confirm wiring.");
    Serial.printf("PRE-SCROLL BLACK: %lu ms, brightness %u/255. Same black data as ALL OFF; f holds this pattern.\n",
                  static_cast<unsigned long>(SCROLL_BLANK_MS), DEFAULT_BRIGHTNESS);
    printBlackState();
  } else {
    const uint8_t index = static_cast<uint8_t>(stage);
    Serial.printf("\nTEST %u/%u: %s\n", index + 1, TEST_COUNT, tests[index].name);
    renderTestFrame();
    if (stage == Stage::AllOff) printBlackState();
  }
  lastStep = millis();
}

static void nextStage() {
  if (stage == Stage::Scroll || stage == Stage::StaticLetters) enterStage(Stage::AllOn);
  else enterStage(static_cast<Stage>(static_cast<uint8_t>(stage) + 1));
}

static void holdDiagnostic(Stage pattern, bool blankOutput) {
  enterStage(pattern);
  if (blankOutput) panel.setBrightness(0);
  paused = true;
  pausedAt = millis();
  if (blankOutput) {
    Serial.printf("BLANK TEST: GPIO %u held %s; OE_ACTIVE_LOW=%s. Check for COMPLETE darkness, not just fading.\n",
                  PIN_OE, panel.oeActiveLow() ? "HIGH" : "LOW",
                  panel.oeActiveLow() ? "true" : "false");
    Serial.println("Send e to test the opposite level. If neither level blanks all modules, check EN wiring/buffering.");
  } else {
    Serial.println("HELD: pattern continuously shifted and latched at normal brightness");
  }
}

static void holdPreScrollBlack() {
  enterStage(Stage::Scroll);
  paused = true;
  pausedAt = millis();
  Serial.println("HELD PRE-SCROLL BLACK: exact state after test 11; p starts the black-frame wait, r restarts tests");
}

static void handleSerial() {
  // Bound work per loop so a stream of serial input cannot starve the demo.
  for (uint8_t count = 0; count < 16 && Serial.available(); ++count) {
    switch (Serial.read()) {
      case 'r': case 'R': enterStage(Stage::AllOn); break;
      case 'n': case 'N': nextStage(); break;
      case 'f': case 'F': holdDiagnostic(Stage::AllOff, false); break;
      case 'o': case 'O': holdDiagnostic(Stage::AllOn, false); break;
      case 'b': case 'B': holdDiagnostic(Stage::AllOn, true); break;
      case 's': case 'S': holdPreScrollBlack(); break;
      case 't': case 'T': enterStage(Stage::StaticLetters); break;
      case 'l': case 'L':
        if (!paused) {
          paused = true;
          pausedAt = millis();
        }
        panel.useLongLatchPulse(!panel.longLatchPulse());
        Serial.printf("LATCH TIMING: LT GPIO %u high for %lu us (%s). Current pattern held; send l to toggle back.\n",
                      PIN_LAT, static_cast<unsigned long>(panel.latchPulseUs()),
                      panel.longLatchPulse() ? "long test" : "normal");
        break;
      case 'e': case 'E':
        panel.setOEActiveLow(!panel.oeActiveLow());
        holdDiagnostic(Stage::AllOn, true);
        break;
      case 'p': case 'P':
        if (paused) {
          lastStep += millis() - pausedAt;
          paused = false;
          Serial.println("RESUMED");
        } else {
          pausedAt = millis();
          paused = true;
          Serial.println("PAUSED - current pattern held; panel refresh continues");
        }
        break;
      case '?': case 'h': case 'H': printHelp(); break;
      default: break;  // Ignore terminal line endings.
    }
  }
}

static void updateDemo() {
  if (paused || stage == Stage::StaticLetters) return;
  const uint32_t now = millis();
  if (stage == Stage::Scroll) {
    if (scrollWaiting) {
      if (now - lastStep < SCROLL_BLANK_MS) return;
      scrollWaiting = false;
      Serial.printf("SCROLL: \"%s\". Press r to repeat tests.\n", SCROLL_TEXT);
    }
    if (now - lastStep < SCROLL_STEP_MS) return;
    panel.fillScreen(0);
    panel.setCursor(scrollX, 1);
    panel.print(SCROLL_TEXT);
    panel.show();
    if (--scrollX < -textWidth) scrollX = PANEL_WIDTH;
  } else {
    const Test& test = tests[static_cast<uint8_t>(stage)];
    if (now - lastStep < test.intervalMs) return;
    if (++step >= test.frames) {
      nextStage();
      return;
    }
    renderTestFrame();
  }
  lastStep = millis();
}

static void initDisplay() {
  panel.begin();
  panel.setTextWrap(false);
  panel.setTextSize(2);  // built-in 5x7 font doubled = 14 px tall
  panel.setTextColor(1);

  int16_t x1, y1;
  uint16_t w, h;
  panel.getTextBounds(SCROLL_TEXT, 0, 0, &x1, &y1, &w, &h);
  textWidth = w;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nRGB_Panel_8x2 - HUB08 wiring diagnostics (115200 baud)");
  Serial.println("Diagnostics revision: latch-compare-1 (normal B/C wiring; l = compare LT pulse width)");
  Serial.println("Refresh: blank before shift, bounded OE pulse, skip overdue row callbacks");
  Serial.printf("Panel: %dx%d, 1/%d scan; connect INPUT200 and common GND.\n",
                PANEL_WIDTH, PANEL_HEIGHT, PANEL_HEIGHT);
  Serial.printf("GPIO: LA/A=%u LB/B=%u LC/C=%u LD/D=%u EN/OE=%u R1=%u LT/LAT=%u SK/CLK=%u\n",
                PIN_A, PIN_B, PIN_C, PIN_D, PIN_OE, PIN_R1, PIN_LAT, PIN_CLK);
  Serial.printf("Polarity: DATA_ACTIVE_LOW=%s, OE_ACTIVE_LOW=%s; brightness=%u/255\n",
                DATA_ACTIVE_LOW ? "true" : "false", OE_ACTIVE_LOW ? "true" : "false", DEFAULT_BRIGHTNESS);
  Serial.printf("Shift timing: %lu us per setup/high/low phase; %d bits per row\n",
                static_cast<unsigned long>(SIGNAL_DELAY_US), PANEL_WIDTH);
  Serial.printf("Mapping: reverse columns=%s; A/B/C/D from y bits %u/%u/%u/%u; address XOR=0x%02X\n",
                REVERSE_SHIFT_COLUMNS ? "true" : "false", ROW_ADDRESS_Y_BITS[0], ROW_ADDRESS_Y_BITS[1],
                ROW_ADDRESS_Y_BITS[2], ROW_ADDRESS_Y_BITS[3], ROW_ADDRESS_XOR);
  printHelp();

  initDisplay();
  enterStage(Stage::AllOn);

#if DEBUG_LEVEL >= 3
  DBG_INFO("Free heap: %u", ESP.getFreeHeap());
#endif
}

void loop() {
  handleSerial();
  updateDemo();
}
