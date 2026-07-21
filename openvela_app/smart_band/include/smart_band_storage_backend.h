#ifndef SMART_BAND_STORAGE_BACKEND_H
#define SMART_BAND_STORAGE_BACKEND_H

#include "smart_band_storage.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_STORAGE_MEMORY_MAX_OBJECTS 16
#define SMART_BAND_STORAGE_MEMORY_OBJECT_CAPACITY 4096
#define SMART_BAND_STORAGE_FILE_MAX_DIRTY_OBJECTS 16
#define SMART_BAND_STORAGE_FILE_OBJECT_CAPACITY 4096
#define SMART_BAND_STORAGE_FILE_PATH_CAPACITY 256

typedef enum
{
  SMART_BAND_STORAGE_OPERATION_READ = 0,
  SMART_BAND_STORAGE_OPERATION_WRITE,
  SMART_BAND_STORAGE_OPERATION_FLUSH
} smart_band_storage_operation_t;

typedef enum
{
  SMART_BAND_STORAGE_FAULT_NONE = 0,
  SMART_BAND_STORAGE_FAULT_IO,
  SMART_BAND_STORAGE_FAULT_NO_SPACE,
  SMART_BAND_STORAGE_FAULT_READ_ONLY,
  SMART_BAND_STORAGE_FAULT_SHORT_WRITE,
  SMART_BAND_STORAGE_FAULT_TRUNCATE,
  SMART_BAND_STORAGE_FAULT_CORRUPT,
  SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION
} smart_band_storage_fault_kind_t;

typedef struct
{
  bool armed;
  smart_band_storage_operation_t operation;
  smart_band_storage_fault_kind_t kind;
  uint32_t trigger_on_call;
  uint32_t matching_calls;
  uint32_t trigger_count;
  smart_band_storage_fault_kind_t last_triggered;
  size_t byte_index;
  uint8_t xor_mask;
} smart_band_storage_fault_plan_t;

typedef smart_band_storage_fault_plan_t smart_band_storage_fault_state_t;

void smart_band_storage_fault_reset(smart_band_storage_fault_plan_t *plan);

smart_band_platform_result_t smart_band_storage_fault_arm(
  smart_band_storage_fault_plan_t *plan,
  smart_band_storage_operation_t operation,
  smart_band_storage_fault_kind_t kind, uint32_t trigger_on_call,
  size_t byte_index, uint8_t xor_mask);

void smart_band_storage_fault_snapshot(
  const smart_band_storage_fault_plan_t *plan,
  smart_band_storage_fault_state_t *state);

typedef struct
{
  bool used;
  uint32_t object_id;
  size_t size;
  uint8_t data[SMART_BAND_STORAGE_MEMORY_OBJECT_CAPACITY];
} smart_band_storage_memory_object_t;

typedef struct
{
  smart_band_storage_memory_object_t
    objects[SMART_BAND_STORAGE_MEMORY_MAX_OBJECTS];
  smart_band_storage_fault_plan_t fault;
} smart_band_storage_memory_t;

smart_band_platform_result_t smart_band_storage_memory_init(
  smart_band_storage_memory_t *context, smart_band_storage_t *storage);

void smart_band_storage_memory_clear(smart_band_storage_memory_t *context);

typedef struct
{
  char base_directory[SMART_BAND_STORAGE_FILE_PATH_CAPACITY];
  uint32_t dirty_objects[SMART_BAND_STORAGE_FILE_MAX_DIRTY_OBJECTS];
  size_t dirty_count;
  smart_band_storage_fault_plan_t fault;
} smart_band_storage_file_t;

/*
 * The base directory must already exist. Each object is a numeric .bin file.
 * Individual writes are not atomic; an A/B store above this backend provides
 * recoverable commit semantics without relying on filesystem rename behavior.
 */
smart_band_platform_result_t smart_band_storage_file_init(
  smart_band_storage_file_t *context, const char *base_directory,
  smart_band_storage_t *storage);

#ifdef __cplusplus
}
#endif

#endif
