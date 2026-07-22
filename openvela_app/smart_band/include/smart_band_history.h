#ifndef SMART_BAND_HISTORY_H
#define SMART_BAND_HISTORY_H

#include "smart_band_store.h"
#include "smart_band_workout_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_HISTORY_DAILY_CAPACITY 30u
#define SMART_BAND_HISTORY_SESSION_CAPACITY 30u
#define SMART_BAND_HISTORY_DAILY_SHARDS 2u
#define SMART_BAND_HISTORY_SESSION_SHARDS 3u

#define SMART_BAND_HISTORY_SOURCE_SENSOR     (1u << 0)
#define SMART_BAND_HISTORY_SOURCE_DERIVED    (1u << 1)
#define SMART_BAND_HISTORY_SOURCE_SIMULATION (1u << 2)

#define SMART_BAND_HISTORY_DAY_COMPLETE       (1u << 0)
#define SMART_BAND_HISTORY_DAY_HEART_VALID    (1u << 1)
#define SMART_BAND_HISTORY_DAY_CLOCK_ROLLBACK (1u << 2)
#define SMART_BAND_HISTORY_DAY_RECOVERY_GAP   (1u << 3)
#define SMART_BAND_HISTORY_DAY_OVERFLOW       (1u << 4)

#define SMART_BAND_HISTORY_SESSION_RTC_VALID  (1u << 0)
#define SMART_BAND_HISTORY_SESSION_RECOVERED  (1u << 1)
#define SMART_BAND_HISTORY_SESSION_COMPLETE   (1u << 2)

typedef enum
{
  SMART_BAND_HISTORY_STATUS_FINISHED = 0,
  SMART_BAND_HISTORY_STATUS_ABORTED
} smart_band_history_session_status_t;

typedef struct
{
  int32_t day_key;
  uint32_t steps;
  uint32_t active_seconds;
  uint32_t calories_milli_kcal;
  uint32_t heart_weighted_bpm_seconds;
  uint32_t heart_duration_seconds;
  uint8_t heart_min_bpm;
  uint8_t heart_max_bpm;
  uint8_t source_flags;
  uint8_t flags;
} smart_band_daily_summary_t;

typedef struct
{
  uint32_t id;
  int64_t start_wall_time;
  int64_t end_wall_time;
  uint64_t active_duration_ms;
  uint32_t steps;
  uint32_t distance_mm;
  uint32_t calories_milli_kcal;
  uint8_t heart_current_bpm;
  uint8_t heart_min_bpm;
  uint8_t heart_max_bpm;
  uint8_t heart_average_bpm;
  uint16_t pause_count;
  uint8_t mode;
  uint8_t status;
  uint8_t source_flags;
  uint8_t flags;
} smart_band_workout_session_t;

typedef struct
{
  smart_band_daily_summary_t daily[SMART_BAND_HISTORY_DAILY_CAPACITY];
  smart_band_workout_session_t
    sessions[SMART_BAND_HISTORY_SESSION_CAPACITY];
  smart_band_store_t *store;
  size_t daily_count;
  size_t session_count;
  uint32_t next_session_id;
  smart_band_store_result_t last_daily_result;
  smart_band_store_result_t last_session_result;
  smart_band_store_result_t last_transaction_result;
  bool initialized;
  bool daily_writes_blocked;
  bool session_writes_blocked;
  bool storage_reload_pending;
  uint8_t daily_dirty_shards;
} smart_band_history_t;

int smart_band_history_init(smart_band_history_t *history,
                             smart_band_store_t *store);
void smart_band_history_reset(smart_band_history_t *history);
smart_band_store_result_t smart_band_history_recover_storage(
  smart_band_history_t *history);
bool smart_band_history_day_key(time_t wall_time, int32_t *day_key);
bool smart_band_history_format_day(int32_t day_key, char *buffer,
                                   size_t size);
bool smart_band_history_add_daily(smart_band_history_t *history,
                                  int32_t day_key, uint32_t step_delta,
                                  uint32_t active_seconds,
                                  uint32_t calories_milli_kcal,
                                  uint16_t heart_rate_bpm,
                                  bool heart_rate_valid,
                                  uint32_t heart_sample_seconds,
                                  uint8_t source_flags,
                                  uint8_t additional_flags);
smart_band_store_result_t
smart_band_history_flush_daily(smart_band_history_t *history);
smart_band_store_result_t smart_band_history_append_session(
  smart_band_history_t *history,
  const smart_band_workout_session_t *session);
smart_band_store_result_t smart_band_history_commit_workout(
  smart_band_history_t *history,
  const smart_band_store_record_spec_t *checkpoint_spec,
  const void *checkpoint_payload, size_t checkpoint_payload_size,
  const smart_band_workout_session_t *session);
size_t smart_band_history_latest_days(const smart_band_history_t *history,
                                      smart_band_daily_summary_t *output,
                                      size_t capacity);
size_t smart_band_history_latest_sessions(
  const smart_band_history_t *history,
  smart_band_workout_session_t *output, size_t capacity);
uint32_t smart_band_history_daily_average_heart_rate(
  const smart_band_daily_summary_t *summary);
bool smart_band_history_session_equal(
  const smart_band_workout_session_t *left,
  const smart_band_workout_session_t *right);

#ifdef __cplusplus
}
#endif

#endif
