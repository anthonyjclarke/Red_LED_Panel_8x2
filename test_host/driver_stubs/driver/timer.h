#pragma once
#include <cstdint>
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_ERR_NO_MEM = 1, ESP_ERR_NOT_FOUND = 2;
inline const char* esp_err_to_name(esp_err_t) {
  return "fixture error";
}
enum timer_group_t { TIMER_GROUP_0 };
enum timer_idx_t { TIMER_0, TIMER_1 };
enum timer_alarm_t { TIMER_ALARM_DIS, TIMER_ALARM_EN };
enum timer_autoreload_t { TIMER_AUTORELOAD_DIS, TIMER_AUTORELOAD_EN };
enum timer_start_t { TIMER_PAUSE, TIMER_START };
constexpr int TIMER_COUNT_UP = 1, TIMER_INTR_LEVEL = 1;
struct timer_config_t {
  int divider, counter_dir;
  timer_start_t counter_en;
  timer_alarm_t alarm_en;
  timer_autoreload_t auto_reload;
  int intr_type;
};
esp_err_t timer_init(timer_group_t, timer_idx_t, const timer_config_t*);
esp_err_t timer_set_counter_value(timer_group_t, timer_idx_t, uint64_t);
esp_err_t timer_get_counter_value(timer_group_t, timer_idx_t, uint64_t*);
esp_err_t timer_set_alarm_value(timer_group_t, timer_idx_t, uint64_t);
esp_err_t timer_isr_callback_add(timer_group_t, timer_idx_t, bool (*)(void*), void*, int);
esp_err_t timer_isr_callback_remove(timer_group_t, timer_idx_t);
esp_err_t timer_deinit(timer_group_t, timer_idx_t);
esp_err_t timer_set_alarm(timer_group_t, timer_idx_t, timer_alarm_t);
esp_err_t timer_start(timer_group_t, timer_idx_t);
esp_err_t timer_pause(timer_group_t, timer_idx_t);
