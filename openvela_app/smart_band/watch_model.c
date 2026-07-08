#include "watch_model.h"

#include <stdio.h>
#include <string.h>

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
  struct tm *tm_result = NULL;

#if defined(_POSIX_VERSION) || defined(__NuttX__)
  tm_result = localtime_r(&now, &local_now);
#else
  tm_result = localtime(&now);
  if (tm_result != NULL)
    {
      local_now = *tm_result;
      tm_result = &local_now;
    }
#endif

  if (tm_result == NULL)
    {
      snprintf(state->time_text, sizeof(state->time_text), "--:--");
      snprintf(state->date_text, sizeof(state->date_text), "----/--/--");
      state->time_valid = false;
      return;
    }

  snprintf(state->time_text, sizeof(state->time_text), "%02d:%02d",
           local_now.tm_hour, local_now.tm_min);
  snprintf(state->date_text, sizeof(state->date_text), "%04d/%02d/%02d",
           local_now.tm_year + 1900, local_now.tm_mon + 1, local_now.tm_mday);
  state->time_valid = true;
}

static void simulate_health_data(smart_band_state_t *state)
{
  const int pulse_wave = (int)((state->ticks * 7u + 11u) % 23u);
  const int motion_wave = (int)((state->ticks * 5u + 3u) % 9u);

  state->heart_rate = clamp_int(66 + pulse_wave + motion_wave / 3, 55, 135);
  state->steps += 4 + (int)(state->ticks % 6u);

  if (state->steps > 99999)
    {
      state->steps = state->steps % SMART_BAND_STEP_GOAL;
    }

  state->battery_percent = clamp_int(96 - (int)(state->ticks / 180u), 5, 100);

  if (state->heart_rate >= 110)
    {
      snprintf(state->status_text, sizeof(state->status_text), "Active");
    }
  else if (state->steps >= SMART_BAND_STEP_GOAL)
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
  state->battery_percent = 96;
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
      default:
        return "Unknown";
    }
}

int smart_band_step_progress(const smart_band_state_t *state)
{
  if (state == NULL || state->steps <= 0)
    {
      return 0;
    }

  return clamp_int((state->steps * 100) / SMART_BAND_STEP_GOAL, 0, 100);
}
