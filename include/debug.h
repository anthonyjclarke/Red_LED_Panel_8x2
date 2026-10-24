#pragma once

#include <Arduino.h>

// Leveled debug macros — set DEBUG_LEVEL via build_flags in platformio.ini
//   1 = ERROR · 2 = WARN · 3 = INFO · 4 = VERBOSE
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 3
#endif

#if DEBUG_LEVEL >= 1
#define DBG_ERROR(fmt, ...) Serial.printf("[E] " fmt "\n", ##__VA_ARGS__)
#else
#define DBG_ERROR(fmt, ...)
#endif

#if DEBUG_LEVEL >= 2
#define DBG_WARN(fmt, ...) Serial.printf("[W] " fmt "\n", ##__VA_ARGS__)
#else
#define DBG_WARN(fmt, ...)
#endif

#if DEBUG_LEVEL >= 3
#define DBG_INFO(fmt, ...) Serial.printf("[I] " fmt "\n", ##__VA_ARGS__)
#else
#define DBG_INFO(fmt, ...)
#endif

#if DEBUG_LEVEL >= 4
#define DBG_VERBOSE(fmt, ...) Serial.printf("[V] " fmt "\n", ##__VA_ARGS__)
#else
#define DBG_VERBOSE(fmt, ...)
#endif
