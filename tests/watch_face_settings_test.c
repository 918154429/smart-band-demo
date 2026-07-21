#include "smart_band_storage_backend.h"
#include "smart_band_storage_codec.h"
#include "smart_band_store.h"
#include "smart_band_watch_face_settings.h"

#include <stdint.h>
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

static int setup(smart_band_storage_memory_t *memory,
                 smart_band_storage_t *storage, smart_band_store_t *store)
{
  CHECK(smart_band_storage_memory_init(memory, storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(store, storage) == 0);
  return 0;
}

static int write_record(smart_band_storage_t *storage, uint32_t object_id,
                        uint16_t schema_major, uint16_t schema_minor,
                        uint64_t generation, const uint8_t *payload,
                        size_t payload_size)
{
  smart_band_storage_record_t record;
  uint8_t frame[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE];
  size_t frame_size = 0;

  memset(&record, 0, sizeof(record));
  record.record_type = SMART_BAND_STORAGE_RECORD_SETTINGS;
  record.schema_major = schema_major;
  record.schema_minor = schema_minor;
  record.generation = generation;
  record.payload = payload;
  record.payload_length = payload_size;
  CHECK(smart_band_storage_codec_encode(&record, frame, sizeof(frame),
                                        &frame_size) ==
        SMART_BAND_STORAGE_CODEC_OK);
  CHECK(storage->ops->write(storage->context, object_id, frame, frame_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage->ops->flush(storage->context) == SMART_BAND_PLATFORM_OK);
  return 0;
}

static int test_default_commit_and_reload(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_watch_face_id_t selected = SMART_BAND_WATCH_FACE_MINIMAL;
  uint64_t generation = 99;

  CHECK(setup(&memory, &storage, &store) == 0);
  CHECK(smart_band_watch_face_settings_load(&store, &selected) ==
        SMART_BAND_STORE_DEFAULTED);
  CHECK(selected == SMART_BAND_WATCH_FACE_LOTUS);

  CHECK(smart_band_watch_face_settings_commit(
          &store, SMART_BAND_WATCH_FACE_ACTIVITY, &generation) ==
        SMART_BAND_STORE_OK);
  CHECK(generation == 1);
  CHECK(smart_band_watch_face_settings_commit(
          &store, SMART_BAND_WATCH_FACE_MINIMAL, &generation) ==
        SMART_BAND_STORE_OK);
  CHECK(generation == 2);

  smart_band_store_deinit(&store);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  selected = SMART_BAND_WATCH_FACE_LOTUS;
  CHECK(smart_band_watch_face_settings_load(&store, &selected) ==
        SMART_BAND_STORE_OK);
  CHECK(selected == SMART_BAND_WATCH_FACE_MINIMAL);
  CHECK(store.generation == 2);
  return 0;
}

static int test_ab_recovery_and_corruption(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_watch_face_id_t selected;

  CHECK(setup(&memory, &storage, &store) == 0);
  CHECK(smart_band_watch_face_settings_commit(
          &store, SMART_BAND_WATCH_FACE_ACTIVITY, NULL) ==
        SMART_BAND_STORE_OK);
  CHECK(smart_band_watch_face_settings_commit(
          &store, SMART_BAND_WATCH_FACE_MINIMAL, NULL) ==
        SMART_BAND_STORE_OK);

  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_CORRUPT, 2,
          SMART_BAND_STORAGE_CODEC_HEADER_SIZE, 0x40) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_watch_face_settings_load(&store, &selected) ==
        SMART_BAND_STORE_RECOVERED);
  CHECK(selected == SMART_BAND_WATCH_FACE_ACTIVITY);
  CHECK(store.generation == 1);

  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_TRUNCATE, 1, 8, 0) ==
        SMART_BAND_PLATFORM_OK);
  selected = SMART_BAND_WATCH_FACE_MINIMAL;
  CHECK(smart_band_watch_face_settings_load(&store, &selected) ==
        SMART_BAND_STORE_DEGRADED);
  CHECK(selected == SMART_BAND_WATCH_FACE_LOTUS);
  return 0;
}

static int test_schema_migration_and_future_fallback(void)
{
  static const uint8_t legacy[] = {SMART_BAND_WATCH_FACE_ACTIVITY};
  static const uint8_t current[] = {SMART_BAND_WATCH_FACE_ACTIVITY, 0, 0, 0};
  static const uint8_t future[] = {SMART_BAND_WATCH_FACE_MINIMAL, 0, 0, 0};
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_watch_face_id_t selected;

  CHECK(setup(&memory, &storage, &store) == 0);
  CHECK(write_record(&storage, SMART_BAND_WATCH_FACE_SETTINGS_SLOT_A,
                     1, 0, 7, legacy, sizeof(legacy)) == 0);
  CHECK(smart_band_watch_face_settings_load(&store, &selected) ==
        SMART_BAND_STORE_MIGRATED);
  CHECK(selected == SMART_BAND_WATCH_FACE_ACTIVITY);

  smart_band_storage_memory_clear(&memory);
  CHECK(write_record(&storage, SMART_BAND_WATCH_FACE_SETTINGS_SLOT_A,
                     1, 1, 1, current, sizeof(current)) == 0);
  CHECK(write_record(&storage, SMART_BAND_WATCH_FACE_SETTINGS_SLOT_B,
                     2, 0, 2, future, sizeof(future)) == 0);
  CHECK(smart_band_watch_face_settings_load(&store, &selected) ==
        SMART_BAND_STORE_RECOVERED);
  CHECK(selected == SMART_BAND_WATCH_FACE_ACTIVITY);
  CHECK(store.slot_states[1] == SMART_BAND_STORE_SLOT_UNSUPPORTED);
  return 0;
}

static int test_invalid_payloads(void)
{
  static const uint8_t bad_face[] = {SMART_BAND_WATCH_FACE_COUNT, 0, 0, 0};
  static const uint8_t bad_reserved[] = {SMART_BAND_WATCH_FACE_MINIMAL,
                                         0, 1, 0};
  static const uint8_t bad_size[] = {SMART_BAND_WATCH_FACE_ACTIVITY};
  static const uint8_t bad_legacy[] = {SMART_BAND_WATCH_FACE_COUNT};
  const uint8_t *payloads[] = {bad_face, bad_reserved, bad_size, bad_legacy};
  const size_t sizes[] = {sizeof(bad_face), sizeof(bad_reserved),
                          sizeof(bad_size), sizeof(bad_legacy)};
  const uint16_t minors[] = {1, 1, 1, 0};
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_watch_face_id_t selected;
  size_t index;

  CHECK(setup(&memory, &storage, &store) == 0);
  for (index = 0; index < sizeof(payloads) / sizeof(payloads[0]); index++)
    {
      smart_band_storage_memory_clear(&memory);
      CHECK(write_record(&storage, SMART_BAND_WATCH_FACE_SETTINGS_SLOT_A,
                         1, minors[index], 1,
                         payloads[index], sizes[index]) ==
            0);
      selected = SMART_BAND_WATCH_FACE_MINIMAL;
      CHECK(smart_band_watch_face_settings_load(&store, &selected) ==
            SMART_BAND_STORE_DEGRADED);
      CHECK(selected == SMART_BAND_WATCH_FACE_LOTUS);
    }
  return 0;
}

static int test_write_flush_failures_and_invalid_arguments(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_watch_face_id_t selected = SMART_BAND_WATCH_FACE_MINIMAL;
  uint64_t generation = 55;

  CHECK(setup(&memory, &storage, &store) == 0);
  CHECK(smart_band_watch_face_settings_load(NULL, &selected) ==
        SMART_BAND_STORE_INVALID);
  CHECK(smart_band_watch_face_settings_load(&store, NULL) ==
        SMART_BAND_STORE_INVALID);
  CHECK(smart_band_watch_face_settings_commit(
          NULL, SMART_BAND_WATCH_FACE_LOTUS, NULL) ==
        SMART_BAND_STORE_INVALID);
  CHECK(smart_band_watch_face_settings_commit(
          &store, (smart_band_watch_face_id_t)SMART_BAND_WATCH_FACE_COUNT,
          NULL) == SMART_BAND_STORE_INVALID);
  CHECK(smart_band_watch_face_settings_commit(
          &store, (smart_band_watch_face_id_t)255, NULL) ==
        SMART_BAND_STORE_INVALID);

  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_NO_SPACE, 1, 0, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_watch_face_settings_commit(
          &store, SMART_BAND_WATCH_FACE_LOTUS, &generation) ==
        SMART_BAND_STORE_BACKEND_ERROR);
  CHECK(generation == 0);

  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_FLUSH,
          SMART_BAND_STORAGE_FAULT_IO, 1, 0, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_watch_face_settings_commit(
          &store, SMART_BAND_WATCH_FACE_ACTIVITY, &generation) ==
        SMART_BAND_STORE_BACKEND_ERROR);
  CHECK(generation == 0);
  return 0;
}

int main(void)
{
  CHECK(test_default_commit_and_reload() == 0);
  CHECK(test_ab_recovery_and_corruption() == 0);
  CHECK(test_schema_migration_and_future_fallback() == 0);
  CHECK(test_invalid_payloads() == 0);
  CHECK(test_write_flush_failures_and_invalid_arguments() == 0);
  return 0;
}
