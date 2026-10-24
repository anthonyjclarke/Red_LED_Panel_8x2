#pragma once

#include <stdint.h>

constexpr const char* FIRMWARE_VERSION = "1.2.0";

// ---------------------------------------------------------------------------
// Panel geometry — 8 x 2 modules of 8x8 LEDs = 64 x 16 pixels, 1/16 scan
// ---------------------------------------------------------------------------
constexpr int16_t PANEL_WIDTH = 64;
constexpr int16_t PANEL_HEIGHT = 16;

// ---------------------------------------------------------------------------
// ESP32 → HUB08 INPUT200 header (silkscreen names in comments)
// All pins must be < 32 — the driver writes GPIO_OUT_W1TS/W1TC directly.
// R1 and SK are driven by the VSPI peripheral (GPIO matrix), not bit-banged.
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

// Column data is shifted by hardware SPI while the previous row is lit, so the
// light window is no longer limited by shifting. 500 kHz keeps the former
// bit-banged 1 us SK high time and 1 us data setup, which suits pull-up level
// shifters such as the LS1; 64 bits take 128 us. Raise only after a hardware check.
constexpr uint32_t SHIFT_CLOCK_HZ = 500000;
// Address and latch setup/pulse time around the row switch (output blanked).
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
// The setup-hotspot and optional settings-page passwords live in
// include/secrets.h (gitignored); see include/secrets.example.h.
constexpr const char* WIFI_AP_PREFIX = "RedPanel-";
constexpr const char* PANEL_ADMIN_USER = "admin"; // Login name when a page password is set.
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;
constexpr uint32_t WIFI_SAVE_CONNECT_TIMEOUT_MS = 8000; // After new credentials are saved.
constexpr uint32_t WIFI_RETRY_MS = 30000;
constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.cloudflare.com";
constexpr uint32_t NTP_RETRY_MS = 60000;
// Without a fresh NTP sync for this long, the clock stops claiming valid time
// (ESP32 RTC drift can reach seconds per hour).
constexpr uint32_t CLOCK_HOLDOVER_MAX_MS = 24UL * 60 * 60 * 1000;
// Serial +/- brightness is saved as the startup level after this quiet period.
constexpr uint32_t BRIGHTNESS_SAVE_DELAY_MS = 5000;
constexpr uint32_t BOOT_CARD_MS = 2000;
