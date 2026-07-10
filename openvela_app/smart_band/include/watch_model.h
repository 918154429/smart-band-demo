#ifndef SMART_BAND_WATCH_MODEL_H
#define SMART_BAND_WATCH_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_TIME_TEXT_LEN 8
#define SMART_BAND_DATE_TEXT_LEN 20
#define SMART_BAND_STATUS_TEXT_LEN 32
#define SMART_BAND_STEP_GOAL_DEFAULT 8000
#define SMART_BAND_STEP_GOAL_MIN 1000
#define SMART_BAND_STEP_GOAL_MAX 50000
#define SMART_BAND_STEP_GOAL_DELTA 1000

typedef enum
{
  SMART_BAND_PAGE_FACE = 0,
  SMART_BAND_PAGE_HEART,
  SMART_BAND_PAGE_STEPS,
  SMART_BAND_PAGE_APPS,
  SMART_BAND_PAGE_COUNT
} smart_band_page_t;

typedef struct
{
  smart_band_page_t page;
  unsigned int ticks;
  int heart_rate;
  int steps;
  int step_goal;
  int battery_percent;
  int temperature_c;
  int humidity_percent;
  bool heart_sensor_active;
  bool step_sensor_active;
  bool battery_sensor_active;
  bool battery_charging;
  bool temperature_sensor_active;
  bool humidity_sensor_active;
  char time_text[SMART_BAND_TIME_TEXT_LEN];
  char date_text[SMART_BAND_DATE_TEXT_LEN];
  char status_text[SMART_BAND_STATUS_TEXT_LEN];
  bool time_valid;
} smart_band_state_t;

void smart_band_state_init(smart_band_state_t *state, time_t now);
void smart_band_state_tick(smart_band_state_t *state, time_t now);
void smart_band_next_page(smart_band_state_t *state);
void smart_band_prev_page(smart_band_state_t *state);
void smart_band_adjust_step_goal(smart_band_state_t *state, int delta);
const char *smart_band_page_title(smart_band_page_t page);
int smart_band_step_progress(const smart_band_state_t *state);
bool smart_band_display_time(time_t now, struct tm *display_time);

#ifdef __cplusplus
}
#endif

#endif
