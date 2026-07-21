#include "smart_band_history.h"
#include "smart_band_storage_backend.h"
#include "smart_band_storage_codec.h"

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

#define DAILY_FULL_PAYLOAD_SIZE ((size_t)428)
#define SESSION_FULL_PAYLOAD_SIZE ((size_t)508)

static smart_band_storage_memory_t g_memory;

static smart_band_workout_session_t make_session(uint32_t id)
{
  smart_band_workout_session_t session;

  memset(&session, 0, sizeof(session));
  session.id = id;
  session.start_wall_time = INT64_C(1784649600) + (int64_t)id * 60;
  session.end_wall_time = session.start_wall_time + 45;
  session.active_duration_ms = UINT64_C(30000) + (uint64_t)id * 1000u;
  session.steps = id * 10u;
  session.distance_mm = id * 8000u;
  session.calories_milli_kcal = id * 400u;
  session.heart_current_bpm = (uint8_t)(90u + id % 20u);
  session.heart_min_bpm = 80u;
  session.heart_max_bpm = 150u;
  session.heart_average_bpm = 110u;
  session.pause_count = (uint16_t)id;
  session.mode = (uint8_t)(id % SMART_BAND_WORKOUT_MODE_COUNT);
  session.status = SMART_BAND_HISTORY_STATUS_FINISHED;
  session.source_flags = SMART_BAND_HISTORY_SOURCE_SENSOR;
  session.flags = SMART_BAND_HISTORY_SESSION_RTC_VALID |
                  SMART_BAND_HISTORY_SESSION_COMPLETE;
  return session;
}

static int test_fixed_utc8_day_key_and_format(void)
{
  int32_t day_key;
  char formatted[16];

  CHECK(smart_band_history_day_key((time_t)0, &day_key));
  CHECK(day_key == 0);
  CHECK(smart_band_history_format_day(day_key, formatted, sizeof(formatted)));
  CHECK(strcmp(formatted, "1970-01-01") == 0);

  CHECK(smart_band_history_day_key((time_t)INT64_C(1784649599), &day_key));
  CHECK(day_key == 20655);
  CHECK(smart_band_history_format_day(day_key, formatted, sizeof(formatted)));
  CHECK(strcmp(formatted, "2026-07-21") == 0);

  CHECK(smart_band_history_day_key((time_t)INT64_C(1784649600), &day_key));
  CHECK(day_key == 20656);
  CHECK(smart_band_history_format_day(day_key, formatted, sizeof(formatted)));
  CHECK(strcmp(formatted, "2026-07-22") == 0);
  CHECK(!smart_band_history_day_key((time_t)-1, &day_key));
  CHECK(!smart_band_history_day_key((time_t)0, NULL));
  CHECK(!smart_band_history_format_day(day_key, NULL, sizeof(formatted)));
  return 0;
}

static int test_missing_dates(void)
{
  smart_band_history_t history;
  smart_band_daily_summary_t output[3];

  CHECK(smart_band_history_init(&history, NULL) == 0);
  CHECK(smart_band_history_add_daily(
    &history, 21000, 10u, 1u, 2u, 100u, true, 1u,
    SMART_BAND_HISTORY_SOURCE_SENSOR, 0u));
  CHECK(smart_band_history_add_daily(
    &history, 21002, 20u, 2u, 4u, 120u, true, 1u,
    SMART_BAND_HISTORY_SOURCE_DERIVED, 0u));
  CHECK(smart_band_history_latest_days(&history, output, 3u) == 2u);
  CHECK(output[0].day_key == 21000 && output[1].day_key == 21002);
  CHECK(output[0].steps == 10u && output[1].steps == 20u);
  CHECK(smart_band_history_daily_average_heart_rate(&output[0]) == 100u);
  CHECK(smart_band_history_daily_average_heart_rate(&output[1]) == 120u);
  return 0;
}

static int test_sparse_days_stay_inside_calendar_window(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_history_t history;
  smart_band_daily_summary_t output[SMART_BAND_HISTORY_DAILY_CAPACITY];
  uint32_t index;

  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_history_init(&history, &store) == 0);
  for (index = 0u; index < 16u; index++)
    {
      CHECK(smart_band_history_add_daily(
        &history, 21000 + (int32_t)(index * 2u), 1u, 0u, 0u, 0u,
        false, 0u, SMART_BAND_HISTORY_SOURCE_SENSOR, 0u));
    }

  CHECK(history.daily_count == 15u);
  CHECK(smart_band_history_latest_days(&history, output,
                                       SMART_BAND_HISTORY_DAILY_CAPACITY) ==
        15u);
  CHECK(output[0].day_key == 21002);
  CHECK(output[14].day_key == 21030);
  CHECK(smart_band_history_flush_daily(&history) == SMART_BAND_STORE_OK);
  smart_band_store_deinit(&store);
  return 0;
}

static int test_session_ids_must_be_contiguous(void)
{
  smart_band_history_t history;
  smart_band_workout_session_t session;

  CHECK(smart_band_history_init(&history, NULL) == 0);
  session = make_session(2u);
  CHECK(smart_band_history_append_session(&history, &session) ==
        SMART_BAND_STORE_INVALID);
  CHECK(history.session_count == 0u);
  CHECK(history.next_session_id == 1u);
  session = make_session(1u);
  CHECK(smart_band_history_append_session(&history, &session) ==
        SMART_BAND_STORE_UNAVAILABLE);
  CHECK(history.session_count == 1u);
  CHECK(history.next_session_id == 2u);
  return 0;
}

static int populate_daily(smart_band_history_t *history)
{
  uint32_t index;

  for (index = 0; index < SMART_BAND_HISTORY_DAILY_CAPACITY; index++)
    {
      CHECK(smart_band_history_add_daily(
        history, 20000 + (int32_t)index, index + 1u, index,
        (index + 1u) * 10u, (uint16_t)(100u + index % 10u), true,
        2u, (index % 2u) == 0u ? SMART_BAND_HISTORY_SOURCE_SENSOR :
                                SMART_BAND_HISTORY_SOURCE_SIMULATION,
        0u));
    }

  CHECK(history->daily_count == SMART_BAND_HISTORY_DAILY_CAPACITY);
  CHECK(smart_band_history_flush_daily(history) == SMART_BAND_STORE_OK);
  CHECK(smart_band_history_add_daily(
    history, 20030, UINT32_MAX, UINT32_MAX, UINT32_MAX, 0u, false, 0u,
    SMART_BAND_HISTORY_SOURCE_DERIVED, 0u));
  CHECK(history->daily_count == SMART_BAND_HISTORY_DAILY_CAPACITY);
  CHECK(smart_band_history_flush_daily(history) == SMART_BAND_STORE_OK);
  return 0;
}

static int populate_sessions(smart_band_history_t *history)
{
  smart_band_workout_session_t session;
  uint32_t id;

  for (id = 1u; id <= SMART_BAND_HISTORY_SESSION_CAPACITY; id++)
    {
      session = make_session(id);
      if (id == SMART_BAND_HISTORY_SESSION_CAPACITY)
        {
          session.active_duration_ms = UINT64_MAX;
          session.steps = UINT32_MAX;
          session.distance_mm = UINT32_MAX;
          session.calories_milli_kcal = UINT32_MAX;
          session.pause_count = UINT16_MAX;
        }
      CHECK(smart_band_history_append_session(history, &session) ==
            SMART_BAND_STORE_OK);
    }

  CHECK(history->session_count == SMART_BAND_HISTORY_SESSION_CAPACITY);
  session = make_session(31u);
  CHECK(smart_band_history_append_session(history, &session) ==
        SMART_BAND_STORE_OK);
  CHECK(history->session_count == SMART_BAND_HISTORY_SESSION_CAPACITY);
  return 0;
}

static int check_backend_capacity(const smart_band_storage_memory_t *memory)
{
  smart_band_storage_decode_options_t options;
  smart_band_storage_record_view_t view;
  size_t used = 0u;
  size_t largest_daily = 0u;
  size_t largest_session = 0u;
  size_t index;

  memset(&options, 0, sizeof(options));
  for (index = 0; index < SMART_BAND_STORAGE_MEMORY_MAX_OBJECTS; index++)
    {
      const smart_band_storage_memory_object_t *object =
        &memory->objects[index];

      if (!object->used)
        {
          continue;
        }

      used++;
      CHECK(object->size <= SMART_BAND_STORAGE_MEMORY_OBJECT_CAPACITY);
      CHECK(object->size <= SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE);
      CHECK(smart_band_storage_codec_decode(object->data, object->size,
                                            &options, &view) ==
            SMART_BAND_STORAGE_CODEC_OK);
      if (view.record_type == SMART_BAND_STORAGE_RECORD_DAILY_HISTORY &&
          view.payload_length > largest_daily)
        {
          largest_daily = view.payload_length;
        }
      if (view.record_type == SMART_BAND_STORAGE_RECORD_WORKOUT_HISTORY &&
          view.payload_length > largest_session)
        {
          largest_session = view.payload_length;
        }
    }

  CHECK(used <= SMART_BAND_STORAGE_MEMORY_MAX_OBJECTS);
  CHECK(largest_daily == DAILY_FULL_PAYLOAD_SIZE);
  CHECK(largest_session == SESSION_FULL_PAYLOAD_SIZE);
  return 0;
}

static int test_capacity_persistence_and_idempotence(void)
{
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_store_t reloaded_store;
  smart_band_history_t history;
  smart_band_history_t reloaded;
  smart_band_daily_summary_t days[SMART_BAND_HISTORY_DAILY_CAPACITY];
  smart_band_workout_session_t sessions[SMART_BAND_HISTORY_SESSION_CAPACITY];
  smart_band_workout_session_t duplicate;
  uint64_t generation;
  size_t index;

  CHECK(smart_band_storage_memory_init(&g_memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_history_init(&history, &store) == 0);
  CHECK(populate_daily(&history) == 0);
  CHECK(populate_sessions(&history) == 0);

  CHECK(smart_band_history_latest_days(&history, days,
        SMART_BAND_HISTORY_DAILY_CAPACITY) ==
        SMART_BAND_HISTORY_DAILY_CAPACITY);
  CHECK(days[0].day_key == 20001 && days[29].day_key == 20030);
  CHECK(days[29].steps == UINT32_MAX &&
        days[29].active_seconds == UINT32_MAX &&
        days[29].calories_milli_kcal == UINT32_MAX);

  CHECK(smart_band_history_latest_sessions(&history, sessions,
        SMART_BAND_HISTORY_SESSION_CAPACITY) ==
        SMART_BAND_HISTORY_SESSION_CAPACITY);
  CHECK(sessions[0].id == 2u && sessions[29].id == 31u);
  for (index = 1; index < SMART_BAND_HISTORY_SESSION_CAPACITY; index++)
    {
      CHECK(sessions[index].id == sessions[index - 1u].id + 1u);
    }

  duplicate = make_session(31u);
  generation = store.generation;
  CHECK(smart_band_history_append_session(&history, &duplicate) ==
        SMART_BAND_STORE_OK);
  CHECK(store.generation == generation);
  CHECK(history.session_count == SMART_BAND_HISTORY_SESSION_CAPACITY);
  duplicate.steps++;
  CHECK(smart_band_history_append_session(&history, &duplicate) ==
        SMART_BAND_STORE_INVALID);
  CHECK(store.generation == generation);
  CHECK(history.session_count == SMART_BAND_HISTORY_SESSION_CAPACITY);

  CHECK(check_backend_capacity(&g_memory) == 0);
  smart_band_store_deinit(&store);
  CHECK(smart_band_store_init(&reloaded_store, &storage) == 0);
  CHECK(smart_band_history_init(&reloaded, &reloaded_store) == 0);
  CHECK(reloaded.daily_count == SMART_BAND_HISTORY_DAILY_CAPACITY);
  CHECK(reloaded.session_count == SMART_BAND_HISTORY_SESSION_CAPACITY);
  CHECK(reloaded.next_session_id == 32u);

  CHECK(smart_band_history_latest_days(&reloaded, days,
        SMART_BAND_HISTORY_DAILY_CAPACITY) ==
        SMART_BAND_HISTORY_DAILY_CAPACITY);
  CHECK(days[0].day_key == 20001 && days[29].day_key == 20030);
  CHECK(days[29].steps == UINT32_MAX);
  CHECK(smart_band_history_latest_sessions(&reloaded, sessions,
        SMART_BAND_HISTORY_SESSION_CAPACITY) ==
        SMART_BAND_HISTORY_SESSION_CAPACITY);
  CHECK(sessions[0].id == 2u && sessions[29].id == 31u);
  CHECK(smart_band_history_session_equal(&sessions[29],
                                         &(smart_band_workout_session_t){
                                           .id = 31u,
                                           .start_wall_time = INT64_C(1784651460),
                                           .end_wall_time = INT64_C(1784651505),
                                           .active_duration_ms = UINT64_C(61000),
                                           .steps = 310u,
                                           .distance_mm = 248000u,
                                           .calories_milli_kcal = 12400u,
                                           .heart_current_bpm = 101u,
                                           .heart_min_bpm = 80u,
                                           .heart_max_bpm = 150u,
                                           .heart_average_bpm = 110u,
                                           .pause_count = 31u,
                                           .mode = SMART_BAND_WORKOUT_MODE_RUN,
                                           .status = SMART_BAND_HISTORY_STATUS_FINISHED,
                                           .source_flags = SMART_BAND_HISTORY_SOURCE_SENSOR,
                                           .flags = SMART_BAND_HISTORY_SESSION_RTC_VALID |
                                                    SMART_BAND_HISTORY_SESSION_COMPLETE
                                         }));
  smart_band_store_deinit(&reloaded_store);
  return 0;
}

static int test_history_write_faults_preserve_old_complete_data(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_store_t reloaded_store;
  smart_band_history_t history;
  smart_band_history_t reloaded;
  smart_band_workout_session_t sessions[3];
  smart_band_workout_session_t session;
  smart_band_storage_fault_kind_t faults[] =
  {
    SMART_BAND_STORAGE_FAULT_SHORT_WRITE,
    SMART_BAND_STORAGE_FAULT_NO_SPACE,
    SMART_BAND_STORAGE_FAULT_READ_ONLY,
    SMART_BAND_STORAGE_FAULT_IO
  };
  size_t fault_index;

  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_history_init(&history, &store) == 0);
  session = make_session(1u);
  CHECK(smart_band_history_append_session(&history, &session) ==
        SMART_BAND_STORE_OK);
  session = make_session(2u);
  CHECK(smart_band_history_append_session(&history, &session) ==
        SMART_BAND_STORE_OK);

  for (fault_index = 0;
       fault_index < sizeof(faults) / sizeof(faults[0]); fault_index++)
    {
      CHECK(smart_band_storage_fault_arm(
        &memory.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
        faults[fault_index], 1u, 17u, UINT8_C(0x01)) ==
        SMART_BAND_PLATFORM_OK);
      session = make_session(3u);
      CHECK(smart_band_history_append_session(&history, &session) !=
            SMART_BAND_STORE_OK);
      CHECK(history.session_count == 2u);
      CHECK(smart_band_history_latest_sessions(&history, sessions, 3u) == 2u);
      CHECK(sessions[0].id == 1u && sessions[1].id == 2u);
      smart_band_storage_fault_reset(&memory.fault);
    }

  smart_band_store_deinit(&store);
  CHECK(smart_band_store_init(&reloaded_store, &storage) == 0);
  CHECK(smart_band_history_init(&reloaded, &reloaded_store) == 0);
  CHECK(smart_band_history_latest_sessions(&reloaded, sessions, 3u) == 2u);
  CHECK(sessions[0].id == 1u && sessions[1].id == 2u);
  smart_band_store_deinit(&reloaded_store);
  return 0;
}

static int test_recovered_snapshot_cannot_overwrite_newer_generation(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_store_t recovered_store;
  smart_band_store_t clean_store;
  smart_band_history_t history;
  smart_band_history_t recovered;
  smart_band_history_t clean;
  smart_band_workout_session_t session;
  smart_band_workout_session_t sessions[4];
  uint32_t id;

  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_history_init(&history, &store) == 0);
  for (id = 1u; id <= 4u; id++)
    {
      session = make_session(id);
      CHECK(smart_band_history_append_session(&history, &session) ==
            SMART_BAND_STORE_OK);
    }
  smart_band_store_deinit(&store);

  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_IO, 8u, 0u, 0u) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&recovered_store, &storage) == 0);
  CHECK(smart_band_history_init(&recovered, &recovered_store) == 0);
  CHECK(recovered.last_session_result == SMART_BAND_STORE_RECOVERED);
  CHECK(recovered.session_writes_blocked);
  session = make_session(recovered.next_session_id);
  CHECK(smart_band_history_append_session(&recovered, &session) ==
        SMART_BAND_STORE_RECOVERED);
  smart_band_store_deinit(&recovered_store);
  smart_band_storage_fault_reset(&memory.fault);

  CHECK(smart_band_store_init(&clean_store, &storage) == 0);
  CHECK(smart_band_history_init(&clean, &clean_store) == 0);
  CHECK(smart_band_history_latest_sessions(&clean, sessions, 4u) == 4u);
  for (id = 0u; id < 4u; id++)
    {
      CHECK(sessions[id].id == id + 1u);
    }
  smart_band_store_deinit(&clean_store);
  return 0;
}

static int test_invalid_overflow_and_transaction_boundaries(void)
{
  static const smart_band_store_record_spec_t checkpoint_spec =
  {
    {UINT32_C(0x00070000), UINT32_C(0x00070001)},
    SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT, 1, 0,
    NULL, 0, NULL, NULL
  };
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_history_t history;
  smart_band_workout_session_t session;
  smart_band_daily_summary_t day;
  uint8_t checkpoint = 0u;
  int32_t day_key;
  char formatted[16];
  int64_t too_many_days = ((int64_t)INT32_MAX + 1) * INT64_C(86400) -
                          INT64_C(28800);

  CHECK(smart_band_history_init(NULL, NULL) == -1);
  CHECK(!smart_band_history_format_day(0, formatted, 0u));
  CHECK(!smart_band_history_day_key((time_t)too_many_days, &day_key));
  CHECK(!smart_band_history_add_daily(NULL, 1, 0u, 0u, 0u, 0u, false,
                                      0u, 0u, 0u));
  CHECK(smart_band_history_flush_daily(NULL) == SMART_BAND_STORE_INVALID);

  CHECK(smart_band_history_init(&history, NULL) == 0);
  CHECK(!smart_band_history_add_daily(&history, -1, 0u, 0u, 0u, 0u,
                                      false, 0u, 0u, 0u));
  CHECK(!smart_band_history_add_daily(&history, 22000, 0u, 0u, 0u, 256u,
                                      true, 1u, 0u, 0u));
  CHECK(smart_band_history_add_daily(
    &history, 22000, UINT32_MAX, UINT32_MAX, UINT32_MAX, 255u, true,
    UINT32_MAX, SMART_BAND_HISTORY_SOURCE_SENSOR,
    SMART_BAND_HISTORY_DAY_OVERFLOW));
  CHECK(smart_band_history_add_daily(
    &history, 22000, 1u, 1u, 1u, 255u, true, 1u,
    SMART_BAND_HISTORY_SOURCE_DERIVED, 0u));
  CHECK(smart_band_history_latest_days(&history, &day, 1u) == 1u);
  CHECK(day.steps == UINT32_MAX && day.active_seconds == UINT32_MAX &&
        day.calories_milli_kcal == UINT32_MAX);
  CHECK((day.flags & SMART_BAND_HISTORY_DAY_OVERFLOW) != 0u);

  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_history_init(&history, &store) == 0);
  CHECK(smart_band_history_commit_workout(
          &history, NULL, &checkpoint, sizeof(checkpoint), NULL) ==
        SMART_BAND_STORE_INVALID);
  CHECK(smart_band_history_commit_workout(
          &history, &checkpoint_spec, NULL, 1u, NULL) ==
        SMART_BAND_STORE_INVALID);
  history.daily_writes_blocked = true;
  history.last_daily_result = SMART_BAND_STORE_RECOVERED;
  CHECK(smart_band_history_commit_workout(
          &history, &checkpoint_spec, &checkpoint, sizeof(checkpoint), NULL) ==
        SMART_BAND_STORE_RECOVERED);
  history.daily_writes_blocked = false;
  session = make_session(1u);
  session.mode = SMART_BAND_WORKOUT_MODE_COUNT;
  CHECK(smart_band_history_commit_workout(
          &history, &checkpoint_spec, &checkpoint, sizeof(checkpoint),
          &session) == SMART_BAND_STORE_INVALID);
  smart_band_store_deinit(&store);
  return 0;
}

int main(void)
{
  CHECK(test_fixed_utc8_day_key_and_format() == 0);
  CHECK(test_missing_dates() == 0);
  CHECK(test_sparse_days_stay_inside_calendar_window() == 0);
  CHECK(test_session_ids_must_be_contiguous() == 0);
  CHECK(test_capacity_persistence_and_idempotence() == 0);
  CHECK(test_history_write_faults_preserve_old_complete_data() == 0);
  CHECK(test_recovered_snapshot_cannot_overwrite_newer_generation() == 0);
  CHECK(test_invalid_overflow_and_transaction_boundaries() == 0);
  puts("smart band history service tests passed");
  return 0;
}
