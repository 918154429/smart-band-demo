#include "smart_band_step_normalizer.h"

#include <limits.h>
#include <string.h>

static uint64_t step_counter_max(uint8_t counter_bits)
{
  return counter_bits == 32u ? UINT32_MAX : UINT64_MAX;
}

static bool step_config_valid(
  const smart_band_step_normalizer_config_t *config)
{
  uint64_t counter_max;

  if (config == NULL ||
      (config->counter_bits != 32u && config->counter_bits != 64u) ||
      config->max_forward_delta == 0u || config->max_sample_gap_ms == 0u)
    {
      return false;
    }

  counter_max = step_counter_max(config->counter_bits);
  return config->max_forward_delta <= counter_max &&
         config->wrap_high_threshold <= counter_max &&
         config->wrap_low_threshold <= counter_max &&
         config->wrap_high_threshold > config->wrap_low_threshold;
}

static void step_output_set(smart_band_step_output_t *output,
                            uint64_t total,
                            smart_band_step_reason_t reason,
                            uint32_t quality_flags)
{
  output->delta = 0u;
  output->total = total;
  output->reason = reason;
  output->quality_flags = quality_flags;
}

static uint64_t step_wrap_delta(uint64_t previous, uint64_t current,
                                uint64_t counter_max)
{
  return (counter_max - previous) + 1u + current;
}

smart_band_step_result_t smart_band_step_normalizer_init(
  smart_band_step_normalizer_t *normalizer,
  const smart_band_step_normalizer_config_t *config)
{
  if (normalizer == NULL || !step_config_valid(config))
    {
      return SMART_BAND_STEP_RESULT_INVALID_ARGUMENT;
    }

  (void)memset(normalizer, 0, sizeof(*normalizer));
  normalizer->config = *config;
  normalizer->initialized = true;
  return SMART_BAND_STEP_RESULT_OK;
}

smart_band_step_result_t smart_band_step_normalizer_push(
  smart_band_step_normalizer_t *normalizer,
  const smart_band_step_sample_t *sample,
  smart_band_step_output_t *output)
{
  smart_band_step_normalizer_t next;
  uint64_t counter_max;
  uint64_t delta;
  uint64_t previous_monotonic_ms;

  if (normalizer == NULL || sample == NULL || output == NULL ||
      !normalizer->initialized)
    {
      return SMART_BAND_STEP_RESULT_INVALID_ARGUMENT;
    }

  step_output_set(output, normalizer->total, SMART_BAND_STEP_REASON_NONE, 0u);
  counter_max = step_counter_max(normalizer->config.counter_bits);
  if (sample->source < SMART_BAND_STEP_SOURCE_SENSOR ||
      sample->source >= SMART_BAND_STEP_SOURCE_COUNT ||
      sample->raw_counter > counter_max)
    {
      return SMART_BAND_STEP_RESULT_INVALID_SAMPLE;
    }

  if (normalizer->has_timestamp &&
      sample->monotonic_ms < normalizer->last_monotonic_ms)
    {
      return SMART_BAND_STEP_RESULT_TIME_REGRESSION;
    }

  next = *normalizer;
  previous_monotonic_ms = next.last_monotonic_ms;
  next.has_timestamp = true;
  next.last_monotonic_ms = sample->monotonic_ms;

  if (!sample->available || !sample->fresh)
    {
      next.has_baseline = false;
      step_output_set(output, next.total,
                      sample->available ? SMART_BAND_STEP_REASON_STALE
                                        : SMART_BAND_STEP_REASON_UNAVAILABLE,
                      SMART_BAND_STEP_QUALITY_REBASED |
                        SMART_BAND_STEP_QUALITY_DISCONTINUITY |
                        (sample->available ? SMART_BAND_STEP_QUALITY_STALE
                                           : SMART_BAND_STEP_QUALITY_UNAVAILABLE));
      *normalizer = next;
      return SMART_BAND_STEP_RESULT_OK;
    }

  if (!next.has_baseline)
    {
      next.has_baseline = true;
      next.baseline_raw = sample->raw_counter;
      next.baseline_source = sample->source;
      step_output_set(output, next.total, SMART_BAND_STEP_REASON_BASELINE,
                      SMART_BAND_STEP_QUALITY_ACCEPTED |
                        SMART_BAND_STEP_QUALITY_REBASED);
      *normalizer = next;
      return SMART_BAND_STEP_RESULT_OK;
    }

  if (sample->source != next.baseline_source)
    {
      next.baseline_raw = sample->raw_counter;
      next.baseline_source = sample->source;
      step_output_set(output, next.total, SMART_BAND_STEP_REASON_SOURCE_SWITCH,
                      SMART_BAND_STEP_QUALITY_ACCEPTED |
                        SMART_BAND_STEP_QUALITY_REBASED |
                        SMART_BAND_STEP_QUALITY_DISCONTINUITY |
                        SMART_BAND_STEP_QUALITY_SOURCE_CHANGED);
      *normalizer = next;
      return SMART_BAND_STEP_RESULT_OK;
    }

  if (sample->monotonic_ms - previous_monotonic_ms >
      next.config.max_sample_gap_ms)
    {
      next.baseline_raw = sample->raw_counter;
      step_output_set(output, next.total, SMART_BAND_STEP_REASON_GAP,
                      SMART_BAND_STEP_QUALITY_ACCEPTED |
                        SMART_BAND_STEP_QUALITY_REBASED |
                        SMART_BAND_STEP_QUALITY_DISCONTINUITY);
      *normalizer = next;
      return SMART_BAND_STEP_RESULT_OK;
    }

  if (sample->raw_counter >= next.baseline_raw)
    {
      delta = sample->raw_counter - next.baseline_raw;
      if (delta > next.config.max_forward_delta)
        {
          next.baseline_raw = sample->raw_counter;
          step_output_set(output, next.total,
                          SMART_BAND_STEP_REASON_FORWARD_JUMP,
                          SMART_BAND_STEP_QUALITY_ACCEPTED |
                            SMART_BAND_STEP_QUALITY_REBASED |
                            SMART_BAND_STEP_QUALITY_DISCONTINUITY);
          *normalizer = next;
          return SMART_BAND_STEP_RESULT_OK;
        }
    }
  else if (next.baseline_raw >= next.config.wrap_high_threshold &&
           sample->raw_counter <= next.config.wrap_low_threshold &&
           (delta = step_wrap_delta(next.baseline_raw, sample->raw_counter,
                                    counter_max)) <=
             next.config.max_forward_delta)
    {
      if (delta > UINT64_MAX - next.total)
        {
          return SMART_BAND_STEP_RESULT_OVERFLOW;
        }

      next.total += delta;
      next.baseline_raw = sample->raw_counter;
      output->delta = delta;
      output->total = next.total;
      output->reason = SMART_BAND_STEP_REASON_WRAP;
      output->quality_flags = SMART_BAND_STEP_QUALITY_ACCEPTED |
                              SMART_BAND_STEP_QUALITY_WRAPPED;
      *normalizer = next;
      return SMART_BAND_STEP_RESULT_OK;
    }
  else
    {
      next.baseline_raw = sample->raw_counter;
      step_output_set(output, next.total, SMART_BAND_STEP_REASON_RESET,
                      SMART_BAND_STEP_QUALITY_ACCEPTED |
                        SMART_BAND_STEP_QUALITY_REBASED |
                        SMART_BAND_STEP_QUALITY_DISCONTINUITY);
      *normalizer = next;
      return SMART_BAND_STEP_RESULT_OK;
    }

  if (delta > UINT64_MAX - next.total)
    {
      return SMART_BAND_STEP_RESULT_OVERFLOW;
    }

  next.total += delta;
  next.baseline_raw = sample->raw_counter;
  output->delta = delta;
  output->total = next.total;
  output->reason = SMART_BAND_STEP_REASON_DELTA;
  output->quality_flags = SMART_BAND_STEP_QUALITY_ACCEPTED;
  *normalizer = next;
  return SMART_BAND_STEP_RESULT_OK;
}
