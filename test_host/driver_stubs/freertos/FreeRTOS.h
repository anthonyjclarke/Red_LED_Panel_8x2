#pragma once
#include <cstdint>
using portMUX_TYPE = int;
using BaseType_t = int;
using TaskHandle_t = void*;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define configMAX_PRIORITIES 25
inline int xPortGetCoreID() {
  return 1;
}
