#pragma once
#include "FreeRTOS.h"
extern unsigned notifications;
extern unsigned activeTasks;
extern bool failTask;
inline BaseType_t xTaskCreatePinnedToCore(void (*)(void*), const char*, uint32_t, void*, unsigned,
                                          TaskHandle_t* handle, int) {
  if (failTask) return pdFALSE;
  *handle = reinterpret_cast<void*>(1);
  ++activeTasks;
  return pdPASS;
}
inline void vTaskDelete(TaskHandle_t handle) {
  if (handle) --activeTasks;
}
inline void vTaskNotifyGiveFromISR(TaskHandle_t, BaseType_t* awakened) {
  ++notifications;
  *awakened = pdTRUE;
}
uint32_t ulTaskNotifyTake(BaseType_t, uint32_t);
