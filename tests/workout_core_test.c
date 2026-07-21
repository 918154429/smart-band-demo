#include "smart_band_step_normalizer.h"
#include "smart_band_workout_model.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

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

static smart_band_step_normalizer_config_t step_config(uint8_t bits)
{
  smart_band_step_normalizer_config_t config;

  config.counter_bits = bits;
  config.max_forward_delta = 100u;
  config.wrap_high_threshold = bits == 32u ? UINT32_MAX - 100u
                                            : UINT64_MAX - 100u;
  config.wrap_low_threshold = 100u;
  config.max_sample_gap_ms = 1000u;
  return config;
}

static smart_band_step_result_t push_step(
  smart_band_step_normalizer_t *normalizer,
  smart_band_step_source_t source, uint64_t raw, bool available, bool fresh,
  uint64_t now, smart_band_step_output_t *output)
{
  smart_band_step_sample_t sample;

  sample.source = source;
  sample.raw_counter = raw;
  sample.available = available;
  sample.fresh = fresh;
  sample.monotonic_ms = now;
  return smart_band_step_normalizer_push(normalizer, &sample, output);
}

static int test_step_normalizer_validation(void)
{
  smart_band_step_normalizer_config_t config = step_config(32u);
  smart_band_step_normalizer_t normalizer;
  smart_band_step_normalizer_t snapshot;
  smart_band_step_output_t output;
  smart_band_step_sample_t sample = {
    SMART_BAND_STEP_SOURCE_SENSOR, 1u, true, true, 0u
  };

  CHECK(smart_band_step_normalizer_init(NULL, &config) ==
        SMART_BAND_STEP_RESULT_INVALID_ARGUMENT);
  CHECK(smart_band_step_normalizer_init(&normalizer, NULL) ==
        SMART_BAND_STEP_RESULT_INVALID_ARGUMENT);
  config.counter_bits = 31u;
  CHECK(smart_band_step_normalizer_init(&normalizer, &config) ==
        SMART_BAND_STEP_RESULT_INVALID_ARGUMENT);
  config = step_config(32u);
  config.max_forward_delta = 0u;
  CHECK(smart_band_step_normalizer_init(&normalizer, &config) ==
        SMART_BAND_STEP_RESULT_INVALID_ARGUMENT);
  config = step_config(32u);
  config.max_sample_gap_ms = 0u;
  CHECK(smart_band_step_normalizer_init(&normalizer, &config) ==
        SMART_BAND_STEP_RESULT_INVALID_ARGUMENT);
  config = step_config(32u);
  config.wrap_high_threshold = config.wrap_low_threshold;
  CHECK(smart_band_step_normalizer_init(&normalizer, &config) ==
        SMART_BAND_STEP_RESULT_INVALID_ARGUMENT);

  config = step_config(32u);
  CHECK(smart_band_step_normalizer_init(&normalizer, &config) ==
        SMART_BAND_STEP_RESULT_OK);
  CHECK(smart_band_step_normalizer_push(NULL, &sample, &output) ==
        SMART_BAND_STEP_RESULT_INVALID_ARGUMENT);
  CHECK(smart_band_step_normalizer_push(&normalizer, NULL, &output) ==
        SMART_BAND_STEP_RESULT_INVALID_ARGUMENT);
  CHECK(smart_band_step_normalizer_push(&normalizer, &sample, NULL) ==
        SMART_BAND_STEP_RESULT_INVALID_ARGUMENT);
  memset(&snapshot, 0, sizeof(snapshot));
  CHECK(smart_band_step_normalizer_push(&snapshot, &sample, &output) ==
        SMART_BAND_STEP_RESULT_INVALID_ARGUMENT);

  snapshot = normalizer;
  sample.source = (smart_band_step_source_t)99;
  CHECK(smart_band_step_normalizer_push(&normalizer, &sample, &output) ==
        SMART_BAND_STEP_RESULT_INVALID_SAMPLE);
  CHECK(memcmp(&normalizer, &snapshot, sizeof(normalizer)) == 0);
  sample.source = SMART_BAND_STEP_SOURCE_SENSOR;
  sample.raw_counter = (uint64_t)UINT32_MAX + 1u;
  CHECK(smart_band_step_normalizer_push(&normalizer, &sample, &output) ==
        SMART_BAND_STEP_RESULT_INVALID_SAMPLE);
  CHECK(memcmp(&normalizer, &snapshot, sizeof(normalizer)) == 0);
  return 0;
}

static int test_step_normalizer_paths(void)
{
  smart_band_step_normalizer_config_t config = step_config(32u);
  smart_band_step_normalizer_t normalizer;
  smart_band_step_normalizer_t snapshot;
  smart_band_step_output_t output;

  CHECK(smart_band_step_normalizer_init(&normalizer, &config) ==
        SMART_BAND_STEP_RESULT_OK);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                  true, 10u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_BASELINE && output.delta == 0u);
  CHECK((output.quality_flags & SMART_BAND_STEP_QUALITY_REBASED) != 0u);

  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 125u, true,
                  true, 20u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_DELTA && output.delta == 25u &&
        output.total == 25u);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 125u, true,
                  true, 20u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.delta == 0u && output.total == 25u);

  snapshot = normalizer;
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 126u, true,
                  true, 19u, &output) ==
        SMART_BAND_STEP_RESULT_TIME_REGRESSION);
  CHECK(memcmp(&normalizer, &snapshot, sizeof(normalizer)) == 0);

  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_DERIVED, 500u, true,
                  true, 30u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_SOURCE_SWITCH &&
        output.total == 25u);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SIMULATION, 5u, true,
                  true, 40u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_SOURCE_SWITCH);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 200u, true,
                  true, 50u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_SOURCE_SWITCH);

  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 205u, true,
                  false, 60u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_STALE);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 220u, true,
                  true, 70u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_BASELINE);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 225u, false,
                  true, 80u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_UNAVAILABLE);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 230u, true,
                  true, 90u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_BASELINE);

  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 240u, true,
                  true, 2000u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_GAP && output.total == 25u);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 400u, true,
                  true, 2010u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_FORWARD_JUMP);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 10u, true,
                  true, 2020u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_RESET);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 15u, true,
                  true, 2030u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.delta == 5u && output.total == 30u);
  return 0;
}

static int test_step_normalizer_wrap_and_overflow(void)
{
  smart_band_step_normalizer_config_t config = step_config(32u);
  smart_band_step_normalizer_t normalizer;
  smart_band_step_normalizer_t snapshot;
  smart_band_step_output_t output;

  CHECK(smart_band_step_normalizer_init(&normalizer, &config) ==
        SMART_BAND_STEP_RESULT_OK);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR,
                  UINT32_MAX - 2u, true, true, 0u, &output) ==
        SMART_BAND_STEP_RESULT_OK);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 2u, true,
                  true, 1u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_WRAP && output.delta == 5u);

  config = step_config(64u);
  CHECK(smart_band_step_normalizer_init(&normalizer, &config) ==
        SMART_BAND_STEP_RESULT_OK);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR,
                  UINT64_MAX - 2u, true, true, 0u, &output) ==
        SMART_BAND_STEP_RESULT_OK);
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 1u, true,
                  true, 1u, &output) == SMART_BAND_STEP_RESULT_OK);
  CHECK(output.reason == SMART_BAND_STEP_REASON_WRAP && output.delta == 4u);

  normalizer.total = UINT64_MAX;
  normalizer.baseline_raw = 1u;
  normalizer.last_monotonic_ms = 1u;
  normalizer.has_baseline = true;
  snapshot = normalizer;
  CHECK(push_step(&normalizer, SMART_BAND_STEP_SOURCE_SENSOR, 2u, true,
                  true, 2u, &output) == SMART_BAND_STEP_RESULT_OVERFLOW);
  CHECK(memcmp(&normalizer, &snapshot, sizeof(normalizer)) == 0);
  return 0;
}

static smart_band_workout_config_t workout_config(void)
{
  smart_band_workout_config_t config;

  memset(&config, 0, sizeof(config));
  config.countdown_ms = 3000u;
  config.modes[SMART_BAND_WORKOUT_MODE_WALK].stride_mm = 800u;
  config.modes[SMART_BAND_WORKOUT_MODE_WALK]
    .calories_milli_kcal_per_step = 40u;
  config.modes[SMART_BAND_WORKOUT_MODE_WALK].max_step_delta = 100000u;
  config.modes[SMART_BAND_WORKOUT_MODE_WALK].minimum_heart_rate_bpm = 30u;
  config.modes[SMART_BAND_WORKOUT_MODE_WALK].maximum_heart_rate_bpm = 240u;
  config.modes[SMART_BAND_WORKOUT_MODE_RUN].stride_mm = 1200u;
  config.modes[SMART_BAND_WORKOUT_MODE_RUN]
    .calories_milli_kcal_per_step = 80u;
  config.modes[SMART_BAND_WORKOUT_MODE_RUN].max_step_delta = 200000u;
  config.modes[SMART_BAND_WORKOUT_MODE_RUN].minimum_heart_rate_bpm = 40u;
  config.modes[SMART_BAND_WORKOUT_MODE_RUN].maximum_heart_rate_bpm = 250u;
  return config;
}

static smart_band_workout_result_t workout_update(
  smart_band_workout_model_t *model, uint64_t now, uint32_t steps,
  bool heart_valid, uint16_t heart_rate)
{
  smart_band_workout_sample_t sample;

  sample.monotonic_ms = now;
  sample.step_delta = steps;
  sample.heart_rate_valid = heart_valid;
  sample.heart_rate_bpm = heart_rate;
  return smart_band_workout_model_update(model, &sample);
}

static int test_workout_validation(void)
{
  smart_band_workout_config_t config = workout_config();
  smart_band_workout_model_t model;
  smart_band_workout_model_t snapshot;
  smart_band_workout_snapshot_t data;
  smart_band_workout_sample_t sample = { 0u, 0u, 0u, false };

  CHECK(smart_band_workout_model_init(NULL, &config,
        SMART_BAND_WORKOUT_MODE_WALK, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT);
  CHECK(smart_band_workout_model_init(&model, NULL,
        SMART_BAND_WORKOUT_MODE_WALK, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG);
  config.countdown_ms = 0u;
  CHECK(smart_band_workout_model_init(&model, &config,
        SMART_BAND_WORKOUT_MODE_WALK, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG);
  config = workout_config();
  config.modes[0].stride_mm = 0u;
  CHECK(smart_band_workout_model_init(&model, &config,
        SMART_BAND_WORKOUT_MODE_WALK, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG);
  config = workout_config();
  config.modes[1].maximum_heart_rate_bpm = 1u;
  CHECK(smart_band_workout_model_init(&model, &config,
        SMART_BAND_WORKOUT_MODE_RUN, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG);
  config = workout_config();
  CHECK(smart_band_workout_model_init(&model, &config,
        (smart_band_workout_mode_t)99, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG);
  CHECK(smart_band_workout_model_init(&model, &config,
        SMART_BAND_WORKOUT_MODE_WALK, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  snapshot = model;
  CHECK(smart_band_workout_model_command(&model,
        (smart_band_workout_command_t)99, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT);
  CHECK(memcmp(&model, &snapshot, sizeof(model)) == 0);
  CHECK(smart_band_workout_model_command(NULL,
        SMART_BAND_WORKOUT_COMMAND_START, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT);
  memset(&snapshot, 0, sizeof(snapshot));
  CHECK(smart_band_workout_model_command(&snapshot,
        SMART_BAND_WORKOUT_COMMAND_START, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT);
  CHECK(smart_band_workout_model_update(NULL, &sample) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT);
  CHECK(smart_band_workout_model_update(&model, NULL) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT);
  CHECK(smart_band_workout_model_update(&model, &sample) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_STATE);
  CHECK(smart_band_workout_model_snapshot(NULL, &data) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT);
  CHECK(smart_band_workout_model_snapshot(&model, NULL) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT);
  CHECK(smart_band_workout_model_restore(NULL, &config, &data, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT);
  CHECK(smart_band_workout_average_heart_rate_bpm(NULL) == 0u);
  CHECK(smart_band_workout_pace_ms_per_km(NULL) == 0u);
  return 0;
}

static int test_workout_full_path(void)
{
  smart_band_workout_config_t config = workout_config();
  smart_band_workout_model_t model;
  smart_band_workout_model_t before;
  smart_band_workout_snapshot_t data;

  CHECK(smart_band_workout_model_init(&model, &config,
        SMART_BAND_WORKOUT_MODE_WALK, 100u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_START, 100u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(model.data.state == SMART_BAND_WORKOUT_STATE_COUNTDOWN);
  before = model;
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_START, 101u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_STATE);
  CHECK(memcmp(&model, &before, sizeof(model)) == 0);

  CHECK(workout_update(&model, 1100u, 99u, true, 90u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(model.data.state == SMART_BAND_WORKOUT_STATE_COUNTDOWN &&
        model.data.steps == 0u && model.data.active_duration_ms == 0u);
  CHECK(workout_update(&model, 3100u, 10u, true, 100u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(model.data.state == SMART_BAND_WORKOUT_STATE_ACTIVE &&
        model.data.steps == 10u && model.data.distance_mm == 8000u &&
        model.data.calories_milli_kcal == 400u);
  CHECK(workout_update(&model, 4100u, 5u, true, 120u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(model.data.active_duration_ms == 1000u);

  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_PAUSE, 5100u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(model.data.active_duration_ms == 2000u &&
        model.data.state == SMART_BAND_WORKOUT_STATE_PAUSED);
  CHECK(workout_update(&model, 8100u, 50u, true, 200u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(model.data.steps == 15u && model.data.active_duration_ms == 2000u);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_RESUME, 9100u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(workout_update(&model, 10100u, 2u, true, 140u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_FINISH, 11100u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_snapshot(&model, &data) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(data.state == SMART_BAND_WORKOUT_STATE_FINISHED && data.steps == 17u);
  CHECK(data.distance_mm == 13600u && data.calories_milli_kcal == 680u);
  CHECK(data.active_duration_ms == 4000u);
  CHECK(data.heart_rate_aggregate_valid && data.heart_rate_min_bpm == 100u &&
        data.heart_rate_max_bpm == 140u);
  CHECK(smart_band_workout_average_heart_rate_bpm(&data) == 120u);
  CHECK(smart_band_workout_pace_ms_per_km(&data) == 294117u);

  before = model;
  CHECK(workout_update(&model, 12000u, 1u, true, 100u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_STATE);
  CHECK(memcmp(&model, &before, sizeof(model)) == 0);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_RESUME, 12000u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_STATE);
  CHECK(memcmp(&model, &before, sizeof(model)) == 0);
  return 0;
}

static int test_workout_recovery_abort_and_modes(void)
{
  smart_band_workout_config_t config = workout_config();
  smart_band_workout_model_t model;
  smart_band_workout_model_t restored;
  smart_band_workout_model_t before;
  smart_band_workout_snapshot_t data;

  CHECK(smart_band_workout_model_init(&model, &config,
        SMART_BAND_WORKOUT_MODE_RUN, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_START, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(workout_update(&model, 3000u, 10u, false, 0u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(model.data.distance_mm == 12000u &&
        model.data.calories_milli_kcal == 800u);
  CHECK(!model.data.heart_rate_aggregate_valid);
  CHECK(smart_band_workout_average_heart_rate_bpm(&model.data) == 0u);
  CHECK(smart_band_workout_model_snapshot(&model, &data) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_restore(&restored, &config, &data, 5000u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(restored.data.state ==
        SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION);
  before = restored;
  CHECK(workout_update(&restored, 6000u, 1u, true, 100u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_STATE);
  CHECK(memcmp(&restored, &before, sizeof(restored)) == 0);
  CHECK(smart_band_workout_model_command(&restored,
        SMART_BAND_WORKOUT_COMMAND_RESUME, 6000u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_STATE);
  CHECK(memcmp(&restored, &before, sizeof(restored)) == 0);
  CHECK(smart_band_workout_model_command(&restored,
        SMART_BAND_WORKOUT_COMMAND_CONFIRM_RECOVERY, 6000u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(restored.data.state == SMART_BAND_WORKOUT_STATE_PAUSED);
  CHECK(smart_band_workout_model_command(&restored,
        SMART_BAND_WORKOUT_COMMAND_RESUME, 7000u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(workout_update(&restored, 8000u, 1u, true, 150u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(restored.data.steps == 11u);
  CHECK(smart_band_workout_model_command(&restored,
        SMART_BAND_WORKOUT_COMMAND_ABORT, 9000u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(restored.data.state == SMART_BAND_WORKOUT_STATE_ABORTED);

  CHECK(smart_band_workout_model_init(&model, &config,
        SMART_BAND_WORKOUT_MODE_WALK, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_START, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_ABORT, 1000u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(model.data.state == SMART_BAND_WORKOUT_STATE_ABORTED);
  return 0;
}

static int test_workout_boundaries(void)
{
  smart_band_workout_config_t config = workout_config();
  smart_band_workout_model_t model;
  smart_band_workout_model_t before;
  smart_band_workout_snapshot_t data;

  CHECK(smart_band_workout_model_init(&model, &config,
        SMART_BAND_WORKOUT_MODE_WALK, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_START, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(workout_update(&model, 3000u, 0u, true, 80u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(workout_update(&model, 86403000u, 100000u, false, 0u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(model.data.active_duration_ms == 86400000u &&
        model.data.steps == 100000u);

  before = model;
  CHECK(workout_update(&model, 86402999u, 0u, false, 0u) ==
        SMART_BAND_WORKOUT_RESULT_TIME_REGRESSION);
  CHECK(memcmp(&model, &before, sizeof(model)) == 0);
  CHECK(workout_update(&model, 86404000u, 100001u, false, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_SAMPLE);
  CHECK(memcmp(&model, &before, sizeof(model)) == 0);
  CHECK(workout_update(&model, 86404000u, 0u, true, 10u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_SAMPLE);
  CHECK(memcmp(&model, &before, sizeof(model)) == 0);

  model.data.steps = UINT64_MAX;
  before = model;
  CHECK(workout_update(&model, 86404000u, 1u, false, 0u) ==
        SMART_BAND_WORKOUT_RESULT_OVERFLOW);
  CHECK(memcmp(&model, &before, sizeof(model)) == 0);

  memset(&data, 0, sizeof(data));
  data.mode = SMART_BAND_WORKOUT_MODE_WALK;
  data.state = SMART_BAND_WORKOUT_STATE_FINISHED;
  CHECK(smart_band_workout_model_restore(&model, &config, &data, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG);
  data.state = SMART_BAND_WORKOUT_STATE_ACTIVE;
  data.heart_rate_aggregate_valid = true;
  CHECK(smart_band_workout_model_restore(&model, &config, &data, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG);
  data.heart_rate_aggregate_valid = false;
  data.countdown_elapsed_ms = 3001u;
  CHECK(smart_band_workout_model_restore(&model, &config, &data, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG);

  memset(&data, 0, sizeof(data));
  data.mode = SMART_BAND_WORKOUT_MODE_WALK;
  data.state = SMART_BAND_WORKOUT_STATE_ACTIVE;
  data.heart_rate_aggregate_valid = true;
  data.heart_rate_min_bpm = 20u;
  data.heart_rate_max_bpm = 100u;
  data.heart_rate_weighted_bpm_ms = 100u;
  data.heart_rate_weighted_duration_ms = 1u;
  CHECK(smart_band_workout_model_restore(&model, &config, &data, 0u) ==
        SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG);

  memset(&data, 0, sizeof(data));
  data.active_duration_ms = UINT64_MAX;
  data.distance_mm = 1u;
  CHECK(smart_band_workout_pace_ms_per_km(&data) == 0u);
  return 0;
}

static int test_workout_terminal_and_overflow_paths(void)
{
  smart_band_workout_config_t config = workout_config();
  smart_band_workout_model_t model;
  smart_band_workout_model_t restored;
  smart_band_workout_model_t before;
  smart_band_workout_snapshot_t data;

  CHECK(smart_band_workout_model_init(&model, &config,
        SMART_BAND_WORKOUT_MODE_WALK, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_START, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(workout_update(&model, 3000u, 1u, true, 100u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(workout_update(&model, 4000u, 0u, true, 80u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(model.data.heart_rate_min_bpm == 80u);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_PAUSE, 5000u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_FINISH, 6000u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(model.data.state == SMART_BAND_WORKOUT_STATE_FINISHED);

  CHECK(smart_band_workout_model_init(&model, &config,
        SMART_BAND_WORKOUT_MODE_WALK, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_START, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(workout_update(&model, 3000u, 1u, true, 100u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_snapshot(&model, &data) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_restore(&restored, &config, &data, 4000u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_command(&restored,
        SMART_BAND_WORKOUT_COMMAND_ABORT, 5000u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(restored.data.state == SMART_BAND_WORKOUT_STATE_ABORTED);

  model.data.active_duration_ms = UINT64_MAX;
  before = model;
  CHECK(workout_update(&model, 4000u, 0u, false, 0u) ==
        SMART_BAND_WORKOUT_RESULT_OVERFLOW);
  CHECK(memcmp(&model, &before, sizeof(model)) == 0);

  CHECK(smart_band_workout_model_init(&model, &config,
        SMART_BAND_WORKOUT_MODE_WALK, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_START, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(workout_update(&model, 3000u, 0u, true, 240u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  model.data.heart_rate_weighted_bpm_ms = UINT64_MAX;
  before = model;
  CHECK(workout_update(&model, 3001u, 0u, true, 240u) ==
        SMART_BAND_WORKOUT_RESULT_OVERFLOW);
  CHECK(memcmp(&model, &before, sizeof(model)) == 0);

  model = before;
  model.data.heart_rate_weighted_bpm_ms = 0u;
  model.data.heart_rate_weighted_duration_ms = UINT64_MAX;
  before = model;
  CHECK(workout_update(&model, 3001u, 0u, true, 240u) ==
        SMART_BAND_WORKOUT_RESULT_OVERFLOW);
  CHECK(memcmp(&model, &before, sizeof(model)) == 0);

  CHECK(smart_band_workout_model_init(&model, &config,
        SMART_BAND_WORKOUT_MODE_WALK, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(smart_band_workout_model_command(&model,
        SMART_BAND_WORKOUT_COMMAND_START, 0u) == SMART_BAND_WORKOUT_RESULT_OK);
  CHECK(workout_update(&model, 3000u, 0u, false, 0u) ==
        SMART_BAND_WORKOUT_RESULT_OK);
  model.data.distance_mm = UINT64_MAX;
  before = model;
  CHECK(workout_update(&model, 3001u, 1u, false, 0u) ==
        SMART_BAND_WORKOUT_RESULT_OVERFLOW);
  CHECK(memcmp(&model, &before, sizeof(model)) == 0);
  return 0;
}

int main(void)
{
  CHECK(test_step_normalizer_validation() == 0);
  CHECK(test_step_normalizer_paths() == 0);
  CHECK(test_step_normalizer_wrap_and_overflow() == 0);
  CHECK(test_workout_validation() == 0);
  CHECK(test_workout_full_path() == 0);
  CHECK(test_workout_recovery_abort_and_modes() == 0);
  CHECK(test_workout_boundaries() == 0);
  CHECK(test_workout_terminal_and_overflow_paths() == 0);
  puts("smart band workout core tests passed");
  return 0;
}
