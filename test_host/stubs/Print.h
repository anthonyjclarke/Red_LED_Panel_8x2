#pragma once
#include "Arduino.h"
class Print {
public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  size_t write(const uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; ++i)
      write(buffer[i]);
    return size;
  }
  size_t print(const char* text) {
    return write(reinterpret_cast<const uint8_t*>(text), strlen(text));
  }
};
