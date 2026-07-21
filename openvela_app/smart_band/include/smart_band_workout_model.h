#ifndef SMART_BAND_WORKOUT_MODEL_H
#define SMART_BAND_WORKOUT_MODEL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  SMART_BAND_WORKOUT_MODE_WALK = 0,
  SMART_BAND_WORKOUT_MODE_RUN,
  SMART_BAND_WORKOUT_MODE_COUNT
} smart_band_workout_mode_t;

typedef enum
{
  SMART_BAND_WORKOUT_STATE_IDLE = 0,
  SMART_BAND_WORKOUT_STATE_COUNTDOWN,
  SMART_BAND_WORKOUT_STATE_ACTIVE,
  SMART_BAND_WORKOUT_STATE_PAUSED,
  SMART_BAND_WORKOUT_STATE_FINISHED,
  SMART_BAND_WORKOUT_STATE_ABORTED,
  SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION
} smart_band_workout_state_t;

typedef enum
{
  SMART_BAND_WORKOUT_COMMAND_START = 0,
  SMART_BAND_WORKOUT_COMMAND_PAUSE,
  SMART_BAND_WORKOUT_COMMAND_RESUME,
  SMART_BAND_WORKOUT_COMMAND_FINISH,
  SMART_BAND_WORKOUT_COMMAND_ABORT,
  SMART_BAND_WORKOUT_COMMAND_CONFIRM_RECOVERY
} smart_band_workout_command_t;

typedef enum
{
  SMART_BAND_WORKOUT_RESULT_OK = 0,
  SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT,
  SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG,
  SMART_BAND_WORKOUT_RESULT_INVALID_STATE,
  SMART_BAND_WORKOUT_RESULT_INVALID_SAMPLE,
  SMART_BAND_WORKOUT_RESULT_TIME_REGRESSION,
  SMART_BAND_WORKOUT_RESULT_OVERFLOW
} smart_band_workout_result_t;

typedef struct
{
  uint32_t stride_mm;
  uint32_t calories_milli_kcal_per_step;
  uint32_t max_step_delta;
  uint16_t minimum_heart_rate_bpm;
  uint16_t maximum_heart_rate_bpm;
} smart_band_workout_mode_config_t;

typedef struct
{
  uint64_t countdown_ms;
  smart_band_workout_mode_config_t modes[SMART_BAND_WORKOUT_MODE_COUNT];
} smart_band_workout_config_t;

typedef struct
{
  uint64_t monotonic_ms;
  uint32_t step_delta;
  uint16_t heart_rate_bpm;
  bool heart_rate_valid;
} smart_band_workout_sample_t;

typedef struct
{
  smart_band_workout_mode_t mode;
  smart_band_workout_state_t state;
  uint64_t countdown_elapsed_ms;
  uint64_t active_duration_ms;
  uint64_t steps;
  uint64_t distance_mm;
  uint64_t calories_milli_kcal;
  uint16_t heart_rate_current_bpm;
  uint16_t heart_rate_min_bpm;
  uint16_t heart_rate_max_bpm;
  bool heart_rate_current_valid;
  bool heart_rate_aggregate_valid;
  uint64_t heart_rate_weighted_bpm_ms;
  uint64_t heart_rate_weighted_duration_ms;
} smart_band_workout_snapshot_t;

typedef struct
{
  smart_band_workout_config_t config;
  smart_band_workout_snapshot_t data;
  uint64_t last_monotonic_ms;
  bool initialized;
} smart_band_workout_model_t;

smart_band_workout_result_t smart_band_workout_model_init(
  smart_band_workout_model_t *model,
  const smart_band_workout_config_t *config,
  smart_band_workout_mode_t mode,
  uint64_t monotonic_ms);
smart_band_workout_result_t smart_band_workout_model_command(
  smart_band_workout_model_t *model,
  smart_band_workout_command_t command,
  uint64_t monotonic_ms);
smart_band_workout_result_t smart_band_workout_model_update(
  smart_band_workout_model_t *model,
  const smart_band_workout_sample_t *sample);
smart_band_workout_result_t smart_band_workout_model_snapshot(
  const smart_band_workout_model_t *model,
  smart_band_workout_snapshot_t *snapshot);
smart_band_workout_result_t smart_band_workout_model_restore(
  smart_band_workout_model_t *model,
  const smart_band_workout_config_t *config,
  const smart_band_workout_snapshot_t *snapshot,
  uint64_t monotonic_ms);
uint64_t smart_band_workout_average_heart_rate_bpm(
  const smart_band_workout_snapshot_t *snapshot);
uint64_t smart_band_workout_pace_ms_per_km(
  const smart_band_workout_snapshot_t *snapshot);

#endif
