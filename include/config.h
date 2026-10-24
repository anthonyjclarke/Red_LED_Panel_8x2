#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// Panel geometry — 8 x 2 modules of 8x8 LEDs = 64 x 16 pixels, 1/16 scan
// ---------------------------------------------------------------------------
constexpr int16_t PANEL_WIDTH  = 64;
constexpr int16_t PANEL_HEIGHT = 16;

// ---------------------------------------------------------------------------
// ESP32 → HUB08 INPUT200 header (silkscreen names in comments)
// All pins must be < 32 — the driver writes GPIO_OUT_W1TS/W1TC directly.
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_A   = 25;  // LA
constexpr uint8_t PIN_B   = 26;  // LB
constexpr uint8_t PIN_C   = 27;  // LC
constexpr uint8_t PIN_D   = 13;  // LD
constexpr uint8_t PIN_OE  = 22;  // EN
constexpr uint8_t PIN_R1  = 23;  // R1
constexpr uint8_t PIN_LAT = 4;   // LT
constexpr uint8_t PIN_CLK = 18;  // SK

// ---------------------------------------------------------------------------
// Output orientation inferred from the static-letter photo. Keep drawing in
// normal top-left coordinates; apply the panel mapping only during refresh.
// Set false, {0,1,2,3}, and 0x00 for the original mapping.
// ---------------------------------------------------------------------------
constexpr bool REVERSE_SHIFT_COLUMNS = true;  // Send logical x=63 first
// For address outputs A, B, C, D: which logical y bit drives each output.
// Normal address order: GPIO 26 -> LB and GPIO 27 -> LC (wiring corrected).
constexpr uint8_t ROW_ADDRESS_Y_BITS[4] = {0, 1, 2, 3};
constexpr uint8_t ROW_ADDRESS_XOR = 0x0F;      // Invert mapped address bits

// ---------------------------------------------------------------------------
// Signal polarity — flip these if the boot test looks inverted
// ---------------------------------------------------------------------------
// 595s sink the column cathodes, so a 0 bit lights the LED on most HUB08 panels.
// If "ALL ON" shows a blank panel (and "ALL OFF" lights everything), set false.
constexpr bool DATA_ACTIVE_LOW = true;
// Initial EN polarity. Serial b/e compares both idle levels at brightness 0;
// select the setting that makes EVERY module completely dark, not just dimmer.
constexpr bool OE_ACTIVE_LOW   = true;

// ---------------------------------------------------------------------------
// Refresh / brightness
// ---------------------------------------------------------------------------
constexpr uint32_t ROW_PERIOD_US      = 500;  // 16 rows x 500us = 125 Hz frame rate
constexpr uint8_t  DEFAULT_BRIGHTNESS = 90;   // 0–255, start low

// Conservative bring-up timing for the buffered 74HC595 chain. Register writes
// alone provide no explicit data setup or minimum SK/LT pulse widths.
// Each bit takes at least 3 * SIGNAL_DELAY_US; keep well inside ROW_PERIOD_US.
constexpr uint32_t SIGNAL_DELAY_US = 1;
constexpr uint32_t LATCH_TEST_PULSE_US = 20; // serial l toggles LT high time only
constexpr uint32_t ROW_GUARD_US = 50;  // reserve time between OE off and next row

// ---------------------------------------------------------------------------
// Wiring test / demo timing (all times in milliseconds)
// ---------------------------------------------------------------------------
constexpr uint32_t TEST_FILL_MS      = 2000;  // all-on / all-off hold time
constexpr uint32_t TEST_ROW_STEP_MS  = 300;   // row walk step
constexpr uint32_t TEST_COL_STEP_MS  = 60;    // column walk step
constexpr uint32_t TEST_PATTERN_MS   = 1500;  // address bits / alternating patterns
constexpr uint32_t TEST_MODULE_MS    = 500;   // one 8x8 module at a time
constexpr uint32_t TEST_CORNER_MS    = 1500;  // one corner pixel at a time
constexpr uint32_t TEST_BRIGHTNESS_MS = 1500; // OE levels, capped at DEFAULT_BRIGHTNESS
constexpr uint32_t SCROLL_STEP_MS    = 40;    // text scroll step
constexpr uint32_t SCROLL_BLANK_MS   = 3000;  // visible black frame before scrolling
constexpr const char* SCROLL_TEXT    = "Hello from ESP32!";
