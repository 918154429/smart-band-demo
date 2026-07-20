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
#define SMART_BAND_SENSOR_TTL_SECONDS_DEFAULT 5

typedef enum
{
  SMART_BAND_PAGE_FACE = 0,
  SMART_BAND_PAGE_HEART,
  SMART_BAND_PAGE_STEPS,
  SMART_BAND_PAGE_APPS,
  SMART_BAND_PAGE_COUNT
} smart_band_page_t;

typedef enum
{
  SMART_BAND_DATA_MODE_AUTO = 0,
  SMART_BAND_DATA_MODE_SIMULATION,
  SMART_BAND_DATA_MODE_SENSORS_ONLY
} smart_band_data_mode_t;

typedef enum
{
  SMART_BAND_DATA_SOURCE_UNAVAILABLE = 0,
  SMART_BAND_DATA_SOURCE_SIMULATED,
  SMART_BAND_DATA_SOURCE_SENSOR,
  SMART_BAND_DATA_SOURCE_SENSOR_DERIVED
} smart_band_data_source_t;

typedef enum
{
  SMART_BAND_DATA_FRESHNESS_UNAVAILABLE = 0,
  SMART_BAND_DATA_FRESHNESS_FRESH,
  SMART_BAND_DATA_FRESHNESS_STALE
} smart_band_data_freshness_t;

typedef enum
{
  SMART_BAND_METRIC_HEART_RATE = 0,
  SMART_BAND_METRIC_STEPS,
  SMART_BAND_METRIC_BATTERY,
  SMART_BAND_METRIC_TEMPERATURE,
  SMART_BAND_METRIC_HUMIDITY,
  SMART_BAND_METRIC_COUNT
} smart_band_metric_t;

typedef struct
{
  smart_band_data_source_t source;
  smart_band_data_freshness_t freshness;
  time_t last_update;
  unsigned int ttl_seconds;
} smart_band_metric_info_t;

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
  smart_band_data_mode_t data_mode;
  smart_band_metric_info_t metrics[SMART_BAND_METRIC_COUNT];
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
  int simulated_heart_rate;
  int simulated_steps;
  int simulated_battery_percent;
  int simulated_temperature_c;
  int simulated_humidity_percent;
} smart_band_state_t;

void smart_band_state_init(smart_band_state_t *state, time_t now);
void smart_band_state_init_mode(smart_band_state_t *state, time_t now,
                                smart_band_data_mode_t mode);
void smart_band_state_tick(smart_band_state_t *state, time_t now);
void smart_band_state_set_data_mode(smart_band_state_t *state,
                                    smart_band_data_mode_t mode);
void smart_band_state_set_data_mode_at(smart_band_state_t *state,
                                       smart_band_data_mode_t mode,
                                       time_t now);
void smart_band_state_begin_sensor_cycle(smart_band_state_t *state,
                                         time_t now);
bool smart_band_state_publish_metric(smart_band_state_t *state,
                                     smart_band_metric_t metric,
                                     int value,
                                     smart_band_data_source_t source,
                                     time_t now);
const smart_band_metric_info_t *
smart_band_state_metric_info(const smart_band_state_t *state,
                             smart_band_metric_t metric);
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
