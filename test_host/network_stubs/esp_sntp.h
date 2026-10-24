#pragma once
#include "Arduino.h"
extern std::function<void(timeval*)> syncCallback;
extern unsigned ntpRequests;
inline void sntp_set_time_sync_notification_cb(void (*callback)(timeval*)) {
  syncCallback = callback;
}
inline void configTzTime(const char* zone, const char*, const char*) {
  ++ntpRequests;
  setenv("TZ", zone, 1);
  tzset();
}
