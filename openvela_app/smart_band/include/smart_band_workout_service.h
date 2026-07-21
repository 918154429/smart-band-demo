#ifndef SMART_BAND_WORKOUT_SERVICE_H
#define SMART_BAND_WORKOUT_SERVICE_H

#include "smart_band_history.h"
#include "smart_band_step_normalizer.h"
#include "smart_band_workout_model.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_WORKOUT_CHECKPOINT_SLOT_A UINT32_C(0x00010000)
#define SMART_BAND_WORKOUT_CHECKPOINT_SLOT_B UINT32_C(0x00010001)
#define SMART_BAND_WORKOUT_CHECKPOINT_INTERVAL_MS UINT64_C(30000)
#define SMART_BAND_HISTORY_FLUSH_INTERVAL_MS UINT64_C(300000)

typedef enum
{
  SMART_BAND_WORKOUT_SERVICE_OK = 0,
  SMART_BAND_WORKOUT_SERVICE_INVALID_ARGUMENT,
  SMART_BAND_WORKOUT_SERVICE_INVALID_STATE,
  SMART_BAND_WORKOUT_SERVICE_MODEL_ERROR,
  SMART_BAND_WORKOUT_SERVICE_STORAGE_ERROR,
  SMART_BAND_WORKOUT_SERVICE_RANGE_ERROR
} smart_band_workout_service_result_t;

typedef enum
{
  SMART_BAND_WORKOUT_SERVICE_PHASE_READY = 0,
  SMART_BAND_WORKOUT_SERVICE_PHASE_FINALIZING
} smart_band_workout_service_phase_t;

typedef struct
{
  smart_band_workout_model_t model;
  smart_band_step_normalizer_t step_normalizer;
  smart_band_history_t *history;
  smart_band_store_t *store;
  smart_band_workout_session_t pending_session;
  int64_t start_wall_time;
  int64_t last_wall_time;
  int32_t last_trusted_day;
  uint64_t last_checkpoint_ms;
  uint64_t last_checkpoint_attempt_ms;
  uint64_t last_daily_flush_ms;
  uint32_t daily_active_ms_remainder;
  uint32_t daily_heart_ms_remainder;
  uint32_t undated_steps;
  uint32_t undated_active_seconds;
  uint32_t undated_calories_milli_kcal;
  uint32_t pause_count;
  uint8_t source_flags;
  smart_band_workout_service_phase_t phase;
  smart_band_store_result_t checkpoint_result;
  bool initialized;
  bool recovery_pending;
  bool recovered_session;
  bool have_trusted_day;
  bool last_wall_valid;
  bool recovery_gap;
  bool clock_anomaly_seen;
} smart_band_workout_service_t;

int smart_band_workout_service_init(smart_band_workout_service_t *service,
                                    smart_band_store_t *store,
                                    smart_band_history_t *history,
                                    uint64_t monotonic_ms);
void smart_band_workout_service_reset(smart_band_workout_service_t *service);
smart_band_workout_service_result_t smart_band_workout_service_start(
  smart_band_workout_service_t *service, smart_band_workout_mode_t mode,
  uint64_t monotonic_ms, time_t wall_time, bool wall_valid);
smart_band_workout_service_result_t smart_band_workout_service_command(
  smart_band_workout_service_t *service,
  smart_band_workout_command_t command, uint64_t monotonic_ms,
  time_t wall_time, bool wall_valid, bool wall_rollback);
smart_band_workout_service_result_t smart_band_workout_service_tick(
  smart_band_workout_service_t *service,
  const smart_band_step_sample_t *step_sample,
  uint16_t heart_rate_bpm, bool heart_rate_valid, bool heart_rate_new,
  uint64_t monotonic_ms, time_t wall_time,
  bool wall_valid, bool wall_rollback);
bool smart_band_workout_service_snapshot(
  const smart_band_workout_service_t *service,
  smart_band_workout_snapshot_t *snapshot);
bool smart_band_workout_service_is_live(
  const smart_band_workout_service_t *service);
smart_band_store_result_t smart_band_workout_service_checkpoint(
  smart_band_workout_service_t *service, uint64_t monotonic_ms);
smart_band_workout_service_result_t
smart_band_workout_service_dismiss_summary(
  smart_band_workout_service_t *service, uint64_t monotonic_ms);

#ifdef __cplusplus
}
#endif

#endif
