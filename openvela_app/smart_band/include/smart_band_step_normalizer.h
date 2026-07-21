#ifndef SMART_BAND_STEP_NORMALIZER_H
#define SMART_BAND_STEP_NORMALIZER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  SMART_BAND_STEP_SOURCE_SENSOR = 0,
  SMART_BAND_STEP_SOURCE_DERIVED,
  SMART_BAND_STEP_SOURCE_SIMULATION,
  SMART_BAND_STEP_SOURCE_COUNT
} smart_band_step_source_t;

typedef enum
{
  SMART_BAND_STEP_REASON_NONE = 0,
  SMART_BAND_STEP_REASON_BASELINE,
  SMART_BAND_STEP_REASON_DELTA,
  SMART_BAND_STEP_REASON_SOURCE_SWITCH,
  SMART_BAND_STEP_REASON_UNAVAILABLE,
  SMART_BAND_STEP_REASON_STALE,
  SMART_BAND_STEP_REASON_GAP,
  SMART_BAND_STEP_REASON_RESET,
  SMART_BAND_STEP_REASON_WRAP,
  SMART_BAND_STEP_REASON_FORWARD_JUMP
} smart_band_step_reason_t;

typedef enum
{
  SMART_BAND_STEP_RESULT_OK = 0,
  SMART_BAND_STEP_RESULT_INVALID_ARGUMENT,
  SMART_BAND_STEP_RESULT_INVALID_SAMPLE,
  SMART_BAND_STEP_RESULT_TIME_REGRESSION,
  SMART_BAND_STEP_RESULT_OVERFLOW
} smart_band_step_result_t;

#define SMART_BAND_STEP_QUALITY_ACCEPTED       (1u << 0)
#define SMART_BAND_STEP_QUALITY_REBASED        (1u << 1)
#define SMART_BAND_STEP_QUALITY_DISCONTINUITY  (1u << 2)
#define SMART_BAND_STEP_QUALITY_SOURCE_CHANGED (1u << 3)
#define SMART_BAND_STEP_QUALITY_UNAVAILABLE    (1u << 4)
#define SMART_BAND_STEP_QUALITY_STALE          (1u << 5)
#define SMART_BAND_STEP_QUALITY_WRAPPED        (1u << 6)

typedef struct
{
  uint8_t counter_bits;
  uint64_t max_forward_delta;
  uint64_t wrap_high_threshold;
  uint64_t wrap_low_threshold;
  uint64_t max_sample_gap_ms;
} smart_band_step_normalizer_config_t;

typedef struct
{
  smart_band_step_source_t source;
  uint64_t raw_counter;
  bool available;
  bool fresh;
  uint64_t monotonic_ms;
} smart_band_step_sample_t;

typedef struct
{
  uint64_t delta;
  uint64_t total;
  smart_band_step_reason_t reason;
  uint32_t quality_flags;
} smart_band_step_output_t;

typedef struct
{
  smart_band_step_normalizer_config_t config;
  uint64_t total;
  uint64_t baseline_raw;
  uint64_t last_monotonic_ms;
  smart_band_step_source_t baseline_source;
  bool initialized;
  bool has_baseline;
  bool has_timestamp;
} smart_band_step_normalizer_t;

smart_band_step_result_t smart_band_step_normalizer_init(
  smart_band_step_normalizer_t *normalizer,
  const smart_band_step_normalizer_config_t *config);
smart_band_step_result_t smart_band_step_normalizer_push(
  smart_band_step_normalizer_t *normalizer,
  const smart_band_step_sample_t *sample,
  smart_band_step_output_t *output);

#endif
