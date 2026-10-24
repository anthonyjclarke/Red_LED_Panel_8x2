#pragma once

#include <stdint.h>

constexpr const char* FIRMWARE_VERSION = "1.1.0";

// ---------------------------------------------------------------------------
// Panel geometry — 8 x 2 modules of 8x8 LEDs = 64 x 16 pixels, 1/16 scan
// ---------------------------------------------------------------------------
constexpr int16_t PANEL_WIDTH = 64;
constexpr int16_t PANEL_HEIGHT = 16;

// ---------------------------------------------------------------------------
// ESP32 → HUB08 INPUT200 header (silkscreen names in comments)
// All pins must be < 32 — the driver writes GPIO_OUT_W1TS/W1TC directly.
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_A = 25;   // LA
constexpr uint8_t PIN_B = 26;   // LB
constexpr uint8_t PIN_C = 27;   // LC
constexpr uint8_t PIN_D = 13;   // LD
constexpr uint8_t PIN_OE = 22;  // EN
constexpr uint8_t PIN_R1 = 23;  // R1
constexpr uint8_t PIN_LAT = 4;  // LT
constexpr uint8_t PIN_CLK = 18; // SK

// ---------------------------------------------------------------------------
// Panel orientation. Draw in normal top-left coordinates; mapping is applied
// during refresh. Identity mapping is false, {0,1,2,3}, and 0x00.
// ---------------------------------------------------------------------------
constexpr bool REVERSE_SHIFT_COLUMNS = true; // Send logical x=63 first
// For address outputs A, B, C, D: which logical y bit drives each output.
// Normal address order: GPIO 26 -> LB and GPIO 27 -> LC.
constexpr uint8_t ROW_ADDRESS_Y_BITS[4] = {0, 1, 2, 3};
constexpr uint8_t ROW_ADDRESS_XOR = 0x0F; // Invert mapped address bits

// ---------------------------------------------------------------------------
// Signal polarity
// ---------------------------------------------------------------------------
// 595s sink the column cathodes, so a 0 bit lights the LED on most HUB08 panels.
// If "ALL ON" shows a blank panel (and "ALL OFF" lights everything), set false.
constexpr bool DATA_ACTIVE_LOW = true;
// EN is LOW while a row is enabled, HIGH while output is blanked.
constexpr bool OE_ACTIVE_LOW = true;

// ---------------------------------------------------------------------------
// Refresh / brightness
// ---------------------------------------------------------------------------
constexpr uint32_t ROW_PERIOD_US = 500;    // 16 rows x 500us = 125 Hz frame rate
constexpr uint8_t DEFAULT_BRIGHTNESS = 90; // 0–255, start low

// Explicit timing for the buffered 74HC595 chain.
// Each bit takes at least 3 * SIGNAL_DELAY_US; keep well inside ROW_PERIOD_US.
constexpr uint32_t SIGNAL_DELAY_US = 1;
constexpr uint32_t ROW_GUARD_US = 50; // reserve time between OE off and next row

// ---------------------------------------------------------------------------
// Showcase timing (all times in milliseconds)
// ---------------------------------------------------------------------------
constexpr uint32_t LETTER_HOLD_MS = 2500;
constexpr uint32_t CHECKER_STEP_MS = 200;
constexpr uint32_t ANIMATION_STEP_MS = 40;
constexpr uint32_t RAIN_STEP_MS = 50;
constexpr uint32_t LIFE_STEP_MS = 100;
constexpr uint32_t SCROLL_STEP_MS = 40;
constexpr uint32_t PONG_STEP_MS = 40;
constexpr uint32_t CLOCK_STEP_MS = 250;   // Read the live clock four times per second.
constexpr uint32_t CLOCK_HOLD_MS = 10000; // Clock between each pair of demos.
constexpr const char* SCROLL_TEXT = "ESP32 + HUB08 = FUN!";

// Wi-Fi credentials are entered in the setup portal, never in source code.
constexpr const char* WIFI_AP_PREFIX = "RedPanel-";
constexpr const char* WIFI_AP_PASSWORD = "PanelSetup32"; // Setup AP only; 8+ characters.
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;
constexpr uint32_t WIFI_RETRY_MS = 30000;
constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.cloudflare.com";
constexpr uint32_t NTP_RETRY_MS = 60000;
constexpr uint32_t BOOT_CARD_MS = 2000;
