#include "watch_model.h"

#include <stdio.h>
#include <string.h>

#define SMART_BAND_UTC_OFFSET_SECONDS (8 * 60 * 60)

static smart_band_data_mode_t default_data_mode(void)
{
#if defined(CONFIG_LVX_DEMO_SMART_BAND_USE_SENSORS)
  return SMART_BAND_DATA_MODE_AUTO;
#else
  return SMART_BAND_DATA_MODE_SIMULATION;
#endif
}

static int clamp_int(int value, int min_value, int max_value)
{
  if (value < min_value)
    {
      return min_value;
    }

  if (value > max_value)
    {
      return max_value;
    }

  return value;
}

static void format_time(smart_band_state_t *state, time_t now)
{
  struct tm local_now;
  int year;
  int month;
  int day;

  if (!smart_band_display_time(now, &local_now))
    {
      snprintf(state->time_text, sizeof(state->time_text), "--:--");
      snprintf(state->date_text, sizeof(state->date_text), "----/--/--");
      state->time_valid = false;
      state->wall_time = (time_t)-1;
      return;
    }

  year = clamp_int(local_now.tm_year + 1900, 0, 9999);
  month = clamp_int(local_now.tm_mon + 1, 1, 12);
  day = clamp_int(local_now.tm_mday, 1, 31);

  snprintf(state->time_text, sizeof(state->time_text), "%02d:%02d",
           clamp_int(local_now.tm_hour, 0, 23),
           clamp_int(local_now.tm_min, 0, 59));
  snprintf(state->date_text, sizeof(state->date_text), "%04d/%02d/%02d",
           year, month, day);
  state->time_valid = true;
  state->wall_time = now;
}

void smart_band_state_set_wall_time(smart_band_state_t *state, time_t now,
                                    bool valid)
{
  if (state == NULL)
    {
      return;
    }

  if (valid)
    {
      format_time(state, now);
    }
  else
    {
      snprintf(state->time_text, sizeof(state->time_text), "--:--");
      snprintf(state->date_text, sizeof(state->date_text), "----/--/--");
      state->time_valid = false;
      state->wall_time = (time_t)-1;
    }
}

bool smart_band_display_time(time_t now, struct tm *display_time)
{
  struct tm *tm_result = NULL;

  if (display_time == NULL)
    {
      return false;
    }

#if defined(__NuttX__)
  now += SMART_BAND_UTC_OFFSET_SECONDS;
  tm_result = gmtime_r(&now, display_time);
#elif defined(_WIN32)
  if (localtime_s(display_time, &now) == 0)
    {
      tm_result = display_time;
    }
#elif defined(_POSIX_VERSION)
  tm_result = localtime_r(&now, display_time);
#else
  tm_result = localtime(&now);
  if (tm_result != NULL)
    {
      *display_time = *tm_result;
      tm_result = display_time;
    }
#endif

  return tm_result != NULL;
}

static bool source_is_sensor(smart_band_data_source_t source)
{
  return source == SMART_BAND_DATA_SOURCE_SENSOR ||
         source == SMART_BAND_DATA_SOURCE_SENSOR_DERIVED;
}

static smart_band_metric_info_t *metric_info(smart_band_state_t *state,
                                             smart_band_metric_t metric)
{
  if (state == NULL || metric < 0 || metric >= SMART_BAND_METRIC_COUNT)
    {
      return NULL;
    }

  return &state->metrics[metric];
}

static void update_status(smart_band_state_t *state)
{
  const smart_band_metric_info_t *heart =
    &state->metrics[SMART_BAND_METRIC_HEART_RATE];
  const smart_band_metric_info_t *steps =
    &state->metrics[SMART_BAND_METRIC_STEPS];
  bool heart_available =
    heart->freshness != SMART_BAND_DATA_FRESHNESS_UNAVAILABLE;
  bool steps_available =
    steps->freshness != SMART_BAND_DATA_FRESHNESS_UNAVAILABLE;

  if (heart_available && state->heart_rate >= 110)
    {
      snprintf(state->status_text, sizeof(state->status_text), "Active");
    }
  else if (steps_available && state->steps >= state->step_goal)
    {
      snprintf(state->status_text, sizeof(state->status_text), "Goal Met");
    }
  else if (!heart_available && !steps_available)
    {
      snprintf(state->status_text, sizeof(state->status_text), "Unavailable");
    }
  else
    {
      snprintf(state->status_text, sizeof(state->status_text), "Stable");
    }
}

static void set_metric_value(smart_band_state_t *state,
                             smart_band_metric_t metric, int value)
{
  switch (metric)
    {
      case SMART_BAND_METRIC_HEART_RATE:
        state->heart_rate = clamp_int(value, 0, 500);
        break;
      case SMART_BAND_METRIC_STEPS:
        state->steps = clamp_int(value, 0, 99999);
        break;
      case SMART_BAND_METRIC_BATTERY:
        state->battery_percent = clamp_int(value, 0, 100);
        break;
      case SMART_BAND_METRIC_TEMPERATURE:
        state->temperature_c = clamp_int(value, -100, 200);
        break;
      case SMART_BAND_METRIC_HUMIDITY:
        state->humidity_percent = clamp_int(value, 0, 100);
        break;
      default:
        break;
    }
}

static int simulated_metric_value(const smart_band_state_t *state,
                                  smart_band_metric_t metric)
{
  switch (metric)
    {
      case SMART_BAND_METRIC_HEART_RATE:
        return state->simulated_heart_rate;
      case SMART_BAND_METRIC_STEPS:
        return state->simulated_steps;
      case SMART_BAND_METRIC_BATTERY:
        return state->simulated_battery_percent;
      case SMART_BAND_METRIC_TEMPERATURE:
        return state->simulated_temperature_c;
      case SMART_BAND_METRIC_HUMIDITY:
        return state->simulated_humidity_percent;
      default:
        return 0;
    }
}

static void sync_legacy_sensor_flags(smart_band_state_t *state)
{
  state->heart_sensor_active =
    source_is_sensor(state->metrics[SMART_BAND_METRIC_HEART_RATE].source);
  state->step_sensor_active =
    source_is_sensor(state->metrics[SMART_BAND_METRIC_STEPS].source);
  state->battery_sensor_active =
    source_is_sensor(state->metrics[SMART_BAND_METRIC_BATTERY].source);
  state->temperature_sensor_active =
    source_is_sensor(state->metrics[SMART_BAND_METRIC_TEMPERATURE].source);
  state->humidity_sensor_active =
    source_is_sensor(state->metrics[SMART_BAND_METRIC_HUMIDITY].source);
}

static void use_simulated_metric(smart_band_state_t *state,
                                 smart_band_metric_t metric, time_t now)
{
  smart_band_metric_info_t *info = metric_info(state, metric);

  set_metric_value(state, metric, simulated_metric_value(state, metric));
  info->source = SMART_BAND_DATA_SOURCE_SIMULATED;
  info->freshness = SMART_BAND_DATA_FRESHNESS_FRESH;
  info->last_update = now;
  info->last_update_monotonic_ms = 0;
  info->monotonic_valid = false;
  if (metric == SMART_BAND_METRIC_BATTERY)
    {
      state->battery_charging = false;
    }
}

static void make_metric_unavailable(smart_band_state_t *state,
                                    smart_band_metric_t metric)
{
  smart_band_metric_info_t *info = metric_info(state, metric);

  info->source = SMART_BAND_DATA_SOURCE_UNAVAILABLE;
  info->freshness = SMART_BAND_DATA_FRESHNESS_UNAVAILABLE;
  if (metric == SMART_BAND_METRIC_BATTERY)
    {
      state->battery_charging = false;
    }
}

static void simulate_health_data(smart_band_state_t *state, time_t now)
{
  const int pulse_wave = (int)((state->ticks * 7u + 11u) % 23u);
  const int motion_wave = (int)((state->ticks * 5u + 3u) % 9u);
  smart_band_metric_t metric;

  state->simulated_heart_rate =
    clamp_int(66 + pulse_wave + motion_wave / 3, 55, 135);
  state->simulated_steps += 4 + (int)(state->ticks % 6u);
  state->simulated_temperature_c =
    clamp_int(24 + (int)(state->ticks % 3u) - 1, -40, 80);
  state->simulated_humidity_percent =
    clamp_int(56 + (int)(state->ticks % 8u), 0, 100);

  if (state->simulated_steps > 99999)
    {
      state->simulated_steps = state->simulated_steps % state->step_goal;
    }

  state->simulated_battery_percent =
    clamp_int(96 - (int)(state->ticks / 180u), 5, 100);

  for (metric = SMART_BAND_METRIC_HEART_RATE;
       metric < SMART_BAND_METRIC_COUNT; metric++)
    {
      smart_band_metric_info_t *info = metric_info(state, metric);

      if (state->data_mode == SMART_BAND_DATA_MODE_SIMULATION ||
          (state->data_mode == SMART_BAND_DATA_MODE_AUTO &&
           !source_is_sensor(info->source)))
        {
          use_simulated_metric(state, metric, now);
        }
    }

  update_status(state);
  sync_legacy_sensor_flags(state);
}

void smart_band_state_init(smart_band_state_t *state, time_t now)
{
  smart_band_state_init_mode(state, now, default_data_mode());
}

void smart_band_state_init_mode(smart_band_state_t *state, time_t now,
                                smart_band_data_mode_t mode)
{
  smart_band_metric_t metric;

  if (state == NULL)
    {
      return;
    }

  if (mode < SMART_BAND_DATA_MODE_AUTO ||
      mode > SMART_BAND_DATA_MODE_SENSORS_ONLY)
    {
      mode = default_data_mode();
    }

  memset(state, 0, sizeof(*state));
  state->page = SMART_BAND_PAGE_FACE;
  state->heart_rate = 72;
  state->steps = 1260;
  state->step_goal = SMART_BAND_STEP_GOAL_DEFAULT;
  state->battery_percent = 96;
  state->temperature_c = 24;
  state->humidity_percent = 60;
  state->simulated_heart_rate = state->heart_rate;
  state->simulated_steps = state->steps;
  state->simulated_battery_percent = state->battery_percent;
  state->simulated_temperature_c = state->temperature_c;
  state->simulated_humidity_percent = state->humidity_percent;
  state->data_mode = mode;

  for (metric = SMART_BAND_METRIC_HEART_RATE;
       metric < SMART_BAND_METRIC_COUNT; metric++)
    {
      state->metrics[metric].ttl_seconds =
        SMART_BAND_SENSOR_TTL_SECONDS_DEFAULT;
      if (mode == SMART_BAND_DATA_MODE_SENSORS_ONLY)
        {
          make_metric_unavailable(state, metric);
        }
      else
        {
          use_simulated_metric(state, metric, now);
        }
    }

  update_status(state);
  sync_legacy_sensor_flags(state);
  format_time(state, now);
}

void smart_band_state_tick(smart_band_state_t *state, time_t now)
{
  if (state == NULL)
    {
      return;
    }

  state->ticks++;
  format_time(state, now);
  simulate_health_data(state, now);
}

void smart_band_state_set_data_mode(smart_band_state_t *state,
                                    smart_band_data_mode_t mode)
{
  smart_band_state_set_data_mode_at(state, mode, time(NULL));
}

void smart_band_state_set_data_mode_at(smart_band_state_t *state,
                                       smart_band_data_mode_t mode,
                                       time_t now)
{
  smart_band_metric_t metric;

  if (state == NULL || mode < SMART_BAND_DATA_MODE_AUTO ||
      mode > SMART_BAND_DATA_MODE_SENSORS_ONLY)
    {
      return;
    }

  state->data_mode = mode;
  for (metric = SMART_BAND_METRIC_HEART_RATE;
       metric < SMART_BAND_METRIC_COUNT; metric++)
    {
      smart_band_metric_info_t *info = metric_info(state, metric);

      if (mode == SMART_BAND_DATA_MODE_SIMULATION ||
          (mode == SMART_BAND_DATA_MODE_AUTO &&
           !source_is_sensor(info->source)))
        {
          use_simulated_metric(state, metric, now);
        }
      else if (mode == SMART_BAND_DATA_MODE_SENSORS_ONLY &&
               !source_is_sensor(info->source))
        {
          make_metric_unavailable(state, metric);
        }
    }

  update_status(state);
  sync_legacy_sensor_flags(state);
}

static bool metric_is_expired(const smart_band_metric_info_t *info,
                              time_t wall_now, uint64_t monotonic_ms,
                              bool monotonic_valid)
{
  if (monotonic_valid && info->monotonic_valid)
    {
      return monotonic_ms - info->last_update_monotonic_ms >
             (uint64_t)info->ttl_seconds * 1000u;
    }

  if (wall_now < info->last_update)
    {
      /* A wall-clock correction must not keep an old sample alive forever. */

      return true;
    }

  if (wall_now == info->last_update)
    {
      return false;
    }

  return (unsigned long)(wall_now - info->last_update) > info->ttl_seconds;
}

void smart_band_state_begin_sensor_cycle(smart_band_state_t *state,
                                         time_t now)
{
  smart_band_state_begin_sensor_cycle_at(
    state, now, SMART_BAND_MONOTONIC_INVALID, false);
}

void smart_band_state_begin_sensor_cycle_at(smart_band_state_t *state,
                                            time_t wall_now,
                                            uint64_t monotonic_ms,
                                            bool wall_rollback)
{
  smart_band_metric_t metric;

  if (state == NULL)
    {
      return;
    }

  for (metric = SMART_BAND_METRIC_HEART_RATE;
       metric < SMART_BAND_METRIC_COUNT; metric++)
    {
      smart_band_metric_info_t *info = metric_info(state, metric);

      if (!source_is_sensor(info->source))
        {
          continue;
        }

      if (!wall_rollback &&
          !metric_is_expired(info, wall_now, monotonic_ms,
                             monotonic_ms != SMART_BAND_MONOTONIC_INVALID))
        {
          info->freshness = SMART_BAND_DATA_FRESHNESS_STALE;
        }
      else if (state->data_mode == SMART_BAND_DATA_MODE_AUTO)
        {
          use_simulated_metric(state, metric, wall_now);
        }
      else
        {
          make_metric_unavailable(state, metric);
        }
    }

  update_status(state);
  sync_legacy_sensor_flags(state);
}

bool smart_band_state_publish_metric(smart_band_state_t *state,
                                     smart_band_metric_t metric,
                                     int value,
                                     smart_band_data_source_t source,
                                     time_t now)
{
  return smart_band_state_publish_metric_at(state, metric, value, source,
                                            now,
                                            SMART_BAND_MONOTONIC_INVALID);
}

bool smart_band_state_publish_metric_at(smart_band_state_t *state,
                                        smart_band_metric_t metric,
                                        int value,
                                        smart_band_data_source_t source,
                                        time_t wall_now,
                                        uint64_t monotonic_ms)
{
  smart_band_metric_info_t *info;

  if (state == NULL || !source_is_sensor(source) ||
      state->data_mode == SMART_BAND_DATA_MODE_SIMULATION)
    {
      return false;
    }

  info = metric_info(state, metric);
  if (info == NULL)
    {
      return false;
    }

  set_metric_value(state, metric, value);
  info->source = source;
  info->freshness = SMART_BAND_DATA_FRESHNESS_FRESH;
  info->last_update = wall_now;
  info->last_update_monotonic_ms = monotonic_ms;
  info->monotonic_valid = monotonic_ms != SMART_BAND_MONOTONIC_INVALID;
  update_status(state);
  sync_legacy_sensor_flags(state);
  return true;
}

const smart_band_metric_info_t *
smart_band_state_metric_info(const smart_band_state_t *state,
                             smart_band_metric_t metric)
{
  if (state == NULL || metric < 0 || metric >= SMART_BAND_METRIC_COUNT)
    {
      return NULL;
    }

  return &state->metrics[metric];
}

void smart_band_next_page(smart_band_state_t *state)
{
  if (state == NULL)
    {
      return;
    }

  state->page = (smart_band_page_t)((state->page + 1) % SMART_BAND_PAGE_COUNT);
}

void smart_band_prev_page(smart_band_state_t *state)
{
  if (state == NULL)
    {
      return;
    }

  state->page = (smart_band_page_t)((state->page + SMART_BAND_PAGE_COUNT - 1) %
                                    SMART_BAND_PAGE_COUNT);
}

void smart_band_adjust_step_goal(smart_band_state_t *state, int delta)
{
  if (state == NULL)
    {
      return;
    }

  state->step_goal = clamp_int(state->step_goal + delta,
                               SMART_BAND_STEP_GOAL_MIN,
                               SMART_BAND_STEP_GOAL_MAX);
}

const char *smart_band_page_title(smart_band_page_t page)
{
  switch (page)
    {
      case SMART_BAND_PAGE_FACE:
        return "Face";
      case SMART_BAND_PAGE_HEART:
        return "Heart";
      case SMART_BAND_PAGE_STEPS:
        return "Steps";
      case SMART_BAND_PAGE_APPS:
        return "Apps";
      default:
        return "Unknown";
    }
}

int smart_band_step_progress(const smart_band_state_t *state)
{
  if (state == NULL || state->steps <= 0 || state->step_goal <= 0)
    {
      return 0;
    }

  return clamp_int((state->steps * 100) / state->step_goal, 0, 100);
}
