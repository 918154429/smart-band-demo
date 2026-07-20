#include "smart_band_clock.h"

#include <string.h>

#define SMART_BAND_RTC_MIN_VALID ((time_t)1577836800)

bool smart_band_clock_wall_is_valid(time_t wall_time)
{
  return wall_time >= SMART_BAND_RTC_MIN_VALID;
}

int smart_band_clock_init(smart_band_clock_t *clock,
                          const smart_band_clock_source_t *source)
{
  if (clock == NULL || source == NULL || source->monotonic_now == NULL)
    {
      return -1;
    }

  memset(clock, 0, sizeof(*clock));
  clock->source = *source;
  return 0;
}

bool smart_band_clock_sample(smart_band_clock_t *clock,
                             smart_band_clock_sample_t *sample)
{
  time_t wall_time;
  uint32_t monotonic_ms;
  bool wall_valid;

  if (clock == NULL || sample == NULL ||
      clock->source.monotonic_now == NULL)
    {
      return false;
    }

  wall_time = clock->source.wall_now == NULL ? (time_t)-1 :
              clock->source.wall_now(clock->source.context);
  monotonic_ms = clock->source.monotonic_now(clock->source.context);
  wall_valid = smart_band_clock_wall_is_valid(wall_time);

  memset(sample, 0, sizeof(*sample));
  sample->wall_time = wall_time;
  sample->monotonic_ms = monotonic_ms;
  sample->wall_valid = wall_valid;

  if (clock->sampled)
    {
      clock->elapsed_ms += (uint32_t)(monotonic_ms -
                                      clock->last_monotonic_ms);
      if (wall_valid && clock->last_wall_valid &&
          wall_time < clock->last_wall_time)
        {
          sample->wall_rollback = true;
          clock->wall_rollback_count++;
        }
    }

  clock->last_monotonic_ms = monotonic_ms;
  if (wall_valid)
    {
      clock->last_wall_time = wall_time;
      clock->last_wall_valid = true;
    }
  clock->sampled = true;
  sample->elapsed_ms = clock->elapsed_ms;
  return true;
}
