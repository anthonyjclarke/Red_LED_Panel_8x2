#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct TimeZoneOption {
  const char* id;
  const char* rule;
};

// IANA names are UI/persistence keys; newlib uses their POSIX rules for localtime_r().
static constexpr TimeZoneOption TIME_ZONES[] = {
    {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Australia/Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Australia/Brisbane", "AEST-10"},
    {"Australia/Adelaide", "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
    {"Australia/Darwin", "ACST-9:30"},
    {"Australia/Perth", "AWST-8"},
    {"Australia/Hobart", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    {"Etc/UTC", "UTC0"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"Asia/Tokyo", "JST-9"},
    {"Asia/Singapore", "SGT-8"},
};

static constexpr size_t TIME_ZONE_COUNT = sizeof(TIME_ZONES) / sizeof(TIME_ZONES[0]);

inline const TimeZoneOption* findTimeZone(const char* id) {
  if (!id) return nullptr;
  for (const auto& zone : TIME_ZONES) {
    if (strcmp(zone.id, id) == 0) return &zone;
  }
  return nullptr;
}

// Saved as one NVS record. Older layouts are recognised by size and upgraded on
// load (see upgradeClockSettings): schema 1 = v1.5.0 and earlier, schema 2 = v1.6.0.
struct ClockSettings {
  uint32_t schema;
  char location[33];
  char zone[40];
  uint8_t brightness;
  uint8_t startClock;
  uint16_t clockHoldSeconds;  // Clock visit between demos.
  char scrollText[65];        // Empty = scroll device details.
  uint8_t nightEnabled;       // Display off between nightStart and nightEnd.
  uint16_t nightStartMinutes; // Minutes after local midnight, 0-1439.
  uint16_t nightEndMinutes;
};

struct ClockSettingsV2 {
  uint32_t schema;
  char location[33];
  char zone[40];
  uint8_t brightness;
  uint8_t startClock;
  uint16_t clockHoldSeconds;
  char scrollText[65];
};

struct ClockSettingsV1 {
  uint32_t schema;
  char location[33];
  char zone[40];
  uint8_t brightness;
  uint8_t startClock;
};

static_assert(sizeof(ClockSettings) != sizeof(ClockSettingsV2) &&
                  sizeof(ClockSettings) != sizeof(ClockSettingsV1) &&
                  sizeof(ClockSettingsV2) != sizeof(ClockSettingsV1),
              "Saved layouts are told apart by size");

constexpr uint32_t CLOCK_SETTINGS_SCHEMA = 3;
constexpr uint16_t CLOCK_HOLD_MIN_S = 5;
constexpr uint16_t CLOCK_HOLD_MAX_S = 3600;
constexpr size_t SCROLL_TEXT_MAX = 64;
constexpr uint16_t MINUTES_PER_DAY = 24 * 60;
constexpr uint16_t NIGHT_DEFAULT_START = 22 * 60; // 22:00
constexpr uint16_t NIGHT_DEFAULT_END = 6 * 60;    // 06:00

inline void setDefaultNight(ClockSettings& settings) {
  settings.nightEnabled = 1;
  settings.nightStartMinutes = NIGHT_DEFAULT_START;
  settings.nightEndMinutes = NIGHT_DEFAULT_END;
}

inline ClockSettings defaultClockSettings(uint8_t brightness, uint16_t clockHoldSeconds) {
  ClockSettings result = {};
  result.schema = CLOCK_SETTINGS_SCHEMA;
  strcpy(result.location, "Sydney");
  strcpy(result.zone, "Australia/Sydney");
  result.brightness = brightness;
  result.clockHoldSeconds = clockHoldSeconds;
  setDefaultNight(result);
  return result;
}

inline ClockSettings upgradeClockSettings(const ClockSettingsV2& old) {
  ClockSettings result = {};
  result.schema = old.schema == 2 ? CLOCK_SETTINGS_SCHEMA : 0; // Anything else stays invalid.
  memcpy(result.location, old.location, sizeof(result.location));
  memcpy(result.zone, old.zone, sizeof(result.zone));
  result.brightness = old.brightness;
  result.startClock = old.startClock;
  result.clockHoldSeconds = old.clockHoldSeconds;
  memcpy(result.scrollText, old.scrollText, sizeof(result.scrollText));
  setDefaultNight(result);
  return result;
}

inline ClockSettings upgradeClockSettings(const ClockSettingsV1& old, uint16_t clockHoldSeconds) {
  ClockSettings result = {};
  result.schema = old.schema == 1 ? CLOCK_SETTINGS_SCHEMA : 0;
  memcpy(result.location, old.location, sizeof(result.location));
  memcpy(result.zone, old.zone, sizeof(result.zone));
  result.brightness = old.brightness;
  result.startClock = old.startClock;
  result.clockHoldSeconds = clockHoldSeconds;
  setDefaultNight(result);
  return result;
}

// True when `minute` (after local midnight) is inside [start, end), wrapping past
// midnight when start > end. Equal times mean no night window.
inline bool inNightWindow(uint16_t minute, uint16_t start, uint16_t end) {
  if (start == end) return false;
  return start < end ? minute >= start && minute < end : minute >= start || minute < end;
}

inline bool validLocation(const char* location) {
  if (!location) return false;
  const size_t length = strnlen(location, 33);
  if (length == 0 || length > 32) return false;
  bool visible = false;
  for (size_t i = 0; i < length; ++i) {
    const uint8_t ch = static_cast<uint8_t>(location[i]);
    if (ch < 0x20 || ch == 0x7f) return false;
    if (ch != ' ') visible = true;
  }
  return visible;
}

// The panel font covers printable ASCII; empty selects the device-details message.
inline bool validScrollText(const char* text) {
  if (!text) return false;
  const size_t length = strnlen(text, SCROLL_TEXT_MAX + 1);
  if (length > SCROLL_TEXT_MAX) return false;
  for (size_t i = 0; i < length; ++i) {
    if (text[i] < 0x20 || text[i] > 0x7e) return false;
  }
  return true;
}

inline bool validClockSettings(const ClockSettings& settings) {
  return settings.schema == CLOCK_SETTINGS_SCHEMA && validLocation(settings.location) &&
         memchr(settings.zone, '\0', sizeof(settings.zone)) && findTimeZone(settings.zone) &&
         settings.startClock <= 1 && settings.clockHoldSeconds >= CLOCK_HOLD_MIN_S &&
         settings.clockHoldSeconds <= CLOCK_HOLD_MAX_S &&
         memchr(settings.scrollText, '\0', sizeof(settings.scrollText)) &&
         validScrollText(settings.scrollText) && settings.nightEnabled <= 1 &&
         settings.nightStartMinutes < MINUTES_PER_DAY && settings.nightEndMinutes < MINUTES_PER_DAY;
}

inline bool parseNumber(const char* value, unsigned maximum, unsigned& result) {
  if (!value || !*value) return false;
  unsigned number = 0;
  for (const char* p = value; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
    number = number * 10 + (*p - '0');
    if (number > maximum) return false;
  }
  result = number;
  return true;
}

inline bool parseBrightness(const char* value, uint8_t& result) {
  unsigned number = 0;
  if (!parseNumber(value, 255, number)) return false;
  result = number;
  return true;
}

inline bool parseClockHold(const char* value, uint16_t& result) {
  unsigned number = 0;
  if (!parseNumber(value, CLOCK_HOLD_MAX_S, number) || number < CLOCK_HOLD_MIN_S) return false;
  result = number;
  return true;
}

// "HH:MM", as sent by an <input type="time">.
inline bool parseTimeOfDay(const char* value, uint16_t& minutes) {
  if (!value || strlen(value) != 5 || value[2] != ':') return false;
  for (int i : {0, 1, 3, 4}) {
    if (value[i] < '0' || value[i] > '9') return false;
  }
  const unsigned hours = (value[0] - '0') * 10 + (value[1] - '0');
  const unsigned mins = (value[3] - '0') * 10 + (value[4] - '0');
  if (hours > 23 || mins > 59) return false;
  minutes = hours * 60 + mins;
  return true;
}

inline void formatTimeOfDay(uint16_t minutes, char (&text)[6]) {
  snprintf(text, sizeof(text), "%02u:%02u", static_cast<unsigned>(minutes / 60 % 24),
           static_cast<unsigned>(minutes % 60));
}
