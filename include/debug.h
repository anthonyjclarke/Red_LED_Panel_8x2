#pragma once

#include <Arduino.h>

// Runtime levels follow the shared Aurora convention. Set the boot level in
// platformio.ini; change debugLevel (0..4) at runtime with serial command d.
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 3
#endif

#define DBG_LEVEL_OFF 0
#define DBG_LEVEL_ERROR 1
#define DBG_LEVEL_WARN 2
#define DBG_LEVEL_INFO 3
#define DBG_LEVEL_VERBOSE 4

extern uint8_t debugLevel;

// clang-format off
#define DBG_ERROR(fmt, ...)   do { if (debugLevel >= DBG_LEVEL_ERROR)   Serial.printf("[ERR ] " fmt "\n", ##__VA_ARGS__); } while (0)
#define DBG_WARN(fmt, ...)    do { if (debugLevel >= DBG_LEVEL_WARN)    Serial.printf("[WARN] " fmt "\n", ##__VA_ARGS__); } while (0)
#define DBG_INFO(fmt, ...)    do { if (debugLevel >= DBG_LEVEL_INFO)    Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__); } while (0)
#define DBG_VERBOSE(fmt, ...) do { if (debugLevel >= DBG_LEVEL_VERBOSE) Serial.printf("[VERB] " fmt "\n", ##__VA_ARGS__); } while (0)
// clang-format on
