#ifndef SMART_BAND_STORE_H
#define SMART_BAND_STORE_H

#include "smart_band_storage.h"
#include "smart_band_storage_codec.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_STORE_SLOT_COUNT 2u
#define SMART_BAND_STORE_NO_SLOT 0xffu
#define SMART_BAND_STORE_FRAME_CAPACITY \
  (SMART_BAND_STORAGE_CODEC_HEADER_SIZE + \
   SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE)

typedef enum
{
  SMART_BAND_STORE_OK = 0,
  SMART_BAND_STORE_DEFAULTED = 1,
  SMART_BAND_STORE_RECOVERED = 2,
  SMART_BAND_STORE_MIGRATED = 3,
  SMART_BAND_STORE_DEGRADED = 4,
  SMART_BAND_STORE_UNAVAILABLE = 5,
  SMART_BAND_STORE_INVALID = -1,
  SMART_BAND_STORE_BUFFER_TOO_SMALL = -2,
  SMART_BAND_STORE_BACKEND_ERROR = -3,
  SMART_BAND_STORE_VERIFY_FAILED = -4,
  SMART_BAND_STORE_GENERATION_EXHAUSTED = -5
} smart_band_store_result_t;

typedef enum
{
  SMART_BAND_STORE_SLOT_MISSING = 0,
  SMART_BAND_STORE_SLOT_VALID,
  SMART_BAND_STORE_SLOT_CORRUPT,
  SMART_BAND_STORE_SLOT_UNSUPPORTED,
  SMART_BAND_STORE_SLOT_IO_ERROR,
  SMART_BAND_STORE_SLOT_UNAVAILABLE
} smart_band_store_slot_state_t;

typedef enum
{
  SMART_BAND_STORE_MIGRATION_OK = 0,
  SMART_BAND_STORE_MIGRATION_UNSUPPORTED,
  SMART_BAND_STORE_MIGRATION_BUFFER_TOO_SMALL,
  SMART_BAND_STORE_MIGRATION_INVALID
} smart_band_store_migration_result_t;

typedef smart_band_store_migration_result_t (*smart_band_store_migrate_fn)(
  void *context, uint16_t source_schema_major,
  uint16_t source_schema_minor, const uint8_t *source, size_t source_size,
  uint16_t target_schema_major, uint16_t target_schema_minor,
  uint8_t *output, size_t output_capacity, size_t *output_size);

typedef struct
{
  uint32_t slot_object_ids[SMART_BAND_STORE_SLOT_COUNT];
  uint16_t record_type;
  uint16_t schema_major;
  uint16_t schema_minor;
  const uint8_t *default_payload;
  size_t default_payload_size;
  smart_band_store_migrate_fn migrate;
  void *migrate_context;
} smart_band_store_record_spec_t;

typedef struct
{
  smart_band_storage_t backend;
  smart_band_store_result_t last_result;
  smart_band_platform_result_t last_backend_result;
  smart_band_store_slot_state_t slot_states[SMART_BAND_STORE_SLOT_COUNT];
  uint64_t generation;
  uint8_t selected_slot;
  uint8_t frame[SMART_BAND_STORE_FRAME_CAPACITY];
  bool initialized;
} smart_band_store_t;

int smart_band_store_init(smart_band_store_t *store,
                          const smart_band_storage_t *backend);
void smart_band_store_deinit(smart_band_store_t *store);
smart_band_store_result_t smart_band_store_load(
  smart_band_store_t *store, const smart_band_store_record_spec_t *spec,
  void *output, size_t output_capacity, size_t *output_size,
  uint64_t *generation);
smart_band_store_result_t smart_band_store_commit(
  smart_band_store_t *store, const smart_band_store_record_spec_t *spec,
  const void *payload, size_t payload_size, uint64_t *generation);

#ifdef __cplusplus
}
#endif

#endif
