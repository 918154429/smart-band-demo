#include "smart_band_workout_service.h"

#include <limits.h>
#include <string.h>

#define CHECKPOINT_MAGIC UINT32_C(0x31504357)
#define CHECKPOINT_HEADER_SIZE 8u
#define CHECKPOINT_LIVE_SIZE 112u
#define CHECKPOINT_RETRY_MS UINT64_C(5000)
#define CST_OFFSET_SECONDS INT64_C(28800)
#define SECONDS_PER_DAY INT64_C(86400)

typedef enum
{
  CHECKPOINT_KIND_EMPTY = 0,
  CHECKPOINT_KIND_LIVE
} checkpoint_kind_t;

static const smart_band_store_record_spec_t g_checkpoint_spec =
{
  {SMART_BAND_WORKOUT_CHECKPOINT_SLOT_A,
   SMART_BAND_WORKOUT_CHECKPOINT_SLOT_B},
  SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT,
  1,
  0,
  NULL,
  0,
  NULL,
  NULL
};

static const smart_band_workout_config_t g_workout_config =
{
  UINT64_C(3000),
  {
    {UINT32_C(800), UINT32_C(40), UINT32_C(2000), UINT16_C(30),
     UINT16_C(240)},
    {UINT32_C(1200), UINT32_C(80), UINT32_C(2000), UINT16_C(30),
     UINT16_C(240)}
  }
};

static const smart_band_step_normalizer_config_t g_step_config =
{
  32u,
  UINT64_C(2000),
  UINT64_C(0xffffff00),
  UINT64_C(0x000000ff),
  UINT64_C(5000)
};

static uint16_t read_le16(const uint8_t *input)
{
  return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8));
}

static uint32_t read_le32(const uint8_t *input)
{
  return (uint32_t)input[0] | ((uint32_t)input[1] << 8) |
         ((uint32_t)input[2] << 16) | ((uint32_t)input[3] << 24);
}

static uint64_t read_le64(const uint8_t *input)
{
  return (uint64_t)read_le32(input) |
         ((uint64_t)read_le32(input + 4) << 32);
}

static void write_le16(uint8_t *output, uint16_t value)
{
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *output, uint32_t value)
{
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8);
  output[2] = (uint8_t)(value >> 16);
  output[3] = (uint8_t)(value >> 24);
}

static void write_le64(uint8_t *output, uint64_t value)
{
  write_le32(output, (uint32_t)value);
  write_le32(output + 4, (uint32_t)(value >> 32));
}

static bool store_commit_succeeded(smart_band_store_result_t result)
{
  return result == SMART_BAND_STORE_OK;
}

static bool store_is_permanently_unavailable(
  const smart_band_workout_service_t *service)
{
  return service->store == NULL || !service->store->initialized ||
         smart_band_storage_is_permanently_unavailable(
           &service->store->backend);
}

static bool service_live_state(smart_band_workout_state_t state)
{
  return state == SMART_BAND_WORKOUT_STATE_COUNTDOWN ||
         state == SMART_BAND_WORKOUT_STATE_ACTIVE ||
         state == SMART_BAND_WORKOUT_STATE_PAUSED ||
         state == SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION;
}

static uint8_t source_flag(smart_band_step_source_t source)
{
  switch (source)
    {
      case SMART_BAND_STEP_SOURCE_SENSOR:
        return SMART_BAND_HISTORY_SOURCE_SENSOR;
      case SMART_BAND_STEP_SOURCE_DERIVED:
        return SMART_BAND_HISTORY_SOURCE_DERIVED;
      case SMART_BAND_STEP_SOURCE_SIMULATION:
        return SMART_BAND_HISTORY_SOURCE_SIMULATION;
      default:
        return 0u;
    }
}

static size_t encode_checkpoint(
  const smart_band_workout_service_t *service, checkpoint_kind_t kind,
  uint8_t *payload)
{
  smart_band_workout_snapshot_t snapshot;
  uint8_t flags = 0u;
  uint16_t heart_flags = 0u;

  memset(payload, 0, CHECKPOINT_LIVE_SIZE);
  write_le32(payload, CHECKPOINT_MAGIC);
  payload[4] = (uint8_t)kind;
  if (kind == CHECKPOINT_KIND_EMPTY)
    {
      return CHECKPOINT_HEADER_SIZE;
    }

  (void)smart_band_workout_model_snapshot(&service->model, &snapshot);
  if (service->recovered_session)
    {
      flags |= 1u << 0;
    }
  if (service->have_trusted_day)
    {
      flags |= 1u << 1;
    }
  if (service->recovery_gap)
    {
      flags |= 1u << 2;
    }
  if (service->clock_anomaly_seen)
    {
      flags |= 1u << 3;
    }
  if (snapshot.heart_rate_current_valid)
    {
      heart_flags |= 1u << 0;
    }
  if (snapshot.heart_rate_aggregate_valid)
    {
      heart_flags |= 1u << 1;
    }

  payload[5] = flags;
  write_le64(payload + 8, (uint64_t)service->start_wall_time);
  write_le32(payload + 16, (uint32_t)service->last_trusted_day);
  write_le32(payload + 20, service->pause_count);
  payload[24] = service->source_flags;
  payload[26] = (uint8_t)snapshot.mode;
  payload[27] = (uint8_t)snapshot.state;
  write_le64(payload + 28, snapshot.countdown_elapsed_ms);
  write_le64(payload + 36, snapshot.active_duration_ms);
  write_le64(payload + 44, snapshot.steps);
  write_le64(payload + 52, snapshot.distance_mm);
  write_le64(payload + 60, snapshot.calories_milli_kcal);
  write_le16(payload + 68, snapshot.heart_rate_current_bpm);
  write_le16(payload + 70, snapshot.heart_rate_min_bpm);
  write_le16(payload + 72, snapshot.heart_rate_max_bpm);
  write_le16(payload + 74, heart_flags);
  write_le64(payload + 76, snapshot.heart_rate_weighted_bpm_ms);
  write_le64(payload + 84, snapshot.heart_rate_weighted_duration_ms);
  write_le32(payload + 92, service->undated_steps);
  write_le32(payload + 96, service->undated_active_seconds);
  write_le32(payload + 100, service->undated_calories_milli_kcal);
  write_le32(payload + 104, service->daily_active_ms_remainder);
  write_le32(payload + 108, service->daily_heart_ms_remainder);
  return CHECKPOINT_LIVE_SIZE;
}

static bool decode_live_checkpoint(smart_band_workout_service_t *service,
                                   const uint8_t *payload,
                                   uint64_t monotonic_ms)
{
  smart_band_workout_snapshot_t snapshot;
  uint16_t heart_flags;

  memset(&snapshot, 0, sizeof(snapshot));
  service->start_wall_time = (int64_t)read_le64(payload + 8);
  service->last_trusted_day = (int32_t)read_le32(payload + 16);
  service->pause_count = read_le32(payload + 20);
  service->source_flags = payload[24];
  snapshot.mode = (smart_band_workout_mode_t)payload[26];
  snapshot.state = (smart_band_workout_state_t)payload[27];
  snapshot.countdown_elapsed_ms = read_le64(payload + 28);
  snapshot.active_duration_ms = read_le64(payload + 36);
  snapshot.steps = read_le64(payload + 44);
  snapshot.distance_mm = read_le64(payload + 52);
  snapshot.calories_milli_kcal = read_le64(payload + 60);
  snapshot.heart_rate_current_bpm = read_le16(payload + 68);
  snapshot.heart_rate_min_bpm = read_le16(payload + 70);
  snapshot.heart_rate_max_bpm = read_le16(payload + 72);
  heart_flags = read_le16(payload + 74);
  snapshot.heart_rate_current_valid = (heart_flags & (1u << 0)) != 0u;
  snapshot.heart_rate_aggregate_valid = (heart_flags & (1u << 1)) != 0u;
  snapshot.heart_rate_weighted_bpm_ms = read_le64(payload + 76);
  snapshot.heart_rate_weighted_duration_ms = read_le64(payload + 84);
  service->undated_steps = read_le32(payload + 92);
  service->undated_active_seconds = read_le32(payload + 96);
  service->undated_calories_milli_kcal = read_le32(payload + 100);
  service->daily_active_ms_remainder = read_le32(payload + 104);
  service->daily_heart_ms_remainder = read_le32(payload + 108);
  service->recovered_session = true;
  service->have_trusted_day = (payload[5] & (1u << 1)) != 0u;
  service->recovery_gap = true;
  service->clock_anomaly_seen = (payload[5] & (1u << 3)) != 0u;
  if (service->pause_count > UINT16_MAX ||
      service->daily_active_ms_remainder >= 1000u ||
      service->daily_heart_ms_remainder >= 1000u ||
      smart_band_workout_model_restore(&service->model, &g_workout_config,
                                       &snapshot, monotonic_ms) !=
      SMART_BAND_WORKOUT_RESULT_OK)
    {
      return false;
    }

  service->recovery_pending = true;
  service->last_checkpoint_ms = monotonic_ms;
  service->last_checkpoint_attempt_ms = monotonic_ms;
  return true;
}

static smart_band_store_result_t load_checkpoint(
  smart_band_workout_service_t *service, uint64_t monotonic_ms)
{
  uint8_t payload[CHECKPOINT_LIVE_SIZE];
  size_t payload_size = 0u;

  service->checkpoint_result = smart_band_store_load(
    service->store, &g_checkpoint_spec, payload, sizeof(payload),
    &payload_size, NULL);
  if (service->checkpoint_result == SMART_BAND_STORE_UNAVAILABLE ||
      payload_size == 0u)
    {
      return service->checkpoint_result;
    }
  if (service->checkpoint_result < SMART_BAND_STORE_OK ||
      payload_size < CHECKPOINT_HEADER_SIZE ||
      read_le32(payload) != CHECKPOINT_MAGIC)
    {
      service->checkpoint_result = SMART_BAND_STORE_DEGRADED;
      return service->checkpoint_result;
    }
  if (payload[4] == CHECKPOINT_KIND_EMPTY &&
      payload_size == CHECKPOINT_HEADER_SIZE)
    {
      return service->checkpoint_result;
    }
  if (payload[4] == CHECKPOINT_KIND_LIVE &&
      payload_size == CHECKPOINT_LIVE_SIZE &&
      decode_live_checkpoint(service, payload, monotonic_ms))
    {
      return service->checkpoint_result;
    }

  service->checkpoint_result = SMART_BAND_STORE_DEGRADED;
  return service->checkpoint_result;
}

static smart_band_store_result_t commit_checkpoint_with_session(
  smart_band_workout_service_t *service, checkpoint_kind_t kind,
  const smart_band_workout_session_t *session, uint64_t monotonic_ms)
{
  uint8_t payload[CHECKPOINT_LIVE_SIZE];
  smart_band_store_result_t recovery;
  size_t payload_size;

  service->last_checkpoint_attempt_ms = monotonic_ms;

  if (service->store == NULL || !service->store->initialized)
    {
      service->checkpoint_result = SMART_BAND_STORE_UNAVAILABLE;
      return service->checkpoint_result;
    }

  recovery = smart_band_history_recover_storage(service->history);
  if (recovery != SMART_BAND_STORE_OK)
    {
      service->checkpoint_result = recovery;
      return service->checkpoint_result;
    }
  if (session != NULL && session->id != service->history->next_session_id)
    {
      bool already_present = false;
      size_t index;

      for (index = 0; index < service->history->session_count; index++)
        {
          if (service->history->sessions[index].id == session->id &&
              smart_band_history_session_equal(
                &service->history->sessions[index], session))
            {
              already_present = true;
              break;
            }
        }
      if (!already_present)
        {
          if (service->history->next_session_id == 0u ||
              session != &service->pending_session)
            {
              service->checkpoint_result = SMART_BAND_STORE_INVALID;
              return service->checkpoint_result;
            }
          service->pending_session.id = service->history->next_session_id;
          session = &service->pending_session;
        }
    }

  payload_size = encode_checkpoint(service, kind, payload);
  service->checkpoint_result = smart_band_history_commit_workout(
    service->history, &g_checkpoint_spec, payload, payload_size, session);
  if (store_commit_succeeded(service->checkpoint_result))
    {
      service->last_checkpoint_ms = monotonic_ms;
    }
  return service->checkpoint_result;
}

static smart_band_store_result_t commit_checkpoint(
  smart_band_workout_service_t *service, checkpoint_kind_t kind,
  uint64_t monotonic_ms)
{
  return commit_checkpoint_with_session(service, kind, NULL, monotonic_ms);
}

static smart_band_workout_service_result_t finalize_pending(
  smart_band_workout_service_t *service, uint64_t monotonic_ms)
{
  smart_band_store_result_t result;

  service->last_checkpoint_attempt_ms = monotonic_ms;

  if (store_is_permanently_unavailable(service))
    {
      result = smart_band_history_flush_daily(service->history);
      if (result != SMART_BAND_STORE_OK &&
          result != SMART_BAND_STORE_UNAVAILABLE)
        {
          return SMART_BAND_WORKOUT_SERVICE_STORAGE_ERROR;
        }
      if (service->pending_session.id != 0u)
        {
          result = smart_band_history_append_session(
            service->history, &service->pending_session);
          if (result != SMART_BAND_STORE_OK &&
              result != SMART_BAND_STORE_UNAVAILABLE)
            {
              return SMART_BAND_WORKOUT_SERVICE_STORAGE_ERROR;
            }
        }
      service->phase = SMART_BAND_WORKOUT_SERVICE_PHASE_READY;
      return SMART_BAND_WORKOUT_SERVICE_OK;
    }

  result = commit_checkpoint_with_session(
    service, CHECKPOINT_KIND_EMPTY,
    service->pending_session.id == 0u ? NULL : &service->pending_session,
    monotonic_ms);
  if (!store_commit_succeeded(result))
    {
      return SMART_BAND_WORKOUT_SERVICE_STORAGE_ERROR;
    }

  service->phase = SMART_BAND_WORKOUT_SERVICE_PHASE_READY;
  return SMART_BAND_WORKOUT_SERVICE_OK;
}

static bool make_session(smart_band_workout_service_t *service,
                         time_t wall_time, bool wall_valid,
                         bool wall_rollback)
{
  smart_band_workout_snapshot_t snapshot;
  smart_band_workout_session_t *session = &service->pending_session;
  uint64_t average;

  if (service->history->next_session_id == 0u ||
      service->pause_count > UINT16_MAX ||
      smart_band_workout_model_snapshot(&service->model, &snapshot) !=
      SMART_BAND_WORKOUT_RESULT_OK || snapshot.steps > UINT32_MAX ||
      snapshot.distance_mm > UINT32_MAX ||
      snapshot.calories_milli_kcal > UINT32_MAX ||
      snapshot.heart_rate_current_bpm > UINT8_MAX ||
      snapshot.heart_rate_min_bpm > UINT8_MAX ||
      snapshot.heart_rate_max_bpm > UINT8_MAX)
    {
      return false;
    }

  average = smart_band_workout_average_heart_rate_bpm(&snapshot);
  if (average > UINT8_MAX)
    {
      return false;
    }

  memset(session, 0, sizeof(*session));
  session->id = service->history->next_session_id;
  session->start_wall_time = service->start_wall_time;
  session->end_wall_time = wall_valid ? (int64_t)wall_time : -1;
  session->active_duration_ms = snapshot.active_duration_ms;
  session->steps = (uint32_t)snapshot.steps;
  session->distance_mm = (uint32_t)snapshot.distance_mm;
  session->calories_milli_kcal = (uint32_t)snapshot.calories_milli_kcal;
  session->heart_current_bpm = snapshot.heart_rate_current_valid ?
                               (uint8_t)snapshot.heart_rate_current_bpm : 0u;
  session->heart_min_bpm = snapshot.heart_rate_aggregate_valid ?
                           (uint8_t)snapshot.heart_rate_min_bpm : 0u;
  session->heart_max_bpm = snapshot.heart_rate_aggregate_valid ?
                           (uint8_t)snapshot.heart_rate_max_bpm : 0u;
  session->heart_average_bpm = (uint8_t)average;
  session->pause_count = (uint16_t)service->pause_count;
  session->mode = (uint8_t)snapshot.mode;
  session->status = SMART_BAND_HISTORY_STATUS_FINISHED;
  session->source_flags = service->source_flags;
  session->flags = SMART_BAND_HISTORY_SESSION_COMPLETE;
  if (service->start_wall_time >= 0 && wall_valid && !wall_rollback &&
      !service->clock_anomaly_seen &&
      (int64_t)wall_time >= service->start_wall_time)
    {
      session->flags |= SMART_BAND_HISTORY_SESSION_RTC_VALID;
    }
  if (service->recovered_session)
    {
      session->flags |= SMART_BAND_HISTORY_SESSION_RECOVERED;
      session->flags = (uint8_t)(session->flags & UINT8_C(0xfb));
    }
  return true;
}

static bool add_undated(smart_band_workout_service_t *service,
                        uint64_t step_delta, uint64_t active_ms,
                        uint64_t calories)
{
  uint64_t active_total = service->daily_active_ms_remainder + active_ms;
  uint64_t seconds = active_total / 1000u;

  service->daily_active_ms_remainder = (uint32_t)(active_total % 1000u);
  service->undated_steps = step_delta > UINT32_MAX - service->undated_steps ?
                           UINT32_MAX :
                           service->undated_steps + (uint32_t)step_delta;
  service->undated_active_seconds =
    seconds > UINT32_MAX - service->undated_active_seconds ? UINT32_MAX :
    service->undated_active_seconds + (uint32_t)seconds;
  service->undated_calories_milli_kcal =
    calories > UINT32_MAX - service->undated_calories_milli_kcal ?
    UINT32_MAX : service->undated_calories_milli_kcal + (uint32_t)calories;
  service->recovery_gap = true;
  service->clock_anomaly_seen = true;
  if (service->have_trusted_day &&
      !smart_band_history_add_daily(
        service->history, service->last_trusted_day, 0u, 0u, 0u, 0u,
        false, 0u, 0u, SMART_BAND_HISTORY_DAY_RECOVERY_GAP))
    {
      return false;
    }
  return true;
}

static bool add_daily_endpoint(smart_band_workout_service_t *service,
                               int32_t day_key, uint64_t step_delta,
                               uint64_t active_ms, uint64_t calories,
                               uint16_t heart_rate_bpm,
                               bool heart_rate_valid, bool heart_rate_new,
                               uint8_t source_flags, uint8_t day_flags)
{
  uint64_t active_total = service->daily_active_ms_remainder + active_ms;
  uint32_t active_seconds = (uint32_t)(active_total / 1000u);
  uint32_t heart_seconds = 0u;

  service->daily_active_ms_remainder = (uint32_t)(active_total % 1000u);
  if (heart_rate_valid && heart_rate_new)
    {
      uint64_t heart_total = service->daily_heart_ms_remainder + active_ms;

      heart_seconds = (uint32_t)(heart_total / 1000u);
      service->daily_heart_ms_remainder = (uint32_t)(heart_total % 1000u);
    }

  if (step_delta == 0u && active_seconds == 0u && calories == 0u &&
      heart_seconds == 0u && source_flags == 0u && day_flags == 0u)
    {
      return true;
    }

  if (service->recovery_gap)
    {
      day_flags |= SMART_BAND_HISTORY_DAY_RECOVERY_GAP;
      service->recovery_gap = false;
    }

  if (step_delta > UINT32_MAX || calories > UINT32_MAX)
    {
      day_flags |= SMART_BAND_HISTORY_DAY_OVERFLOW;
      step_delta = step_delta > UINT32_MAX ? UINT32_MAX : step_delta;
      calories = calories > UINT32_MAX ? UINT32_MAX : calories;
    }

  return smart_band_history_add_daily(
    service->history, day_key, (uint32_t)step_delta, active_seconds,
    (uint32_t)calories, heart_rate_bpm, heart_rate_valid,
    heart_seconds, source_flags, day_flags);
}

static uint64_t scale_interval_value(uint64_t value, uint64_t part_ms,
                                     uint64_t total_ms)
{
  uint64_t whole;
  uint64_t remainder;

  if (value == 0u || part_ms == 0u || total_ms == 0u)
    {
      return 0u;
    }
  if (part_ms >= total_ms)
    {
      return value;
    }

  whole = (value / total_ms) * part_ms;
  remainder = value % total_ms;
  if (remainder <= UINT64_MAX / part_ms)
    {
      whole += (remainder * part_ms) / total_ms;
    }
  return whole;
}

static bool record_daily(smart_band_workout_service_t *service,
                         uint64_t step_delta, uint64_t active_ms,
                         uint64_t calories, uint16_t heart_rate_bpm,
                         bool heart_rate_valid, bool heart_rate_new,
                         uint8_t source_flags, time_t wall_time,
                         bool wall_valid, bool wall_rollback)
{
  int32_t day_key;
  uint8_t flags = 0u;

  if (!wall_valid || !smart_band_history_day_key(wall_time, &day_key))
    {
      if (!add_undated(service, step_delta, active_ms, calories))
        {
          return false;
        }
      service->last_wall_valid = false;
      return true;
    }

  if (service->have_trusted_day && day_key < service->last_trusted_day)
    {
      if (!add_undated(service, step_delta, active_ms, calories))
        {
          return false;
        }
      service->last_wall_time = (int64_t)wall_time;
      service->last_wall_valid = true;
      return true;
    }

  if (wall_rollback)
    {
      flags |= SMART_BAND_HISTORY_DAY_CLOCK_ROLLBACK;
      service->clock_anomaly_seen = true;
    }
  if (!service->have_trusted_day || day_key > service->last_trusted_day)
    {
      service->last_trusted_day = day_key;
      service->have_trusted_day = true;
    }

  if (service->last_wall_valid && wall_time >= service->last_wall_time &&
      wall_time > service->last_wall_time)
    {
      int32_t previous_day;

      if (smart_band_history_day_key((time_t)service->last_wall_time,
                                     &previous_day) &&
          previous_day < day_key)
        {
          uint64_t remaining_interval_ms =
            (uint64_t)((int64_t)wall_time - service->last_wall_time) * 1000u;

          if ((int64_t)day_key - previous_day >
              SMART_BAND_HISTORY_DAILY_CAPACITY)
            {
              if (!add_undated(service, step_delta, active_ms, calories))
                {
                  return false;
                }
              service->last_wall_time = (int64_t)wall_time;
              service->last_wall_valid = true;
              return true;
            }

          while (previous_day < day_key)
            {
              int64_t boundary = ((int64_t)previous_day + 1) *
                                 SECONDS_PER_DAY - CST_OFFSET_SECONDS;
              uint64_t part_ms = boundary > service->last_wall_time ?
                                 (uint64_t)(boundary -
                                            service->last_wall_time) * 1000u :
                                 0u;
              uint64_t part_steps;
              uint64_t part_active_ms;
              uint64_t part_calories;

              if (part_ms > remaining_interval_ms)
                {
                  part_ms = remaining_interval_ms;
                }
              part_steps = scale_interval_value(step_delta, part_ms,
                                                remaining_interval_ms);
              part_active_ms = scale_interval_value(
                active_ms, part_ms, remaining_interval_ms);
              part_calories = scale_interval_value(
                calories, part_ms, remaining_interval_ms);
              if (!add_daily_endpoint(service, previous_day, part_steps,
                                      part_active_ms, part_calories,
                                      heart_rate_bpm, heart_rate_valid,
                                      heart_rate_new, source_flags, flags))
                {
                  return false;
                }
              step_delta -= part_steps;
              active_ms -= part_active_ms;
              calories -= part_calories;
              remaining_interval_ms -= part_ms;
              service->last_wall_time = boundary;
              previous_day++;
            }
        }
    }

  service->last_wall_time = (int64_t)wall_time;
  service->last_wall_valid = true;
  return add_daily_endpoint(service, day_key, step_delta, active_ms,
                            calories, heart_rate_bpm, heart_rate_valid,
                            heart_rate_new, source_flags, flags);
}

int smart_band_workout_service_init(smart_band_workout_service_t *service,
                                    smart_band_store_t *store,
                                    smart_band_history_t *history,
                                    uint64_t monotonic_ms)
{
  if (service == NULL || history == NULL || !history->initialized)
    {
      return -1;
    }

  memset(service, 0, sizeof(*service));
  service->store = store;
  service->history = history;
  service->start_wall_time = -1;
  service->last_wall_time = -1;
  service->checkpoint_result = SMART_BAND_STORE_UNAVAILABLE;
  if (smart_band_step_normalizer_init(&service->step_normalizer,
                                      &g_step_config) !=
      SMART_BAND_STEP_RESULT_OK ||
      smart_band_workout_model_init(&service->model, &g_workout_config,
                                    SMART_BAND_WORKOUT_MODE_WALK,
                                    monotonic_ms) !=
      SMART_BAND_WORKOUT_RESULT_OK)
    {
      return -1;
    }

  service->initialized = true;
  if (store == NULL || !store->initialized)
    {
      return 0;
    }

  (void)load_checkpoint(service, monotonic_ms);
  return 0;
}

void smart_band_workout_service_reset(smart_band_workout_service_t *service)
{
  if (service != NULL)
    {
      memset(service, 0, sizeof(*service));
    }
}

smart_band_workout_service_result_t smart_band_workout_service_start(
  smart_band_workout_service_t *service, smart_band_workout_mode_t mode,
  uint64_t monotonic_ms, time_t wall_time, bool wall_valid)
{
  smart_band_workout_state_t state;

  if (service == NULL || !service->initialized ||
      mode >= SMART_BAND_WORKOUT_MODE_COUNT)
    {
      return SMART_BAND_WORKOUT_SERVICE_INVALID_ARGUMENT;
    }
  if (service->checkpoint_result == SMART_BAND_STORE_UNAVAILABLE &&
      !store_is_permanently_unavailable(service))
    {
      smart_band_store_result_t recovery =
        smart_band_history_recover_storage(service->history);

      if (recovery != SMART_BAND_STORE_OK ||
          load_checkpoint(service, monotonic_ms) < SMART_BAND_STORE_OK ||
          service->checkpoint_result == SMART_BAND_STORE_DEGRADED ||
          service->checkpoint_result == SMART_BAND_STORE_UNAVAILABLE)
        {
          return SMART_BAND_WORKOUT_SERVICE_STORAGE_ERROR;
        }
    }
  state = service->model.data.state;
  if (service->phase == SMART_BAND_WORKOUT_SERVICE_PHASE_FINALIZING ||
      service_live_state(state))
    {
      return SMART_BAND_WORKOUT_SERVICE_INVALID_STATE;
    }
  if (smart_band_workout_model_init(&service->model, &g_workout_config,
                                    mode, monotonic_ms) !=
      SMART_BAND_WORKOUT_RESULT_OK ||
      smart_band_workout_model_command(&service->model,
                                       SMART_BAND_WORKOUT_COMMAND_START,
                                       monotonic_ms) !=
      SMART_BAND_WORKOUT_RESULT_OK)
    {
      return SMART_BAND_WORKOUT_SERVICE_MODEL_ERROR;
    }

  service->start_wall_time = -1;
  service->pause_count = 0u;
  service->source_flags = 0u;
  service->recovery_pending = false;
  service->recovered_session = false;
  service->recovery_gap = !wall_valid;
  service->clock_anomaly_seen = !wall_valid;
  if (wall_valid)
    {
      service->last_wall_time = (int64_t)wall_time;
      service->last_wall_valid = true;
    }
  (void)commit_checkpoint(service, CHECKPOINT_KIND_LIVE, monotonic_ms);
  return SMART_BAND_WORKOUT_SERVICE_OK;
}

smart_band_workout_service_result_t smart_band_workout_service_command(
  smart_band_workout_service_t *service,
  smart_band_workout_command_t command, uint64_t monotonic_ms,
  time_t wall_time, bool wall_valid, bool wall_rollback)
{
  smart_band_workout_result_t result;

  if (service == NULL || !service->initialized ||
      command > SMART_BAND_WORKOUT_COMMAND_CONFIRM_RECOVERY)
    {
      return SMART_BAND_WORKOUT_SERVICE_INVALID_ARGUMENT;
    }
  if (service->phase == SMART_BAND_WORKOUT_SERVICE_PHASE_FINALIZING)
    {
      return SMART_BAND_WORKOUT_SERVICE_INVALID_STATE;
    }
  if (command == SMART_BAND_WORKOUT_COMMAND_START)
    {
      return SMART_BAND_WORKOUT_SERVICE_INVALID_ARGUMENT;
    }
  if (command == SMART_BAND_WORKOUT_COMMAND_PAUSE &&
      service->pause_count == UINT16_MAX)
    {
      return SMART_BAND_WORKOUT_SERVICE_RANGE_ERROR;
    }

  result = smart_band_workout_model_command(&service->model, command,
                                             monotonic_ms);
  if (result == SMART_BAND_WORKOUT_RESULT_INVALID_STATE)
    {
      return SMART_BAND_WORKOUT_SERVICE_INVALID_STATE;
    }
  if (result != SMART_BAND_WORKOUT_RESULT_OK)
    {
      return SMART_BAND_WORKOUT_SERVICE_MODEL_ERROR;
    }

  if (command == SMART_BAND_WORKOUT_COMMAND_PAUSE)
    {
      service->pause_count++;
    }
  else if (command == SMART_BAND_WORKOUT_COMMAND_CONFIRM_RECOVERY)
    {
      service->recovery_pending = false;
    }
  else if (command == SMART_BAND_WORKOUT_COMMAND_ABORT)
    {
      memset(&service->pending_session, 0, sizeof(service->pending_session));
      service->phase = SMART_BAND_WORKOUT_SERVICE_PHASE_FINALIZING;
      return finalize_pending(service, monotonic_ms);
    }
  else if (command == SMART_BAND_WORKOUT_COMMAND_FINISH)
    {
      if (!make_session(service, wall_time, wall_valid, wall_rollback))
        {
          return SMART_BAND_WORKOUT_SERVICE_RANGE_ERROR;
        }
      service->phase = SMART_BAND_WORKOUT_SERVICE_PHASE_FINALIZING;
      return finalize_pending(service, monotonic_ms);
    }

  (void)commit_checkpoint(service, CHECKPOINT_KIND_LIVE, monotonic_ms);
  return SMART_BAND_WORKOUT_SERVICE_OK;
}

smart_band_workout_service_result_t smart_band_workout_service_tick(
  smart_band_workout_service_t *service,
  const smart_band_step_sample_t *step_sample,
  uint16_t heart_rate_bpm, bool heart_rate_valid, bool heart_rate_new,
  uint64_t monotonic_ms, time_t wall_time,
  bool wall_valid, bool wall_rollback)
{
  smart_band_step_output_t step_output;
  smart_band_workout_snapshot_t before;
  smart_band_workout_snapshot_t after;
  smart_band_workout_sample_t sample;
  smart_band_step_result_t step_result;
  uint64_t active_delta;
  uint64_t calories_delta;
  uint8_t sample_source = 0u;
  bool transitioned;

  /* Elapsed weighting uses the current HR at the start of this tick. */
  (void)heart_rate_new;
  if (service == NULL || !service->initialized || step_sample == NULL ||
      step_sample->monotonic_ms != monotonic_ms)
    {
      return SMART_BAND_WORKOUT_SERVICE_INVALID_ARGUMENT;
    }

  if (heart_rate_valid &&
      (heart_rate_bpm < UINT16_C(30) || heart_rate_bpm > UINT16_C(240)))
    {
      heart_rate_valid = false;
    }

  if (service->phase == SMART_BAND_WORKOUT_SERVICE_PHASE_FINALIZING)
    {
      if (monotonic_ms - service->last_checkpoint_attempt_ms >=
          CHECKPOINT_RETRY_MS)
        {
          return finalize_pending(service, monotonic_ms);
        }
      return SMART_BAND_WORKOUT_SERVICE_OK;
    }

  step_result = smart_band_step_normalizer_push(
    &service->step_normalizer, step_sample, &step_output);
  if (step_result != SMART_BAND_STEP_RESULT_OK)
    {
      return SMART_BAND_WORKOUT_SERVICE_MODEL_ERROR;
    }
  if (step_sample->available && step_sample->fresh)
    {
      sample_source = source_flag(step_sample->source);
      service->source_flags |= sample_source;
    }

  (void)smart_band_workout_model_snapshot(&service->model, &before);
  after = before;
  if (before.state == SMART_BAND_WORKOUT_STATE_COUNTDOWN ||
      before.state == SMART_BAND_WORKOUT_STATE_ACTIVE ||
      before.state == SMART_BAND_WORKOUT_STATE_PAUSED)
    {
      memset(&sample, 0, sizeof(sample));
      sample.monotonic_ms = monotonic_ms;
      sample.step_delta = step_output.delta > UINT32_MAX ?
                          UINT32_MAX : (uint32_t)step_output.delta;
      sample.heart_rate_bpm = heart_rate_bpm;
      sample.heart_rate_valid = heart_rate_valid;
      if (step_output.delta > UINT32_MAX ||
          smart_band_workout_model_update(&service->model, &sample) !=
          SMART_BAND_WORKOUT_RESULT_OK)
        {
          return SMART_BAND_WORKOUT_SERVICE_MODEL_ERROR;
        }
      (void)smart_band_workout_model_snapshot(&service->model, &after);
    }

  active_delta = after.active_duration_ms - before.active_duration_ms;
  calories_delta = after.calories_milli_kcal - before.calories_milli_kcal;
  transitioned = before.state != after.state;
  if (before.state == SMART_BAND_WORKOUT_STATE_COUNTDOWN &&
      after.state == SMART_BAND_WORKOUT_STATE_ACTIVE &&
      service->start_wall_time < 0 && wall_valid)
    {
      service->start_wall_time = (int64_t)wall_time -
                                 (int64_t)(active_delta / 1000u);
    }

  if (!record_daily(
        service, step_output.delta, active_delta, calories_delta,
        before.heart_rate_current_bpm, before.heart_rate_current_valid,
        before.heart_rate_current_valid && active_delta != 0u,
        sample_source, wall_time, wall_valid, wall_rollback))
    {
      return SMART_BAND_WORKOUT_SERVICE_RANGE_ERROR;
    }

  if (transitioned ||
      (service_live_state(after.state) &&
       monotonic_ms - service->last_checkpoint_attempt_ms >=
       SMART_BAND_WORKOUT_CHECKPOINT_INTERVAL_MS))
    {
      if (commit_checkpoint(service, CHECKPOINT_KIND_LIVE, monotonic_ms) ==
          SMART_BAND_STORE_OK)
        {
          service->last_daily_flush_ms = monotonic_ms;
        }
    }
  else if (monotonic_ms - service->last_daily_flush_ms >=
           SMART_BAND_HISTORY_FLUSH_INTERVAL_MS)
    {
      checkpoint_kind_t kind = service_live_state(after.state) ?
                               CHECKPOINT_KIND_LIVE :
                               CHECKPOINT_KIND_EMPTY;

      if (commit_checkpoint(service, kind, monotonic_ms) ==
          SMART_BAND_STORE_OK)
        {
          service->last_daily_flush_ms = monotonic_ms;
        }
    }

  return SMART_BAND_WORKOUT_SERVICE_OK;
}

bool smart_band_workout_service_snapshot(
  const smart_band_workout_service_t *service,
  smart_band_workout_snapshot_t *snapshot)
{
  return service != NULL && service->initialized && snapshot != NULL &&
         smart_band_workout_model_snapshot(&service->model, snapshot) ==
         SMART_BAND_WORKOUT_RESULT_OK;
}

bool smart_band_workout_service_is_live(
  const smart_band_workout_service_t *service)
{
  return service != NULL && service->initialized &&
         service_live_state(service->model.data.state);
}

smart_band_store_result_t smart_band_workout_service_checkpoint(
  smart_band_workout_service_t *service, uint64_t monotonic_ms)
{
  checkpoint_kind_t kind;

  if (service == NULL || !service->initialized)
    {
      return SMART_BAND_STORE_INVALID;
    }
  if (service->phase == SMART_BAND_WORKOUT_SERVICE_PHASE_FINALIZING)
    {
      (void)finalize_pending(service, monotonic_ms);
      return service->checkpoint_result;
    }

  kind = service_live_state(service->model.data.state) ?
         CHECKPOINT_KIND_LIVE : CHECKPOINT_KIND_EMPTY;
  return commit_checkpoint(service, kind, monotonic_ms);
}

smart_band_workout_service_result_t
smart_band_workout_service_dismiss_summary(
  smart_band_workout_service_t *service, uint64_t monotonic_ms)
{
  smart_band_workout_mode_t mode;

  if (service == NULL || !service->initialized)
    {
      return SMART_BAND_WORKOUT_SERVICE_INVALID_ARGUMENT;
    }
  if (service->phase != SMART_BAND_WORKOUT_SERVICE_PHASE_READY ||
      (service->model.data.state != SMART_BAND_WORKOUT_STATE_FINISHED &&
       service->model.data.state != SMART_BAND_WORKOUT_STATE_ABORTED))
    {
      return SMART_BAND_WORKOUT_SERVICE_INVALID_STATE;
    }

  mode = service->model.data.mode;
  return smart_band_workout_model_init(&service->model, &g_workout_config,
                                       mode, monotonic_ms) ==
         SMART_BAND_WORKOUT_RESULT_OK ?
         SMART_BAND_WORKOUT_SERVICE_OK :
         SMART_BAND_WORKOUT_SERVICE_MODEL_ERROR;
}
