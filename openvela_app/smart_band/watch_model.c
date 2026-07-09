#include "watch_model.h"

#include <stdio.h>
#include <string.h>

#define SMART_BAND_UTC_OFFSET_SECONDS (8 * 60 * 60)

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

static void simulate_health_data(smart_band_state_t *state)
{
  const int pulse_wave = (int)((state->ticks * 7u + 11u) % 23u);
  const int motion_wave = (int)((state->ticks * 5u + 3u) % 9u);

  state->heart_rate = clamp_int(66 + pulse_wave + motion_wave / 3, 55, 135);
  state->steps += 4 + (int)(state->ticks % 6u);
  state->temperature_c = clamp_int(24 + (int)(state->ticks % 3u) - 1,
                                   -40, 80);

  if (state->steps > 99999)
    {
      state->steps = state->steps % state->step_goal;
    }

  state->battery_percent = clamp_int(96 - (int)(state->ticks / 180u), 5, 100);

  if (state->heart_rate >= 110)
    {
      snprintf(state->status_text, sizeof(state->status_text), "Active");
    }
  else if (state->steps >= state->step_goal)
    {
      snprintf(state->status_text, sizeof(state->status_text), "Goal Met");
    }
  else
    {
      snprintf(state->status_text, sizeof(state->status_text), "Stable");
    }
}

void smart_band_state_init(smart_band_state_t *state, time_t now)
{
  if (state == NULL)
    {
      return;
    }

  memset(state, 0, sizeof(*state));
  state->page = SMART_BAND_PAGE_FACE;
  state->heart_rate = 72;
  state->steps = 1260;
  state->step_goal = SMART_BAND_STEP_GOAL_DEFAULT;
  state->battery_percent = 96;
  state->temperature_c = 24;
  snprintf(state->status_text, sizeof(state->status_text), "Stable");
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
  simulate_health_data(state);
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
