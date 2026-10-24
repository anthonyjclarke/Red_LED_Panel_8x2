#pragma once
#include "Arduino.h"
extern std::vector<uint8_t> nvs;
extern bool nvsWritable;
class Preferences {
public:
  bool begin(const char*, bool) { return true; }
  size_t getBytesLength(const char*) { return nvs.size(); }
  size_t getBytes(const char*, void* data, size_t count) {
    if (count != nvs.size()) return 0;
    memcpy(data, nvs.data(), count);
    return count;
  }
  size_t putBytes(const char*, const void* data, size_t count) {
    if (!nvsWritable) return 0;
    nvs.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + count);
    return count;
  }
};
