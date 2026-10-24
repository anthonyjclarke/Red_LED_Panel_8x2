#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <algorithm>
#define PROGMEM
#define PI 3.14159265358979323846
#define DEG_TO_RAD (PI / 180.0)
class __FlashStringHelper;
class String : public std::string {
public:
  using std::string::string;
};
extern uint32_t fakeMillis;
inline uint32_t millis() {
  return fakeMillis;
}
inline void delay(uint32_t ms) {
  fakeMillis += ms;
}
struct SerialPort {
  std::string input, output;
  void begin(int) {}
  int available() { return input.size(); }
  int read() {
    char c = input[0];
    input.erase(0, 1);
    return c;
  }
  void println(const char* s) {
    output += s;
    output += '\n';
  }
  void printf(const char* format, ...) {
    char out[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(out, sizeof(out), format, args);
    va_end(args);
    output += out;
  }
};
extern SerialPort Serial;
inline double radians(double value) {
  return value * DEG_TO_RAD;
}
