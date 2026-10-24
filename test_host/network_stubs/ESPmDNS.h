#pragma once
struct MDNSPort {
  bool begin(const char*) { return true; }
  void addService(const char*, const char*, int) {}
  void end() {}
};
static MDNSPort MDNS;
