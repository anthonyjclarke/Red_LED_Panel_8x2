#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include <cstdarg>
#include <time.h>
#include <sys/time.h>
#define PROGMEM
class String {
public:
  String() = default;
  String(const char* text) : value(text) {}
  String(char ch) : value(1, ch) {}
  String(const std::string& text) : value(text) {}
  template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
  String(T number) : value(std::to_string(number)) {}
  const char* c_str() const { return value.c_str(); }
  size_t length() const { return value.size(); }
  void reserve(size_t size) { value.reserve(size); }
  void trim() {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
      value.clear();
    else
      value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
  }
  String& operator+=(const String& other) {
    value += other.value;
    return *this;
  }
  friend String operator+(const String& a, const String& b) { return String(a.value + b.value); }
  friend bool operator==(const String& a, const String& b) { return a.value == b.value; }
  friend bool operator!=(const String& a, const String& b) { return !(a == b); }

private:
  std::string value;
};
extern uint32_t fakeMillis;
inline uint32_t millis() {
  return fakeMillis;
}
inline void configTzTime(const char* zone, const char*, const char*);
inline size_t strlcpy(char* to, const char* from, size_t size) {
  const auto n = strlen(from);
  if (size) {
    std::strncpy(to, from, size - 1);
    to[size - 1] = 0;
  }
  return n;
}
struct SerialPort {
  std::string output;
  void printf(const char* format, ...) {
    char text[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    output += text;
  }
};
extern SerialPort Serial;
struct ESPPort {
  uint64_t getEfuseMac() const { return 0x1234abcdefULL; }
};
static ESPPort ESP;
inline uint32_t esp_random() {
  return 0x1234abcd;
}
