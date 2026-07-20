#ifndef SMART_BAND_CLOCK_H
#define SMART_BAND_CLOCK_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef time_t (*smart_band_wall_now_fn)(void *context);
typedef uint32_t (*smart_band_monotonic_now_fn)(void *context);

typedef struct
{
  smart_band_wall_now_fn wall_now;
  smart_band_monotonic_now_fn monotonic_now;
  void *context;
} smart_band_clock_source_t;

typedef struct
{
  time_t wall_time;
  uint32_t monotonic_ms;
  uint64_t elapsed_ms;
  bool wall_valid;
  bool wall_rollback;
} smart_band_clock_sample_t;

typedef struct
{
  smart_band_clock_source_t source;
  time_t last_wall_time;
  uint32_t last_monotonic_ms;
  uint64_t elapsed_ms;
  unsigned int wall_rollback_count;
  bool sampled;
  bool last_wall_valid;
} smart_band_clock_t;

bool smart_band_clock_wall_is_valid(time_t wall_time);
int smart_band_clock_init(smart_band_clock_t *clock,
                          const smart_band_clock_source_t *source);
bool smart_band_clock_sample(smart_band_clock_t *clock,
                             smart_band_clock_sample_t *sample);

#ifdef __cplusplus
}
#endif

#endif
