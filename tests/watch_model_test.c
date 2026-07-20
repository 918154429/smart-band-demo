#include "watch_model.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHECK(condition)                                                     \
  do                                                                         \
    {                                                                        \
      if (!(condition))                                                      \
        {                                                                    \
          fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
                  #condition);                                               \
          return 1;                                                          \
        }                                                                    \
    }                                                                        \
  while (0)

static time_t local_time_value(int year, int month, int day, int hour,
                               int minute, int second)
{
  struct tm value;

  memset(&value, 0, sizeof(value));
  value.tm_year = year - 1900;
  value.tm_mon = month - 1;
  value.tm_mday = day;
  value.tm_hour = hour;
  value.tm_min = minute;
  value.tm_sec = second;
  value.tm_isdst = -1;
  return mktime(&value);
}

static int test_time_and_initial_state(void)
{
  smart_band_state_t state;
  struct tm display_time;
  const time_t now = local_time_value(2026, 7, 6, 9, 5, 0);

  CHECK(now != (time_t)-1);
  CHECK(smart_band_display_time(now, &display_time));
  CHECK(!smart_band_display_time(now, NULL));

  smart_band_state_init(&state, now);
  CHECK(state.page == SMART_BAND_PAGE_FACE);
  CHECK(state.time_valid);
  CHECK(strcmp(state.time_text, "09:05") == 0);
  CHECK(strcmp(state.date_text, "2026/07/06") == 0);
  CHECK(state.heart_rate == 72);
  CHECK(state.steps == 1260);
  CHECK(state.step_goal == SMART_BAND_STEP_GOAL_DEFAULT);
  CHECK(state.battery_percent == 96);
  CHECK(strcmp(state.status_text, "Stable") == 0);

  smart_band_state_init(NULL, now);
  smart_band_state_tick(NULL, now);
  return 0;
}

static int test_page_navigation_and_titles(void)
{
  smart_band_state_t state;

  smart_band_state_init(&state, time(NULL));
  smart_band_prev_page(&state);
  CHECK(state.page == SMART_BAND_PAGE_APPS);
  smart_band_next_page(&state);
  CHECK(state.page == SMART_BAND_PAGE_FACE);
  smart_band_next_page(&state);
  CHECK(state.page == SMART_BAND_PAGE_HEART);

  CHECK(strcmp(smart_band_page_title(SMART_BAND_PAGE_FACE), "Face") == 0);
  CHECK(strcmp(smart_band_page_title(SMART_BAND_PAGE_HEART), "Heart") == 0);
  CHECK(strcmp(smart_band_page_title(SMART_BAND_PAGE_STEPS), "Steps") == 0);
  CHECK(strcmp(smart_band_page_title(SMART_BAND_PAGE_APPS), "Apps") == 0);
  CHECK(strcmp(smart_band_page_title(SMART_BAND_PAGE_COUNT), "Unknown") == 0);

  smart_band_next_page(NULL);
  smart_band_prev_page(NULL);
  return 0;
}

static int test_tick_ranges(void)
{
  smart_band_state_t state;
  time_t now = local_time_value(2026, 7, 6, 9, 5, 0);
  int index;

  smart_band_state_init(&state, now);
  for (index = 1; index < 600; index++)
    {
      smart_band_state_tick(&state, now + index);
      CHECK(state.heart_rate >= 55 && state.heart_rate <= 135);
      CHECK(state.battery_percent >= 5 && state.battery_percent <= 100);
      CHECK(state.temperature_c >= -40 && state.temperature_c <= 80);
      CHECK(state.humidity_percent >= 0 && state.humidity_percent <= 100);
      CHECK(smart_band_step_progress(&state) >= 0);
      CHECK(smart_band_step_progress(&state) <= 100);
    }

  CHECK(state.ticks == 599u);
  CHECK(state.steps > 1260);
  return 0;
}

static int test_step_goal_and_progress(void)
{
  smart_band_state_t state;
  int index;

  smart_band_state_init(&state, time(NULL));
  state.steps = 4000;
  CHECK(smart_band_step_progress(&state) == 50);

  smart_band_adjust_step_goal(&state, SMART_BAND_STEP_GOAL_DELTA);
  CHECK(state.step_goal == 9000);
  CHECK(smart_band_step_progress(&state) == 44);

  for (index = 0; index < 20; index++)
    {
      smart_band_adjust_step_goal(&state, -SMART_BAND_STEP_GOAL_DELTA);
    }
  CHECK(state.step_goal == SMART_BAND_STEP_GOAL_MIN);

  for (index = 0; index < 100; index++)
    {
      smart_band_adjust_step_goal(&state, SMART_BAND_STEP_GOAL_DELTA);
    }
  CHECK(state.step_goal == SMART_BAND_STEP_GOAL_MAX);

  state.steps = 0;
  CHECK(smart_band_step_progress(&state) == 0);
  state.steps = 1;
  state.step_goal = 0;
  CHECK(smart_band_step_progress(&state) == 0);
  CHECK(smart_band_step_progress(NULL) == 0);
  smart_band_adjust_step_goal(NULL, SMART_BAND_STEP_GOAL_DELTA);
  return 0;
}

int main(void)
{
  CHECK(test_time_and_initial_state() == 0);
  CHECK(test_page_navigation_and_titles() == 0);
  CHECK(test_tick_ranges() == 0);
  CHECK(test_step_goal_and_progress() == 0);
  puts("watch_model production C tests passed");
  return 0;
}
