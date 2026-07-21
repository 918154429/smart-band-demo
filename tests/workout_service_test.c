#include "smart_band_history.h"
#include "smart_band_platform.h"
#include "smart_band_storage_backend.h"
#include "smart_band_workout_service.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WALL_BASE ((time_t)1800000000)

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

typedef struct
{
  smart_band_storage_t underlying;
  bool unavailable;
} recoverable_storage_t;

static smart_band_platform_result_t recoverable_read(
  void *opaque, uint32_t object_id, void *buffer, size_t capacity,
  size_t *actual_size)
{
  recoverable_storage_t *context = opaque;

  if (context->unavailable)
    {
      if (actual_size != NULL)
        {
          *actual_size = 0u;
        }
      return SMART_BAND_PLATFORM_UNAVAILABLE;
    }
  return context->underlying.ops->read(
    context->underlying.context, object_id, buffer, capacity, actual_size);
}

static smart_band_platform_result_t recoverable_write(
  void *opaque, uint32_t object_id, const void *buffer, size_t size)
{
  recoverable_storage_t *context = opaque;

  return context->unavailable ? SMART_BAND_PLATFORM_UNAVAILABLE :
         context->underlying.ops->write(context->underlying.context,
                                        object_id, buffer, size);
}

static smart_band_platform_result_t recoverable_flush(void *opaque)
{
  recoverable_storage_t *context = opaque;

  return context->unavailable ? SMART_BAND_PLATFORM_UNAVAILABLE :
         context->underlying.ops->flush(context->underlying.context);
}

static const smart_band_storage_ops_t g_recoverable_storage_ops =
{
  recoverable_read,
  recoverable_write,
  recoverable_flush,
  NULL
};

static smart_band_step_sample_t step_sample(
  smart_band_step_source_t source, uint64_t raw_counter, bool available,
  bool fresh, uint64_t monotonic_ms)
{
  smart_band_step_sample_t sample;

  memset(&sample, 0, sizeof(sample));
  sample.source = source;
  sample.raw_counter = raw_counter;
  sample.available = available;
  sample.fresh = fresh;
  sample.monotonic_ms = monotonic_ms;
  return sample;
}

static int tick_service(smart_band_workout_service_t *service,
                        smart_band_step_source_t source,
                        uint64_t raw_counter, bool available, bool fresh,
                        uint16_t heart_rate_bpm, bool heart_rate_valid,
                        bool heart_rate_new, uint64_t monotonic_ms)
{
  smart_band_step_sample_t sample = step_sample(
    source, raw_counter, available, fresh, monotonic_ms);
  time_t wall_time = WALL_BASE + (time_t)(monotonic_ms / 1000u);

  return smart_band_workout_service_tick(
           service, &sample, heart_rate_bpm, heart_rate_valid,
           heart_rate_new, monotonic_ms, wall_time, true, false) ==
         SMART_BAND_WORKOUT_SERVICE_OK ? 0 : -1;
}

static int snapshot_is(const smart_band_workout_service_t *service,
                       smart_band_workout_state_t state,
                       uint64_t active_ms, uint64_t steps)
{
  smart_band_workout_snapshot_t snapshot;

  if (!smart_band_workout_service_snapshot(service, &snapshot))
    {
      return 0;
    }

  return snapshot.state == state &&
         snapshot.active_duration_ms == active_ms &&
         snapshot.steps == steps;
}

static int test_volatile_lifecycle_and_background_ticks(void)
{
  smart_band_history_t history;
  smart_band_workout_service_t service;
  smart_band_workout_session_t sessions[2];
  size_t session_count;

  CHECK(smart_band_history_init(&history, NULL) == 0);
  CHECK(smart_band_workout_service_init(&service, NULL, &history, 0u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_IDLE, 0u, 0u));
  CHECK(!smart_band_workout_service_is_live(&service));

  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_WALK, 0u, WALL_BASE, true) ==
        SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_COUNTDOWN, 0u, 0u));
  CHECK(smart_band_workout_service_is_live(&service));

  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 0u, false, false, 0u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 0u, false, false, 2999u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_COUNTDOWN, 0u, 0u));
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 3000u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_ACTIVE, 0u, 0u));

  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 110u, true,
                     true, 105u, true, true, 4000u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_ACTIVE, 1000u, 10u));

  /* The service has no view-visible input: these ticks represent time spent
   * away from the Workout page and must still advance the session. */
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 115u, true,
                     true, 110u, true, true, 5000u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_ACTIVE, 2000u, 15u));

  CHECK(smart_band_workout_service_command(
          &service, SMART_BAND_WORKOUT_COMMAND_PAUSE, 5000u,
          WALL_BASE + 5, true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_PAUSED, 2000u, 15u));
  CHECK(service.pause_count == 1u);

  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 120u, true,
                     true, 115u, true, true, 9000u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_PAUSED, 2000u, 15u));
  CHECK(smart_band_workout_service_command(
          &service, SMART_BAND_WORKOUT_COMMAND_RESUME, 9000u,
          WALL_BASE + 9, true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 125u, true,
                     true, 120u, true, true, 11000u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_ACTIVE, 4000u, 20u));

  CHECK(smart_band_workout_service_command(
          &service, SMART_BAND_WORKOUT_COMMAND_FINISH, 12000u,
          WALL_BASE + 12, true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_FINISHED, 5000u, 20u));
  CHECK(!smart_band_workout_service_is_live(&service));
  CHECK(service.checkpoint_result == SMART_BAND_STORE_UNAVAILABLE);

  session_count = smart_band_history_latest_sessions(
    &history, sessions, sizeof(sessions) / sizeof(sessions[0]));
  CHECK(session_count == 1u);
  CHECK(sessions[0].steps == 20u);
  CHECK(sessions[0].active_duration_ms == 5000u);
  CHECK(sessions[0].pause_count == 1u);
  CHECK(sessions[0].mode == SMART_BAND_WORKOUT_MODE_WALK);
  CHECK(sessions[0].status == SMART_BAND_HISTORY_STATUS_FINISHED);
  CHECK((sessions[0].source_flags & SMART_BAND_HISTORY_SOURCE_SENSOR) != 0u);
  CHECK((sessions[0].flags & SMART_BAND_HISTORY_SESSION_RTC_VALID) != 0u);

  CHECK(smart_band_workout_service_command(
          &service, SMART_BAND_WORKOUT_COMMAND_FINISH, 12000u,
          WALL_BASE + 12, true, false) ==
        SMART_BAND_WORKOUT_SERVICE_INVALID_STATE);
  CHECK(smart_band_history_latest_sessions(
          &history, sessions, sizeof(sessions) / sizeof(sessions[0])) == 1u);

  smart_band_workout_service_reset(&service);
  smart_band_history_reset(&history);
  return 0;
}

static int test_initialized_noop_backend_remains_usable(void)
{
  smart_band_platform_t platform;
  smart_band_store_t store;
  smart_band_history_t history;
  smart_band_workout_service_t service;
  smart_band_workout_session_t session;

  smart_band_platform_init_noop(&platform);
  CHECK(smart_band_store_init(&store, &platform.storage) == 0);
  CHECK(smart_band_history_init(&history, &store) == 0);
  CHECK(smart_band_workout_service_init(&service, &store, &history, 0u) == 0);
  CHECK(service.checkpoint_result == SMART_BAND_STORE_UNAVAILABLE);

  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_RUN, 0u, WALL_BASE, true) ==
        SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SIMULATION, 500u, true,
                     true, 90u, true, true, 0u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SIMULATION, 500u, true,
                     true, 95u, true, true, 3000u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SIMULATION, 504u, true,
                     true, 100u, true, true, 4000u) == 0);
  CHECK(smart_band_workout_service_command(
          &service, SMART_BAND_WORKOUT_COMMAND_FINISH, 5000u,
          WALL_BASE + 5, true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(smart_band_history_latest_sessions(&history, &session, 1u) == 1u);
  CHECK(session.steps == 4u);
  CHECK((session.source_flags & SMART_BAND_HISTORY_SOURCE_SIMULATION) != 0u);
  CHECK(service.checkpoint_result == SMART_BAND_STORE_UNAVAILABLE);

  smart_band_workout_service_reset(&service);
  smart_band_history_reset(&history);
  smart_band_store_deinit(&store);
  return 0;
}

static int test_raw_step_source_switch_and_reset(void)
{
  smart_band_history_t history;
  smart_band_workout_service_t service;

  CHECK(smart_band_history_init(&history, NULL) == 0);
  CHECK(smart_band_workout_service_init(&service, NULL, &history, 0u) == 0);
  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_WALK, 0u, WALL_BASE, true) ==
        SMART_BAND_WORKOUT_SERVICE_OK);

  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 0u, false, false, 0u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 0u, false, false, 3000u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 110u, true,
                     true, 0u, false, false, 4000u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_ACTIVE, 1000u, 10u));

  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 5u, true,
                     true, 0u, false, false, 5000u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_ACTIVE, 2000u, 10u));
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 8u, true,
                     true, 0u, false, false, 6000u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_ACTIVE, 3000u, 13u));

  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_DERIVED, 1000u, true,
                     true, 0u, false, false, 7000u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_ACTIVE, 4000u, 13u));
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_DERIVED, 1004u, true,
                     true, 0u, false, false, 8000u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_ACTIVE, 5000u, 17u));

  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_DERIVED, 1008u, true,
                     false, 0u, false, false, 9000u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_DERIVED, 1010u, true,
                     true, 0u, false, false, 10000u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_ACTIVE, 7000u, 17u));
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_DERIVED, 1012u, true,
                     true, 0u, false, false, 11000u) == 0);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_ACTIVE, 8000u, 19u));
  CHECK((service.source_flags & SMART_BAND_HISTORY_SOURCE_SENSOR) != 0u);
  CHECK((service.source_flags & SMART_BAND_HISTORY_SOURCE_DERIVED) != 0u);
  CHECK((service.source_flags & SMART_BAND_HISTORY_SOURCE_SIMULATION) == 0u);

  smart_band_workout_service_reset(&service);
  smart_band_history_reset(&history);
  return 0;
}

static int test_summary_dismissal_and_new_session(void)
{
  smart_band_history_t history;
  smart_band_workout_service_t service;

  CHECK(smart_band_history_init(&history, NULL) == 0);
  CHECK(smart_band_workout_service_init(&service, NULL, &history, 0u) == 0);
  CHECK(smart_band_workout_service_dismiss_summary(&service, 0u) ==
        SMART_BAND_WORKOUT_SERVICE_INVALID_STATE);
  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_WALK, 0u, WALL_BASE, true) ==
        SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 10u, true,
                     true, 100u, true, true, 0u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 10u, true,
                     true, 100u, true, true, 3000u) == 0);
  CHECK(smart_band_workout_service_command(
          &service, SMART_BAND_WORKOUT_COMMAND_FINISH, 4000u,
          WALL_BASE + 4, true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(smart_band_workout_service_dismiss_summary(&service, 4000u) ==
        SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_IDLE, 0u, 0u));
  CHECK(history.session_count == 1u);
  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_RUN, 5000u, WALL_BASE + 5,
          true) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(snapshot_is(&service, SMART_BAND_WORKOUT_STATE_COUNTDOWN, 0u, 0u));
  return 0;
}

static int test_rtc_invalid_and_rollback_flags(void)
{
  smart_band_history_t history;
  smart_band_workout_service_t service;
  smart_band_step_sample_t sample;
  smart_band_workout_session_t session;

  CHECK(smart_band_history_init(&history, NULL) == 0);
  CHECK(smart_band_workout_service_init(&service, NULL, &history, 0u) == 0);
  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_WALK, 0u, (time_t)0,
          false) == SMART_BAND_WORKOUT_SERVICE_OK);
  sample = step_sample(SMART_BAND_STEP_SOURCE_SENSOR, 100u, true, true, 0u);
  CHECK(smart_band_workout_service_tick(
          &service, &sample, 100u, true, true, 0u, (time_t)0,
          false, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  sample.monotonic_ms = 3000u;
  CHECK(smart_band_workout_service_tick(
          &service, &sample, 100u, true, true, 3000u, (time_t)0,
          false, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(smart_band_workout_service_command(
          &service, SMART_BAND_WORKOUT_COMMAND_FINISH, 4000u,
          (time_t)0, false, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(smart_band_history_latest_sessions(&history, &session, 1u) == 1u);
  CHECK(session.start_wall_time == -1 && session.end_wall_time == -1);
  CHECK((session.flags & SMART_BAND_HISTORY_SESSION_RTC_VALID) == 0u);

  CHECK(smart_band_workout_service_dismiss_summary(&service, 4000u) ==
        SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_WALK, 5000u, WALL_BASE,
          true) == SMART_BAND_WORKOUT_SERVICE_OK);
  sample.raw_counter = 200u;
  sample.monotonic_ms = 5000u;
  CHECK(smart_band_workout_service_tick(
          &service, &sample, 100u, true, true, 5000u, WALL_BASE,
          true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  sample.monotonic_ms = 8000u;
  CHECK(smart_band_workout_service_tick(
          &service, &sample, 100u, true, true, 8000u, WALL_BASE + 3,
          true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  sample.raw_counter = 202u;
  sample.monotonic_ms = 9000u;
  CHECK(smart_band_workout_service_tick(
          &service, &sample, 101u, true, true, 9000u, WALL_BASE + 2,
          true, true) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(history.daily_count != 0u);
  CHECK((history.daily[history.daily_count - 1u].flags &
         SMART_BAND_HISTORY_DAY_CLOCK_ROLLBACK) != 0u);
  CHECK((history.daily[history.daily_count - 1u].flags &
         SMART_BAND_HISTORY_DAY_COMPLETE) == 0u);
  CHECK(smart_band_workout_service_command(
          &service, SMART_BAND_WORKOUT_COMMAND_FINISH, 10000u,
          WALL_BASE + 3, true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(smart_band_history_latest_sessions(&history, &session, 1u) == 1u);
  CHECK(session.id == 2u);
  CHECK((session.flags & SMART_BAND_HISTORY_SESSION_RTC_VALID) == 0u);
  return 0;
}

static int test_cross_midnight_splits_interval_metrics(void)
{
  const time_t boundary = (time_t)INT64_C(1784649600);
  smart_band_history_t history;
  smart_band_workout_service_t service;
  smart_band_step_sample_t sample;
  smart_band_daily_summary_t days[2];

  CHECK(smart_band_history_init(&history, NULL) == 0);
  CHECK(smart_band_workout_service_init(&service, NULL, &history, 0u) == 0);
  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_WALK, 0u, boundary - 5,
          true) == SMART_BAND_WORKOUT_SERVICE_OK);
  sample = step_sample(SMART_BAND_STEP_SOURCE_SENSOR, 100u, true, true, 0u);
  CHECK(smart_band_workout_service_tick(
          &service, &sample, 100u, true, true, 0u, boundary - 5,
          true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  sample.monotonic_ms = 3000u;
  CHECK(smart_band_workout_service_tick(
          &service, &sample, 100u, true, true, 3000u, boundary - 2,
          true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  sample.raw_counter = 108u;
  sample.monotonic_ms = 7000u;
  CHECK(smart_band_workout_service_tick(
          &service, &sample, 100u, true, true, 7000u, boundary + 2,
          true, false) == SMART_BAND_WORKOUT_SERVICE_OK);

  CHECK(smart_band_history_latest_days(&history, days, 2u) == 2u);
  CHECK(days[0].day_key == 20655 && days[1].day_key == 20656);
  CHECK(days[0].steps == 4u && days[1].steps == 4u);
  CHECK(days[0].active_seconds == 2u && days[1].active_seconds == 2u);
  CHECK(days[0].calories_milli_kcal == 160u &&
        days[1].calories_milli_kcal == 160u);
  CHECK(days[0].heart_duration_seconds == 2u &&
        days[1].heart_duration_seconds == 2u);
  CHECK(smart_band_history_daily_average_heart_rate(&days[0]) == 100u);
  CHECK(smart_band_history_daily_average_heart_rate(&days[1]) == 100u);
  return 0;
}

static int init_memory_stack(smart_band_storage_memory_t *memory,
                             smart_band_storage_t *storage,
                             smart_band_store_t *store,
                             smart_band_history_t *history,
                             smart_band_workout_service_t *service,
                             uint64_t monotonic_ms, bool initialize_memory)
{
  if (initialize_memory &&
      smart_band_storage_memory_init(memory, storage) !=
      SMART_BAND_PLATFORM_OK)
    {
      return -1;
    }
  if (smart_band_store_init(store, storage) != 0 ||
      smart_band_history_init(history, store) != 0 ||
      smart_band_workout_service_init(service, store, history,
                                      monotonic_ms) != 0)
    {
      return -1;
    }
  return 0;
}

static int test_checkpoint_boundary_recovery_and_finalization(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store1;
  smart_band_history_t history1;
  smart_band_workout_service_t service1;
  smart_band_workout_snapshot_t before_restart;
  smart_band_store_t store2;
  smart_band_history_t history2;
  smart_band_workout_service_t service2;
  smart_band_workout_snapshot_t recovered;
  smart_band_store_t store3;
  smart_band_history_t history3;
  smart_band_workout_service_t service3;
  smart_band_workout_session_t sessions[2];

  CHECK(init_memory_stack(&memory, &storage, &store1, &history1, &service1,
                          0u, true) == 0);
  CHECK(smart_band_workout_service_start(
          &service1, SMART_BAND_WORKOUT_MODE_WALK, 0u, WALL_BASE, true) ==
        SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 0u) == 0);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 3000u) == 0);
  CHECK(service1.last_checkpoint_ms == 3000u);

  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 101u, true,
                     true, 101u, true, true, 32999u) == 0);
  CHECK(service1.last_checkpoint_ms == 3000u);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 101u, true,
                     true, 101u, true, true, 33000u) == 0);
  CHECK(service1.last_checkpoint_ms == 33000u);
  CHECK(service1.checkpoint_result == SMART_BAND_STORE_OK);

  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 103u, true,
                     true, 102u, true, true, 34000u) == 0);
  CHECK(smart_band_workout_service_checkpoint(&service1, 34000u) ==
        SMART_BAND_STORE_OK);
  CHECK(service1.last_checkpoint_ms == 34000u);
  CHECK(smart_band_workout_service_snapshot(&service1, &before_restart));

  smart_band_workout_service_reset(&service1);
  smart_band_history_reset(&history1);
  smart_band_store_deinit(&store1);

  CHECK(init_memory_stack(&memory, &storage, &store2, &history2, &service2,
                          50000u, false) == 0);
  CHECK(service2.recovery_pending);
  CHECK(service2.recovered_session);
  CHECK(smart_band_workout_service_snapshot(&service2, &recovered));
  CHECK(recovered.state == SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION);
  CHECK(recovered.active_duration_ms == before_restart.active_duration_ms);
  CHECK(recovered.steps == before_restart.steps);

  CHECK(smart_band_workout_service_command(
          &service2, SMART_BAND_WORKOUT_COMMAND_CONFIRM_RECOVERY, 50000u,
          WALL_BASE + 50, true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(snapshot_is(&service2, SMART_BAND_WORKOUT_STATE_PAUSED,
                    before_restart.active_duration_ms,
                    before_restart.steps));
  CHECK(!service2.recovery_pending);
  CHECK(smart_band_workout_service_command(
          &service2, SMART_BAND_WORKOUT_COMMAND_RESUME, 50000u,
          WALL_BASE + 50, true, false) == SMART_BAND_WORKOUT_SERVICE_OK);

  CHECK(tick_service(&service2, SMART_BAND_STEP_SOURCE_SENSOR, 1000u, true,
                     true, 105u, true, true, 50000u) == 0);
  CHECK(tick_service(&service2, SMART_BAND_STEP_SOURCE_SENSOR, 1004u, true,
                     true, 106u, true, true, 51000u) == 0);
  CHECK(smart_band_workout_service_command(
          &service2, SMART_BAND_WORKOUT_COMMAND_FINISH, 52000u,
          WALL_BASE + 52, true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(history2.session_count == 1u);

  smart_band_workout_service_reset(&service2);
  smart_band_history_reset(&history2);
  smart_band_store_deinit(&store2);

  CHECK(init_memory_stack(&memory, &storage, &store3, &history3, &service3,
                          60000u, false) == 0);
  CHECK(!service3.recovery_pending);
  CHECK(snapshot_is(&service3, SMART_BAND_WORKOUT_STATE_IDLE, 0u, 0u));
  CHECK(smart_band_history_latest_sessions(
          &history3, sessions, sizeof(sessions) / sizeof(sessions[0])) == 1u);
  CHECK((sessions[0].flags & SMART_BAND_HISTORY_SESSION_RECOVERED) != 0u);
  CHECK(sessions[0].steps == before_restart.steps + 4u);

  smart_band_workout_service_reset(&service3);
  smart_band_history_reset(&history3);
  smart_band_store_deinit(&store3);
  return 0;
}

static int test_abort_flushes_daily_before_clearing_checkpoint(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store1;
  smart_band_history_t history1;
  smart_band_workout_service_t service1;
  smart_band_store_t store2;
  smart_band_history_t history2;
  smart_band_workout_service_t service2;
  smart_band_daily_summary_t day;

  CHECK(init_memory_stack(&memory, &storage, &store1, &history1, &service1,
                          0u, true) == 0);
  CHECK(smart_band_workout_service_start(
          &service1, SMART_BAND_WORKOUT_MODE_WALK, 0u, WALL_BASE,
          true) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 0u) == 0);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 3000u) == 0);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 110u, true,
                     true, 101u, true, true, 4000u) == 0);
  CHECK(smart_band_workout_service_command(
          &service1, SMART_BAND_WORKOUT_COMMAND_ABORT, 4000u,
          WALL_BASE + 4, true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(history1.daily_dirty_shards == 0u);

  smart_band_workout_service_reset(&service1);
  smart_band_history_reset(&history1);
  smart_band_store_deinit(&store1);
  CHECK(init_memory_stack(&memory, &storage, &store2, &history2, &service2,
                          5000u, false) == 0);
  CHECK(!service2.recovery_pending);
  CHECK(smart_band_history_latest_days(&history2, &day, 1u) == 1u);
  CHECK(day.steps == 10u && day.active_seconds == 1u);
  smart_band_workout_service_reset(&service2);
  smart_band_history_reset(&history2);
  smart_band_store_deinit(&store2);
  return 0;
}

static int test_checkpoint_failure_is_rate_limited(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_history_t history;
  smart_band_workout_service_t service;
  uint32_t calls_after_failure;

  CHECK(init_memory_stack(&memory, &storage, &store, &history, &service,
                          0u, true) == 0);
  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_WALK, 0u, WALL_BASE,
          true) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 0u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 3000u) == 0);
  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_IO, 1u, 0u, 0u) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 101u, true,
                     true, 101u, true, true, 33000u) == 0);
  CHECK(service.checkpoint_result != SMART_BAND_STORE_OK);
  CHECK(service.last_checkpoint_attempt_ms == 33000u);
  calls_after_failure = memory.fault.matching_calls;
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 102u, true,
                     true, 102u, true, true, 34000u) == 0);
  CHECK(memory.fault.matching_calls == calls_after_failure);
  CHECK(service.last_checkpoint_attempt_ms == 33000u);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 103u, true,
                     true, 103u, true, true, 63000u) == 0);
  CHECK(service.last_checkpoint_attempt_ms == 63000u);
  CHECK(service.checkpoint_result == SMART_BAND_STORE_OK);
  smart_band_storage_fault_reset(&memory.fault);
  smart_band_workout_service_reset(&service);
  smart_band_history_reset(&history);
  smart_band_store_deinit(&store);
  return 0;
}

static int prepare_finish_fixture(
  smart_band_storage_memory_t *memory, smart_band_storage_t *storage,
  smart_band_store_t *store, smart_band_history_t *history,
  smart_band_workout_service_t *service)
{
  if (init_memory_stack(memory, storage, store, history, service, 0u,
                        true) != 0 ||
      smart_band_workout_service_start(
        service, SMART_BAND_WORKOUT_MODE_WALK, 0u, WALL_BASE, true) !=
        SMART_BAND_WORKOUT_SERVICE_OK ||
      tick_service(service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                   true, 100u, true, true, 0u) != 0 ||
      tick_service(service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                   true, 100u, true, true, 3000u) != 0 ||
      tick_service(service, SMART_BAND_STEP_SOURCE_SENSOR, 110u, true,
                   true, 101u, true, true, 4000u) != 0)
    {
      return -1;
    }
  return 0;
}

static uint32_t finish_operation_call_count(
  smart_band_storage_operation_t operation)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_history_t history;
  smart_band_workout_service_t service;
  smart_band_storage_fault_state_t state;

  if (prepare_finish_fixture(&memory, &storage, &store, &history, &service) !=
        0 ||
      smart_band_storage_fault_arm(
        &memory.fault, operation, SMART_BAND_STORAGE_FAULT_IO, UINT32_MAX,
        0u, 0u) != SMART_BAND_PLATFORM_OK ||
      smart_band_workout_service_command(
        &service, SMART_BAND_WORKOUT_COMMAND_FINISH, 5000u, WALL_BASE + 5,
        true, false) != SMART_BAND_WORKOUT_SERVICE_OK)
    {
      return 0u;
    }
  smart_band_storage_fault_snapshot(&memory.fault, &state);
  return state.matching_calls;
}

static int verify_finish_recovery_is_coherent(
  smart_band_storage_memory_t *memory, smart_band_storage_t *storage)
{
  smart_band_store_t store;
  smart_band_history_t history;
  smart_band_workout_service_t service;
  smart_band_workout_snapshot_t snapshot;
  smart_band_daily_summary_t day;
  smart_band_workout_session_t session;
  size_t day_count;
  size_t session_count;

  smart_band_storage_fault_reset(&memory->fault);
  CHECK(init_memory_stack(memory, storage, &store, &history, &service,
                          10000u, false) == 0);
  CHECK(smart_band_workout_service_snapshot(&service, &snapshot));
  day_count = smart_band_history_latest_days(&history, &day, 1u);
  session_count = smart_band_history_latest_sessions(&history, &session, 1u);
  if (session_count == 0u)
    {
      CHECK(service.recovery_pending);
      CHECK(snapshot.state == SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION);
      CHECK(day_count == 0u || day.steps == 0u);
    }
  else
    {
      CHECK(session_count == 1u);
      CHECK(!service.recovery_pending);
      CHECK(snapshot.state == SMART_BAND_WORKOUT_STATE_IDLE);
      CHECK(day_count == 1u && day.steps == 10u);
      CHECK(session.steps == 10u);
    }
  smart_band_workout_service_reset(&service);
  smart_band_history_reset(&history);
  smart_band_store_deinit(&store);
  return 0;
}

static int test_finish_transaction_crash_cut_matrix(void)
{
  smart_band_storage_operation_t operations[] =
  {
    SMART_BAND_STORAGE_OPERATION_READ,
    SMART_BAND_STORAGE_OPERATION_WRITE,
    SMART_BAND_STORAGE_OPERATION_FLUSH
  };
  size_t operation_index;

  for (operation_index = 0;
       operation_index < sizeof(operations) / sizeof(operations[0]);
       operation_index++)
    {
      smart_band_storage_operation_t operation = operations[operation_index];
      uint32_t calls = finish_operation_call_count(operation);
      uint32_t trigger;

      CHECK(calls != 0u);
      for (trigger = 1u; trigger <= calls; trigger++)
        {
          smart_band_storage_memory_t memory;
          smart_band_storage_t storage;
          smart_band_store_t store;
          smart_band_history_t history;
          smart_band_workout_service_t service;
          smart_band_storage_fault_kind_t kind =
            operation == SMART_BAND_STORAGE_OPERATION_WRITE ?
            SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION :
            SMART_BAND_STORAGE_FAULT_IO;

          CHECK(prepare_finish_fixture(&memory, &storage, &store, &history,
                                       &service) == 0);
          CHECK(smart_band_storage_fault_arm(
                  &memory.fault, operation, kind, trigger, 17u,
                  UINT8_C(0x01)) == SMART_BAND_PLATFORM_OK);
          (void)smart_band_workout_service_command(
            &service, SMART_BAND_WORKOUT_COMMAND_FINISH, 5000u,
            WALL_BASE + 5, true, false);
          smart_band_workout_service_reset(&service);
          smart_band_history_reset(&history);
          smart_band_store_deinit(&store);
          CHECK(verify_finish_recovery_is_coherent(&memory, &storage) == 0);
        }
    }

  return 0;
}

static int test_temporary_unavailable_backend_recovers_finalization(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t underlying;
  recoverable_storage_t recoverable;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_history_t history;
  smart_band_workout_service_t service;
  smart_band_workout_session_t session;

  CHECK(smart_band_storage_memory_init(&memory, &underlying) ==
        SMART_BAND_PLATFORM_OK);
  memset(&recoverable, 0, sizeof(recoverable));
  recoverable.underlying = underlying;
  recoverable.unavailable = false;
  memset(&storage, 0, sizeof(storage));
  storage.ops = &g_recoverable_storage_ops;
  storage.context = &recoverable;
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_history_init(&history, &store) == 0);
  CHECK(!history.daily_writes_blocked && !history.session_writes_blocked);
  CHECK(smart_band_workout_service_init(&service, &store, &history, 0u) == 0);
  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_RUN, 0u, WALL_BASE, true) ==
        SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 0u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 3000u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 105u, true,
                     true, 105u, true, true, 4000u) == 0);
  recoverable.unavailable = true;
  CHECK(smart_band_workout_service_command(
          &service, SMART_BAND_WORKOUT_COMMAND_FINISH, 5000u,
          WALL_BASE + 5, true, false) ==
        SMART_BAND_WORKOUT_SERVICE_STORAGE_ERROR);
  CHECK(service.phase == SMART_BAND_WORKOUT_SERVICE_PHASE_FINALIZING);
  CHECK(history.session_count == 0u);

  recoverable.unavailable = false;
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 105u, true,
                     true, 105u, true, true, 10000u) == 0);
  CHECK(service.phase == SMART_BAND_WORKOUT_SERVICE_PHASE_READY);
  CHECK(smart_band_history_latest_sessions(&history, &session, 1u) == 1u);
  CHECK(session.steps == 5u);

  smart_band_workout_service_reset(&service);
  smart_band_history_reset(&history);
  smart_band_store_deinit(&store);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_history_init(&history, &store) == 0);
  CHECK(smart_band_workout_service_init(&service, &store, &history,
                                        11000u) == 0);
  CHECK(!service.recovery_pending);
  CHECK(smart_band_history_latest_sessions(&history, &session, 1u) == 1u);
  CHECK(session.steps == 5u);
  smart_band_workout_service_reset(&service);
  smart_band_history_reset(&history);
  smart_band_store_deinit(&store);
  return 0;
}

static int test_initial_unavailable_reloads_existing_baseline(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t underlying;
  recoverable_storage_t recoverable;
  smart_band_storage_t storage;
  smart_band_store_t store1;
  smart_band_history_t history1;
  smart_band_workout_service_t service1;
  smart_band_store_t store2;
  smart_band_history_t history2;
  smart_band_workout_service_t service2;
  smart_band_workout_snapshot_t snapshot;
  smart_band_daily_summary_t day;
  smart_band_workout_session_t session;
  int32_t day_key;

  CHECK(smart_band_storage_memory_init(&memory, &underlying) ==
        SMART_BAND_PLATFORM_OK);
  memset(&recoverable, 0, sizeof(recoverable));
  recoverable.underlying = underlying;
  memset(&storage, 0, sizeof(storage));
  storage.ops = &g_recoverable_storage_ops;
  storage.context = &recoverable;

  CHECK(smart_band_store_init(&store1, &storage) == 0);
  CHECK(smart_band_history_init(&history1, &store1) == 0);
  CHECK(smart_band_workout_service_init(
          &service1, &store1, &history1, 0u) == 0);
  CHECK(smart_band_workout_service_start(
          &service1, SMART_BAND_WORKOUT_MODE_WALK, 0u, WALL_BASE, true) ==
        SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 0u) == 0);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 3000u) == 0);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 105u, true,
                     true, 105u, true, true, 4000u) == 0);
  CHECK(smart_band_workout_service_command(
          &service1, SMART_BAND_WORKOUT_COMMAND_FINISH, 5000u,
          WALL_BASE + 5, true, false) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(smart_band_workout_service_dismiss_summary(&service1, 5000u) ==
        SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(smart_band_workout_service_start(
          &service1, SMART_BAND_WORKOUT_MODE_RUN, 6000u,
          WALL_BASE + 6, true) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 105u, true,
                     true, 105u, true, true, 6000u) == 0);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 105u, true,
                     true, 105u, true, true, 9000u) == 0);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 108u, true,
                     true, 108u, true, true, 10000u) == 0);
  CHECK(smart_band_workout_service_checkpoint(&service1, 10000u) ==
        SMART_BAND_STORE_OK);
  CHECK(smart_band_history_day_key(WALL_BASE + 10, &day_key));
  CHECK(smart_band_history_add_daily(
          &history1, day_key, 0u, 0u, 0u, 0u, false, 0u, 0u,
          SMART_BAND_HISTORY_DAY_RECOVERY_GAP));
  CHECK(smart_band_workout_service_checkpoint(&service1, 10001u) ==
        SMART_BAND_STORE_OK);
  smart_band_workout_service_reset(&service1);
  smart_band_history_reset(&history1);
  smart_band_store_deinit(&store1);

  recoverable.unavailable = true;
  CHECK(smart_band_store_init(&store2, &storage) == 0);
  CHECK(smart_band_history_init(&history2, &store2) == 0);
  CHECK(history2.storage_reload_pending);
  CHECK(smart_band_workout_service_init(
          &service2, &store2, &history2, 11000u) == 0);
  CHECK(service2.checkpoint_result == SMART_BAND_STORE_UNAVAILABLE);
  CHECK(smart_band_history_day_key(WALL_BASE + 5, &day_key));
  CHECK(smart_band_history_add_daily(
          &history2, day_key, 2u, 0u, 0u, 0u, false, 0u,
          SMART_BAND_HISTORY_SOURCE_SENSOR, 0u));
  CHECK(smart_band_workout_service_start(
          &service2, SMART_BAND_WORKOUT_MODE_RUN, 11000u,
          WALL_BASE + 11, true) == SMART_BAND_WORKOUT_SERVICE_STORAGE_ERROR);

  recoverable.unavailable = false;
  CHECK(smart_band_workout_service_start(
          &service2, SMART_BAND_WORKOUT_MODE_RUN, 12000u,
          WALL_BASE + 12, true) == SMART_BAND_WORKOUT_SERVICE_INVALID_STATE);
  CHECK(!history2.storage_reload_pending);
  CHECK(service2.recovery_pending);
  CHECK(smart_band_workout_service_snapshot(&service2, &snapshot));
  CHECK(snapshot.state ==
        SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION);
  CHECK(snapshot.steps == 3u);
  CHECK(smart_band_history_latest_days(&history2, &day, 1u) == 1u);
  CHECK(day.steps == 10u);
  CHECK((day.flags & SMART_BAND_HISTORY_DAY_RECOVERY_GAP) != 0u);
  CHECK((day.flags & SMART_BAND_HISTORY_DAY_OVERFLOW) == 0u);
  CHECK(smart_band_history_latest_sessions(&history2, &session, 1u) == 1u);
  CHECK(session.id == 1u && session.steps == 5u);

  smart_band_workout_service_reset(&service2);
  smart_band_history_reset(&history2);
  smart_band_store_deinit(&store2);
  return 0;
}

static int test_malformed_checkpoint_and_finalization_boundaries(void)
{
  static const smart_band_store_record_spec_t checkpoint_spec =
  {
    {SMART_BAND_WORKOUT_CHECKPOINT_SLOT_A,
     SMART_BAND_WORKOUT_CHECKPOINT_SLOT_B},
    SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT, 1, 0,
    NULL, 0, NULL, NULL
  };
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_history_t history;
  smart_band_workout_service_t service;
  uint8_t bad_magic[8] = {0};
  uint8_t bad_kind[8] = {0x57, 0x43, 0x50, 0x31, 2, 0, 0, 0};
  uint8_t bad_live[112] = {0};
  uint8_t empty[8] = {0x57, 0x43, 0x50, 0x31, 0, 0, 0, 0};

  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_history_init(&history, &store) == 0);

  CHECK(smart_band_store_commit(
          &store, &checkpoint_spec, bad_magic, sizeof(bad_magic), NULL) ==
        SMART_BAND_STORE_OK);
  CHECK(smart_band_workout_service_init(&service, &store, &history, 0u) == 0);
  CHECK(service.checkpoint_result == SMART_BAND_STORE_DEGRADED);
  smart_band_workout_service_reset(&service);

  CHECK(smart_band_store_commit(
          &store, &checkpoint_spec, bad_kind, sizeof(bad_kind), NULL) ==
        SMART_BAND_STORE_OK);
  CHECK(smart_band_workout_service_init(&service, &store, &history, 0u) == 0);
  CHECK(service.checkpoint_result == SMART_BAND_STORE_DEGRADED);
  smart_band_workout_service_reset(&service);

  memcpy(bad_live, empty, sizeof(empty));
  bad_live[4] = 1u;
  bad_live[104] = UINT8_C(0xe8);
  bad_live[105] = UINT8_C(0x03);
  CHECK(smart_band_store_commit(
          &store, &checkpoint_spec, bad_live, sizeof(bad_live), NULL) ==
        SMART_BAND_STORE_OK);
  CHECK(smart_band_workout_service_init(&service, &store, &history, 0u) == 0);
  CHECK(service.checkpoint_result == SMART_BAND_STORE_DEGRADED);
  smart_band_workout_service_reset(&service);

  CHECK(smart_band_store_commit(
          &store, &checkpoint_spec, empty, sizeof(empty), NULL) ==
        SMART_BAND_STORE_OK);
  CHECK(smart_band_workout_service_init(&service, &store, &history, 0u) == 0);
  service.phase = SMART_BAND_WORKOUT_SERVICE_PHASE_FINALIZING;
  CHECK(smart_band_workout_service_checkpoint(&service, 1u) ==
        SMART_BAND_STORE_OK);
  CHECK(service.phase == SMART_BAND_WORKOUT_SERVICE_PHASE_READY);

  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_WALK, 2u,
          WALL_BASE, true) == SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 2u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 3002u) == 0);
  history.next_session_id = 0u;
  CHECK(smart_band_workout_service_command(
          &service, SMART_BAND_WORKOUT_COMMAND_FINISH, 4002u,
          WALL_BASE + 4, true, false) ==
        SMART_BAND_WORKOUT_SERVICE_RANGE_ERROR);

  smart_band_workout_service_reset(&service);
  smart_band_history_reset(&history);
  smart_band_store_deinit(&store);
  return 0;
}

static int test_idle_daily_flush_uses_empty_checkpoint_transaction(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store1;
  smart_band_history_t history1;
  smart_band_workout_service_t service1;
  smart_band_store_t store2;
  smart_band_history_t history2;
  smart_band_workout_service_t service2;
  smart_band_daily_summary_t day;
  uint64_t monotonic_ms;

  CHECK(init_memory_stack(&memory, &storage, &store1, &history1, &service1,
                          0u, true) == 0);
  CHECK(tick_service(&service1, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 0u) == 0);
  for (monotonic_ms = 1000u;
       monotonic_ms <= SMART_BAND_HISTORY_FLUSH_INTERVAL_MS;
       monotonic_ms += 1000u)
    {
      CHECK(tick_service(
              &service1, SMART_BAND_STEP_SOURCE_SENSOR,
              monotonic_ms == SMART_BAND_HISTORY_FLUSH_INTERVAL_MS ?
                110u : 100u,
              true, true, 100u, true, false, monotonic_ms) == 0);
    }
  CHECK(history1.daily_dirty_shards == 0u);
  smart_band_workout_service_reset(&service1);
  smart_band_history_reset(&history1);
  smart_band_store_deinit(&store1);

  CHECK(init_memory_stack(&memory, &storage, &store2, &history2, &service2,
                          SMART_BAND_HISTORY_FLUSH_INTERVAL_MS + 1000u,
                          false) == 0);
  CHECK(!service2.recovery_pending);
  CHECK(smart_band_history_latest_days(&history2, &day, 1u) == 1u);
  CHECK(day.steps == 10u);
  smart_band_workout_service_reset(&service2);
  smart_band_history_reset(&history2);
  smart_band_store_deinit(&store2);
  return 0;
}

static int test_single_tick_spanning_two_midnights_is_split_per_day(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_history_t history;
  smart_band_workout_service_t service;
  smart_band_workout_snapshot_t snapshot;
  smart_band_daily_summary_t days[4];
  uint64_t active_seconds = 0u;
  uint64_t calories = 0u;
  size_t count;
  size_t index;
  uint64_t final_ms = UINT64_C(172803000);

  CHECK(init_memory_stack(&memory, &storage, &store, &history, &service,
                          0u, true) == 0);
  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_WALK, 0u, WALL_BASE, true) ==
        SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 0u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 100u, true, true, 3000u) == 0);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, false,
                     false, 100u, true, true, final_ms) == 0);
  CHECK(smart_band_workout_service_snapshot(&service, &snapshot));
  CHECK(snapshot.active_duration_ms == UINT64_C(172800000));
  count = smart_band_history_latest_days(&history, days,
                                         sizeof(days) / sizeof(days[0]));
  CHECK(count == 3u);
  for (index = 0; index < count; index++)
    {
      active_seconds += days[index].active_seconds;
      calories += days[index].calories_milli_kcal;
      if (index != 0u)
        {
          CHECK(days[index].day_key == days[index - 1u].day_key + 1);
        }
    }
  CHECK(active_seconds == snapshot.active_duration_ms / 1000u);
  CHECK(calories == snapshot.calories_milli_kcal);
  smart_band_workout_service_reset(&service);
  smart_band_history_reset(&history);
  smart_band_store_deinit(&store);
  return 0;
}

static int test_service_api_boundaries(void)
{
  smart_band_history_t history;
  smart_band_workout_service_t service;
  smart_band_step_sample_t sample;

  CHECK(smart_band_history_init(&history, NULL) == 0);
  CHECK(smart_band_workout_service_init(NULL, NULL, &history, 0u) == -1);
  CHECK(smart_band_workout_service_init(&service, NULL, &history, 0u) == 0);
  CHECK(!smart_band_workout_service_is_live(NULL));
  CHECK(smart_band_workout_service_checkpoint(NULL, 0u) ==
        SMART_BAND_STORE_INVALID);
  CHECK(smart_band_workout_service_dismiss_summary(NULL, 0u) ==
        SMART_BAND_WORKOUT_SERVICE_INVALID_ARGUMENT);
  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_COUNT, 0u, WALL_BASE, true) ==
        SMART_BAND_WORKOUT_SERVICE_INVALID_ARGUMENT);
  CHECK(smart_band_workout_service_command(
          &service, SMART_BAND_WORKOUT_COMMAND_START, 0u, WALL_BASE, true,
          false) == SMART_BAND_WORKOUT_SERVICE_INVALID_ARGUMENT);
  CHECK(smart_band_workout_service_command(
          &service,
          (smart_band_workout_command_t)(SMART_BAND_WORKOUT_COMMAND_CONFIRM_RECOVERY + 1),
          0u, WALL_BASE, true, false) ==
        SMART_BAND_WORKOUT_SERVICE_INVALID_ARGUMENT);
  sample = step_sample(SMART_BAND_STEP_SOURCE_SENSOR, 0u, true, true, 1u);
  CHECK(smart_band_workout_service_tick(
          &service, &sample, 100u, true, true, 0u, WALL_BASE, true, false) ==
        SMART_BAND_WORKOUT_SERVICE_INVALID_ARGUMENT);

  CHECK(smart_band_workout_service_start(
          &service, SMART_BAND_WORKOUT_MODE_WALK, 0u, WALL_BASE, true) ==
        SMART_BAND_WORKOUT_SERVICE_OK);
  CHECK(tick_service(&service, SMART_BAND_STEP_SOURCE_SENSOR, 100u, true,
                     true, 25u, true, true, 0u) == 0);
  service.pause_count = UINT16_MAX;
  CHECK(smart_band_workout_service_command(
          &service, SMART_BAND_WORKOUT_COMMAND_PAUSE, 1u, WALL_BASE, true,
          false) == SMART_BAND_WORKOUT_SERVICE_RANGE_ERROR);
  smart_band_workout_service_reset(&service);
  smart_band_history_reset(&history);
  return 0;
}

int main(void)
{
  CHECK(test_volatile_lifecycle_and_background_ticks() == 0);
  CHECK(test_initialized_noop_backend_remains_usable() == 0);
  CHECK(test_raw_step_source_switch_and_reset() == 0);
  CHECK(test_summary_dismissal_and_new_session() == 0);
  CHECK(test_rtc_invalid_and_rollback_flags() == 0);
  CHECK(test_cross_midnight_splits_interval_metrics() == 0);
  CHECK(test_checkpoint_boundary_recovery_and_finalization() == 0);
  CHECK(test_abort_flushes_daily_before_clearing_checkpoint() == 0);
  CHECK(test_checkpoint_failure_is_rate_limited() == 0);
  CHECK(test_finish_transaction_crash_cut_matrix() == 0);
  CHECK(test_temporary_unavailable_backend_recovers_finalization() == 0);
  CHECK(test_initial_unavailable_reloads_existing_baseline() == 0);
  CHECK(test_malformed_checkpoint_and_finalization_boundaries() == 0);
  CHECK(test_idle_daily_flush_uses_empty_checkpoint_transaction() == 0);
  CHECK(test_single_tick_spanning_two_midnights_is_split_per_day() == 0);
  CHECK(test_service_api_boundaries() == 0);
  puts("smart band workout service tests passed");
  return 0;
}
