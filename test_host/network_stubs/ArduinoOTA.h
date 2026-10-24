#pragma once
#include "Arduino.h"
// Host fixture: records configuration; no UDP listener or flash writes.
using ota_error_t = int;
struct ArduinoOTAPort {
  unsigned begins = 0, handles = 0;
  std::string hostname, password;
  uint16_t port = 0;
  bool mdns = true;
  std::function<void()> start, end;
  std::function<void(unsigned int, unsigned int)> progress;
  std::function<void(ota_error_t)> error;
  ArduinoOTAPort& setHostname(const char* name) { hostname = name; return *this; }
  ArduinoOTAPort& setPort(uint16_t value) { port = value; return *this; }
  ArduinoOTAPort& setPassword(const char* value) { password = value; return *this; }
  ArduinoOTAPort& setMdnsEnabled(bool enabled) { mdns = enabled; return *this; }
  ArduinoOTAPort& onStart(std::function<void()> fn) { start = fn; return *this; }
  ArduinoOTAPort& onEnd(std::function<void()> fn) { end = fn; return *this; }
  ArduinoOTAPort& onProgress(std::function<void(unsigned int, unsigned int)> fn) { progress = fn; return *this; }
  ArduinoOTAPort& onError(std::function<void(ota_error_t)> fn) { error = fn; return *this; }
  void begin() { ++begins; }
  void handle() { ++handles; }
};
extern ArduinoOTAPort ArduinoOTA;
