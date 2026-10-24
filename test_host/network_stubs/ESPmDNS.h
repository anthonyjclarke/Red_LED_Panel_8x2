#pragma once
#include <cstdint>
struct MDNSPort {
  bool begin(const char*) { return true; }
  void addService(const char*, const char*, int) {}
  void end() {}
  void enableArduino(uint16_t, bool) {}
};
static MDNSPort MDNS;
