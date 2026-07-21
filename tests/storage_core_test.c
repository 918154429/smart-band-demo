#ifndef _WIN32
#  define _POSIX_C_SOURCE 200809L
#endif

#include "smart_band_storage_backend.h"
#include "smart_band_storage_codec.h"
#include "smart_band_store.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#  include <sys/stat.h>
#  include <unistd.h>
#endif

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

#define SLOT_A UINT32_C(0x51530001)
#define SLOT_B UINT32_C(0x51530002)

static const uint8_t g_defaults[] = {0xa0, 0xa1, 0xa2};

static smart_band_platform_result_t unavailable_read(
  void *context, uint32_t object_id, void *buffer, size_t capacity,
  size_t *actual_size)
{
  (void)context;
  (void)object_id;
  (void)buffer;
  (void)capacity;
  if (actual_size != NULL)
    {
      *actual_size = 0;
    }
  return SMART_BAND_PLATFORM_UNAVAILABLE;
}

static smart_band_platform_result_t unavailable_write(
  void *context, uint32_t object_id, const void *buffer, size_t size)
{
  (void)context;
  (void)object_id;
  (void)buffer;
  (void)size;
  return SMART_BAND_PLATFORM_UNAVAILABLE;
}

static smart_band_platform_result_t unavailable_flush(void *context)
{
  (void)context;
  return SMART_BAND_PLATFORM_UNAVAILABLE;
}

static const smart_band_storage_ops_t g_unavailable_ops =
{
  unavailable_read,
  unavailable_write,
  unavailable_flush,
  NULL
};

typedef struct
{
  smart_band_storage_t underlying;
  uint32_t failed_object_id;
  smart_band_platform_result_t read_failure;
  size_t reads;
  size_t writes;
  size_t flushes;
} counting_storage_t;

static smart_band_platform_result_t counting_read(
  void *opaque, uint32_t object_id, void *buffer, size_t capacity,
  size_t *actual_size)
{
  counting_storage_t *context = opaque;

  context->reads++;
  if (object_id == context->failed_object_id)
    {
      if (actual_size != NULL)
        {
          *actual_size = 0;
        }
      return context->read_failure;
    }
  return context->underlying.ops->read(
    context->underlying.context, object_id, buffer, capacity, actual_size);
}

static smart_band_platform_result_t counting_write(
  void *opaque, uint32_t object_id, const void *buffer, size_t size)
{
  counting_storage_t *context = opaque;

  context->writes++;
  return context->underlying.ops->write(
    context->underlying.context, object_id, buffer, size);
}

static smart_band_platform_result_t counting_flush(void *opaque)
{
  counting_storage_t *context = opaque;

  context->flushes++;
  return context->underlying.ops->flush(context->underlying.context);
}

static const smart_band_storage_ops_t g_counting_ops =
{
  counting_read,
  counting_write,
  counting_flush,
  NULL
};

static smart_band_store_record_spec_t make_spec(uint16_t schema_minor)
{
  smart_band_store_record_spec_t spec;

  memset(&spec, 0, sizeof(spec));
  spec.slot_object_ids[0] = SLOT_A;
  spec.slot_object_ids[1] = SLOT_B;
  spec.record_type = SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT;
  spec.schema_major = 1;
  spec.schema_minor = schema_minor;
  spec.default_payload = g_defaults;
  spec.default_payload_size = sizeof(g_defaults);
  return spec;
}

static void write_le16(uint8_t *bytes, uint16_t value)
{
  bytes[0] = (uint8_t)value;
  bytes[1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *bytes, uint32_t value)
{
  bytes[0] = (uint8_t)value;
  bytes[1] = (uint8_t)(value >> 8);
  bytes[2] = (uint8_t)(value >> 16);
  bytes[3] = (uint8_t)(value >> 24);
}

static void refresh_header_crc(uint8_t *frame)
{
  write_le32(frame + 32, smart_band_storage_codec_crc32(frame, 32));
}

static int encode_record(uint16_t schema_minor, uint64_t generation,
                         const uint8_t *payload, size_t payload_size,
                         uint8_t *frame, size_t *frame_size)
{
  smart_band_storage_record_t record;

  memset(&record, 0, sizeof(record));
  record.record_type = SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT;
  record.schema_major = 1;
  record.schema_minor = schema_minor;
  record.generation = generation;
  record.payload = payload;
  record.payload_length = payload_size;
  return smart_band_storage_codec_encode(
           &record, frame, SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE,
           frame_size) == SMART_BAND_STORAGE_CODEC_OK ? 0 : 1;
}

static int mutate_object(smart_band_storage_memory_t *memory,
                         smart_band_storage_t *storage, uint32_t object_id,
                         smart_band_storage_fault_kind_t kind,
                         size_t byte_index)
{
  uint8_t frame[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE];
  size_t actual_size = 0;

  CHECK(smart_band_storage_fault_arm(
          &memory->fault, SMART_BAND_STORAGE_OPERATION_READ, kind, 1,
          byte_index, 0x40) == SMART_BAND_PLATFORM_OK);
  CHECK(storage->ops->read(storage->context, object_id, frame, sizeof(frame),
                           &actual_size) == SMART_BAND_PLATFORM_OK);
  return 0;
}

static int test_codec_golden_vector(void)
{
  static const uint8_t payload[] = {0x10, 0x20, 0x30, 0x40};
  static const uint8_t golden[] = {
    0x53, 0x42, 0x53, 0x54, 0x01, 0x00, 0x24, 0x00,
    0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x08, 0x07, 0x06, 0x05,
    0x04, 0x03, 0x02, 0x01, 0x00, 0xb9, 0x8a, 0xe0,
    0x56, 0x8b, 0x20, 0x53, 0x10, 0x20, 0x30, 0x40
  };
  static const char crc_input[] = "123456789";
  smart_band_storage_record_t record;
  smart_band_storage_record_view_t view;
  smart_band_storage_decode_options_t options;
  uint8_t encoded[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE];
  size_t encoded_size = 0;

  CHECK(smart_band_storage_codec_crc32(crc_input, 9) ==
        UINT32_C(0xcbf43926));
  CHECK(smart_band_storage_codec_crc32(NULL, 0) == 0);
  CHECK(smart_band_storage_codec_crc32(NULL, 1) == 0);

  memset(&record, 0, sizeof(record));
  record.record_type = SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT;
  record.schema_major = 1;
  record.schema_minor = 2;
  record.generation = UINT64_C(0x0102030405060708);
  record.payload = payload;
  record.payload_length = sizeof(payload);
  CHECK(smart_band_storage_codec_encode(&record, encoded, sizeof(encoded),
                                        &encoded_size) ==
        SMART_BAND_STORAGE_CODEC_OK);
  CHECK(encoded_size == sizeof(golden));
  CHECK(memcmp(encoded, golden, sizeof(golden)) == 0);

  memset(&options, 0, sizeof(options));
  options.expected_record_type =
    SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT;
  options.accepted_schema_major = 1;
  options.maximum_schema_minor = 2;
  CHECK(smart_band_storage_codec_decode(golden, sizeof(golden), &options,
                                        &view) ==
        SMART_BAND_STORAGE_CODEC_OK);
  CHECK(view.generation == UINT64_C(0x0102030405060708));
  CHECK(view.payload_length == sizeof(payload));
  CHECK(memcmp(view.payload, payload, sizeof(payload)) == 0);
  return 0;
}

static int test_codec_rejections(void)
{
  uint8_t payload[SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE];
  uint8_t frame[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE + 1];
  uint8_t changed[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE + 1];
  smart_band_storage_record_t record;
  smart_band_storage_record_view_t view;
  smart_band_storage_decode_options_t options;
  size_t frame_size = 0;
  size_t index;

  memset(payload, 0x5a, sizeof(payload));
  memset(&record, 0, sizeof(record));
  record.record_type = SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT;
  record.schema_major = 1;
  record.payload = payload;
  record.payload_length = 4;

  CHECK(smart_band_storage_codec_encode(NULL, frame, sizeof(frame),
                                        &frame_size) ==
        SMART_BAND_STORAGE_CODEC_INVALID_ARGUMENT);
  CHECK(smart_band_storage_codec_encode(&record, NULL, sizeof(frame),
                                        &frame_size) ==
        SMART_BAND_STORAGE_CODEC_INVALID_ARGUMENT);
  CHECK(smart_band_storage_codec_encode(&record, frame, sizeof(frame),
                                        NULL) ==
        SMART_BAND_STORAGE_CODEC_INVALID_ARGUMENT);
  record.record_type = SMART_BAND_STORAGE_CODEC_RECORD_TYPE_ANY;
  CHECK(smart_band_storage_codec_encode(&record, frame, sizeof(frame),
                                        &frame_size) ==
        SMART_BAND_STORAGE_CODEC_INVALID_ARGUMENT);
  record.record_type = SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT;
  record.schema_major = 0;
  CHECK(smart_band_storage_codec_encode(&record, frame, sizeof(frame),
                                        &frame_size) ==
        SMART_BAND_STORAGE_CODEC_INVALID_ARGUMENT);
  record.schema_major = 1;
  record.payload = NULL;
  CHECK(smart_band_storage_codec_encode(&record, frame, sizeof(frame),
                                        &frame_size) ==
        SMART_BAND_STORAGE_CODEC_INVALID_ARGUMENT);
  record.payload = payload;
  record.payload_length = SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE + 1;
  CHECK(smart_band_storage_codec_encode(&record, frame, sizeof(frame),
                                        &frame_size) ==
        SMART_BAND_STORAGE_CODEC_BOUNDS);
  record.payload_length = 4;
  CHECK(smart_band_storage_codec_encode(
          &record, frame, SMART_BAND_STORAGE_CODEC_HEADER_SIZE + 3,
          &frame_size) == SMART_BAND_STORAGE_CODEC_BUFFER_TOO_SMALL);
  CHECK(frame_size == SMART_BAND_STORAGE_CODEC_HEADER_SIZE + 4);
  record.payload_length = sizeof(payload);
  CHECK(smart_band_storage_codec_encode(&record, frame, sizeof(frame),
                                        &frame_size) ==
        SMART_BAND_STORAGE_CODEC_OK);
  CHECK(frame_size == SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE);

  record.payload_length = 4;
  CHECK(smart_band_storage_codec_encode(&record, frame, sizeof(frame),
                                        &frame_size) ==
        SMART_BAND_STORAGE_CODEC_OK);
  CHECK(smart_band_storage_codec_decode(NULL, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_INVALID_ARGUMENT);
  CHECK(smart_band_storage_codec_decode(frame, frame_size, NULL, NULL) ==
        SMART_BAND_STORAGE_CODEC_INVALID_ARGUMENT);
  for (index = 0; index < SMART_BAND_STORAGE_CODEC_HEADER_SIZE; index++)
    {
      CHECK(smart_band_storage_codec_decode(frame, index, NULL, &view) ==
            SMART_BAND_STORAGE_CODEC_TRUNCATED);
    }

  memcpy(changed, frame, frame_size);
  changed[0] ^= 1;
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_BAD_MAGIC);
  memcpy(changed, frame, frame_size);
  changed[4] = 2;
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_UNSUPPORTED_FORMAT);
  memcpy(changed, frame, frame_size);
  changed[5] = 1;
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_UNSUPPORTED_FORMAT);
  memcpy(changed, frame, frame_size);
  write_le16(changed + 6, 35);
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_MALFORMED_HEADER);
  memcpy(changed, frame, frame_size);
  changed[32] ^= 1;
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_BAD_HEADER_CRC);

  memcpy(changed, frame, frame_size);
  changed[14] = 1;
  refresh_header_crc(changed);
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_MALFORMED_HEADER);
  memcpy(changed, frame, frame_size);
  write_le16(changed + 8, 0);
  refresh_header_crc(changed);
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_MALFORMED_HEADER);
  memcpy(changed, frame, frame_size);
  write_le16(changed + 10, 0);
  refresh_header_crc(changed);
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_MALFORMED_HEADER);
  memcpy(changed, frame, frame_size);
  write_le32(changed + 16,
             (uint32_t)SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE + 1u);
  refresh_header_crc(changed);
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_BOUNDS);
  memcpy(changed, frame, frame_size);
  write_le32(changed + 16, 5);
  refresh_header_crc(changed);
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_TRUNCATED);
  memcpy(changed, frame, frame_size);
  write_le32(changed + 16, 3);
  refresh_header_crc(changed);
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_TRAILING_DATA);
  memcpy(changed, frame, frame_size);
  changed[SMART_BAND_STORAGE_CODEC_HEADER_SIZE] ^= 1;
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_BAD_PAYLOAD_CRC);
  memcpy(changed, frame, frame_size);
  changed[28] ^= 1;
  refresh_header_crc(changed);
  CHECK(smart_band_storage_codec_decode(changed, frame_size, NULL, &view) ==
        SMART_BAND_STORAGE_CODEC_BAD_PAYLOAD_CRC);

  memset(&options, 0, sizeof(options));
  options.expected_record_type = SMART_BAND_STORAGE_RECORD_SETTINGS;
  CHECK(smart_band_storage_codec_decode(frame, frame_size, &options, &view) ==
        SMART_BAND_STORAGE_CODEC_UNEXPECTED_RECORD_TYPE);
  options.expected_record_type =
    SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT;
  options.accepted_schema_major = 2;
  CHECK(smart_band_storage_codec_decode(frame, frame_size, &options, &view) ==
        SMART_BAND_STORAGE_CODEC_UNSUPPORTED_SCHEMA);
  options.accepted_schema_major = 1;
  options.maximum_schema_minor = 0;
  memcpy(changed, frame, frame_size);
  write_le16(changed + 12, 1);
  refresh_header_crc(changed);
  CHECK(smart_band_storage_codec_decode(changed, frame_size, &options,
                                        &view) ==
        SMART_BAND_STORAGE_CODEC_UNSUPPORTED_SCHEMA);
  return 0;
}

static int test_memory_backend(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_fault_state_t snapshot;
  smart_band_storage_t storage;
  uint8_t input[] = {1, 2, 3, 4};
  uint8_t output[8];
  size_t actual = 99;
  size_t index;

  CHECK(smart_band_storage_memory_init(NULL, &storage) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(smart_band_storage_memory_init(&memory, NULL) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, 7, output, sizeof(output),
                          &actual) == SMART_BAND_PLATFORM_NOT_FOUND);
  CHECK(actual == 0);
  CHECK(storage.ops->write(storage.context, 7, input, sizeof(input)) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, 7, output, 3, &actual) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(actual == sizeof(input));
  CHECK(storage.ops->read(storage.context, 7, output, sizeof(output),
                          &actual) == SMART_BAND_PLATFORM_OK);
  CHECK(actual == sizeof(input));
  CHECK(memcmp(input, output, sizeof(input)) == 0);

  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_SHORT_WRITE, 2, 2, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context, 8, input, sizeof(input)) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context, 8, input, sizeof(input)) ==
        SMART_BAND_PLATFORM_IO);
  smart_band_storage_fault_snapshot(&memory.fault, &snapshot);
  CHECK(snapshot.trigger_count == 1);
  CHECK(snapshot.last_triggered == SMART_BAND_STORAGE_FAULT_SHORT_WRITE);
  CHECK(storage.ops->read(storage.context, 8, output, sizeof(output),
                          &actual) == SMART_BAND_PLATFORM_OK);
  CHECK(actual == 2);

  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_CORRUPT, 1, 1, 0x80) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, 7, output, sizeof(output),
                          &actual) == SMART_BAND_PLATFORM_OK);
  CHECK(output[1] == (uint8_t)(input[1] ^ 0x80));
  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_TRUNCATE, 1, 2, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, 7, output, sizeof(output),
                          &actual) == SMART_BAND_PLATFORM_OK);
  CHECK(actual == 2);

  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_NO_SPACE, 1, 0, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context, 9, input, sizeof(input)) ==
        SMART_BAND_PLATFORM_NO_SPACE);
  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_READ_ONLY, 1, 0, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context, 9, input, sizeof(input)) ==
        SMART_BAND_PLATFORM_READ_ONLY);
  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_FLUSH,
          SMART_BAND_STORAGE_FAULT_IO, 1, 0, 0) == SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->flush(storage.context) == SMART_BAND_PLATFORM_IO);

  CHECK(smart_band_storage_fault_arm(NULL,
          SMART_BAND_STORAGE_OPERATION_READ, SMART_BAND_STORAGE_FAULT_IO,
          1, 0, 0) == SMART_BAND_PLATFORM_INVALID);
  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_SHORT_WRITE, 1, 0, 0) ==
        SMART_BAND_PLATFORM_INVALID);
  smart_band_storage_fault_reset(&memory.fault);
  for (index = 0; index < SMART_BAND_STORAGE_MEMORY_MAX_OBJECTS; index++)
    {
      CHECK(storage.ops->write(storage.context, (uint32_t)(100 + index),
                               input, sizeof(input)) ==
            (index < SMART_BAND_STORAGE_MEMORY_MAX_OBJECTS - 2 ?
             SMART_BAND_PLATFORM_OK : SMART_BAND_PLATFORM_NO_SPACE));
    }
  smart_band_storage_memory_clear(&memory);
  CHECK(storage.ops->read(storage.context, 7, output, sizeof(output),
                          &actual) == SMART_BAND_PLATFORM_NOT_FOUND);
  return 0;
}

static smart_band_store_migration_result_t migrate_minor(
  void *context, uint16_t source_major, uint16_t source_minor,
  const uint8_t *source, size_t source_size, uint16_t target_major,
  uint16_t target_minor, uint8_t *output, size_t output_capacity,
  size_t *output_size)
{
  uint8_t suffix = context == NULL ? 0 : *(const uint8_t *)context;

  if (source_major != 1 || source_minor != 0 || target_major != 1 ||
      target_minor != 1 || source_size != 2)
    {
      return SMART_BAND_STORE_MIGRATION_UNSUPPORTED;
    }
  if (output_size == NULL)
    {
      return SMART_BAND_STORE_MIGRATION_INVALID;
    }

  *output_size = 3;
  if (output_capacity < *output_size)
    {
      return SMART_BAND_STORE_MIGRATION_BUFFER_TOO_SMALL;
    }

  memcpy(output, source, source_size);
  output[2] = suffix;
  return SMART_BAND_STORE_MIGRATION_OK;
}

static smart_band_store_migration_result_t reject_migration(
  void *context, uint16_t source_major, uint16_t source_minor,
  const uint8_t *source, size_t source_size, uint16_t target_major,
  uint16_t target_minor, uint8_t *output, size_t output_capacity,
  size_t *output_size)
{
  (void)context;
  (void)source_major;
  (void)source_minor;
  (void)source;
  (void)source_size;
  (void)target_major;
  (void)target_minor;
  (void)output;
  (void)output_capacity;
  (void)output_size;
  return SMART_BAND_STORE_MIGRATION_UNSUPPORTED;
}

static smart_band_store_migration_result_t oversized_migration(
  void *context, uint16_t source_major, uint16_t source_minor,
  const uint8_t *source, size_t source_size, uint16_t target_major,
  uint16_t target_minor, uint8_t *output, size_t output_capacity,
  size_t *output_size)
{
  (void)context;
  (void)source_major;
  (void)source_minor;
  (void)source;
  (void)source_size;
  (void)target_major;
  (void)target_minor;
  (void)output;
  *output_size = output_capacity + 1;
  return SMART_BAND_STORE_MIGRATION_OK;
}

static smart_band_store_migration_result_t invalid_migration(
  void *context, uint16_t source_major, uint16_t source_minor,
  const uint8_t *source, size_t source_size, uint16_t target_major,
  uint16_t target_minor, uint8_t *output, size_t output_capacity,
  size_t *output_size)
{
  (void)context;
  (void)source_major;
  (void)source_minor;
  (void)source;
  (void)source_size;
  (void)target_major;
  (void)target_minor;
  (void)output;
  (void)output_capacity;
  (void)output_size;
  return SMART_BAND_STORE_MIGRATION_INVALID;
}

static int test_store_selection_migration_and_corruption(void)
{
  static const uint8_t first[] = {1, 2};
  static const uint8_t second[] = {3, 4, 5};
  static const uint8_t old_schema_fixture[] = {
    0x53, 0x42, 0x53, 0x54, 0x01, 0x00, 0x24, 0x00,
    0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x92, 0x42, 0xcc, 0xb6,
    0xa4, 0xbb, 0x9d, 0x76, 0x01, 0x02
  };
  uint8_t migrated_default = 0x77;
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_store_record_spec_t spec = make_spec(0);
  smart_band_store_record_spec_t new_spec;
  uint8_t output[16];
  size_t output_size;
  uint64_t generation;

  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, &generation) ==
        SMART_BAND_STORE_DEFAULTED);
  CHECK(output_size == sizeof(g_defaults));
  CHECK(memcmp(output, g_defaults, sizeof(g_defaults)) == 0);
  CHECK(store.selected_slot == SMART_BAND_STORE_NO_SLOT);

  CHECK(smart_band_store_commit(&store, &spec, first, sizeof(first),
                                &generation) == SMART_BAND_STORE_OK);
  CHECK(generation == 1 && store.selected_slot == 0);
  CHECK(smart_band_store_commit(&store, &spec, second, sizeof(second),
                                &generation) == SMART_BAND_STORE_OK);
  CHECK(generation == 2 && store.selected_slot == 1);
  smart_band_store_deinit(&store);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, &generation) ==
        SMART_BAND_STORE_OK);
  CHECK(generation == 2 && output_size == sizeof(second));
  CHECK(memcmp(output, second, sizeof(second)) == 0);

  CHECK(mutate_object(&memory, &storage, SLOT_B,
                      SMART_BAND_STORAGE_FAULT_CORRUPT,
                      SMART_BAND_STORAGE_CODEC_HEADER_SIZE) == 0);
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, &generation) ==
        SMART_BAND_STORE_RECOVERED);
  CHECK(generation == 1 && memcmp(output, first, sizeof(first)) == 0);
  CHECK(store.slot_states[1] == SMART_BAND_STORE_SLOT_CORRUPT);
  CHECK(mutate_object(&memory, &storage, SLOT_A,
                      SMART_BAND_STORAGE_FAULT_TRUNCATE, 8) == 0);
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, &generation) ==
        SMART_BAND_STORE_DEGRADED);
  CHECK(memcmp(output, g_defaults, sizeof(g_defaults)) == 0);

  smart_band_storage_memory_clear(&memory);
  CHECK(storage.ops->write(storage.context, SLOT_A, old_schema_fixture,
                           sizeof(old_schema_fixture)) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->flush(storage.context) == SMART_BAND_PLATFORM_OK);
  new_spec = make_spec(1);
  new_spec.migrate = migrate_minor;
  new_spec.migrate_context = &migrated_default;
  CHECK(smart_band_store_load(&store, &new_spec, output, sizeof(output),
                              &output_size, &generation) ==
        SMART_BAND_STORE_MIGRATED);
  CHECK(output_size == 3 && output[0] == 1 && output[1] == 2 &&
        output[2] == migrated_default);
  CHECK(smart_band_store_commit(&store, &new_spec, output, output_size,
                                &generation) == SMART_BAND_STORE_OK);
  CHECK(generation == 2);
  CHECK(smart_band_store_load(&store, &new_spec, output, sizeof(output),
                              &output_size, NULL) == SMART_BAND_STORE_OK);
  return 0;
}

static int test_store_fault_matrix_and_crash_sweep(void)
{
  static const uint8_t stale_payload[] = {0x01, 0x02, 0x03};
  static const uint8_t old_payload[] = {0x11, 0x22, 0x33};
  static const uint8_t new_payload[] = {0x44, 0x55, 0x66};
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_store_record_spec_t spec = make_spec(0);
  uint8_t output[8];
  size_t output_size;
  uint64_t generation;
  size_t cut;
  smart_band_storage_fault_kind_t kind;

  for (kind = SMART_BAND_STORAGE_FAULT_SHORT_WRITE;
       kind <= SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION; kind++)
    {
      if (kind != SMART_BAND_STORAGE_FAULT_SHORT_WRITE &&
          kind != SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION)
        {
          continue;
        }
      for (cut = 0;
           cut <= SMART_BAND_STORAGE_CODEC_HEADER_SIZE + sizeof(new_payload);
           cut++)
        {
          CHECK(smart_band_storage_memory_init(&memory, &storage) ==
                SMART_BAND_PLATFORM_OK);
          CHECK(smart_band_store_init(&store, &storage) == 0);
          CHECK(smart_band_store_commit(&store, &spec, old_payload,
                                        sizeof(old_payload), NULL) ==
                SMART_BAND_STORE_OK);
          CHECK(smart_band_storage_fault_arm(
                  &memory.fault, SMART_BAND_STORAGE_OPERATION_WRITE, kind,
                  1, cut, 0) == SMART_BAND_PLATFORM_OK);
          CHECK(smart_band_store_commit(&store, &spec, new_payload,
                                        sizeof(new_payload), NULL) ==
                SMART_BAND_STORE_BACKEND_ERROR);
          smart_band_store_deinit(&store);
          CHECK(smart_band_store_init(&store, &storage) == 0);
          CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                                      &output_size, &generation) >=
                SMART_BAND_STORE_OK);
          CHECK((generation == 1 && output_size == sizeof(old_payload) &&
                 memcmp(output, old_payload, sizeof(old_payload)) == 0) ||
                (generation == 2 && output_size == sizeof(new_payload) &&
                 memcmp(output, new_payload, sizeof(new_payload)) == 0));
        }
    }

  /* A valid generation zero is old state and forces generation one into B. */
  for (cut = 0;
       cut <= SMART_BAND_STORAGE_CODEC_HEADER_SIZE + sizeof(new_payload);
       cut++)
    {
      uint8_t generation_zero[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE];
      size_t generation_zero_size;

      CHECK(smart_band_storage_memory_init(&memory, &storage) ==
            SMART_BAND_PLATFORM_OK);
      CHECK(encode_record(0, 0, old_payload, sizeof(old_payload),
                          generation_zero, &generation_zero_size) == 0);
      CHECK(storage.ops->write(storage.context, SLOT_A, generation_zero,
                               generation_zero_size) ==
            SMART_BAND_PLATFORM_OK);
      CHECK(storage.ops->flush(storage.context) == SMART_BAND_PLATFORM_OK);
      CHECK(smart_band_store_init(&store, &storage) == 0);
      CHECK(smart_band_storage_fault_arm(
              &memory.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
              SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION, 1, cut, 0) ==
            SMART_BAND_PLATFORM_OK);
      CHECK(smart_band_store_commit(&store, &spec, new_payload,
                                    sizeof(new_payload), NULL) ==
            SMART_BAND_STORE_BACKEND_ERROR);
      smart_band_store_deinit(&store);
      CHECK(smart_band_store_init(&store, &storage) == 0);
      CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                                  &output_size, &generation) >=
            SMART_BAND_STORE_OK);
      CHECK((generation == 0 && output_size == sizeof(old_payload) &&
             memcmp(output, old_payload, sizeof(old_payload)) == 0) ||
            (generation == 1 && output_size == sizeof(new_payload) &&
             memcmp(output, new_payload, sizeof(new_payload)) == 0));
    }

  /* Gen3 targets populated slot A, producing a new-prefix/gen1-tail image. */
  for (cut = 0;
       cut <= SMART_BAND_STORAGE_CODEC_HEADER_SIZE + sizeof(new_payload);
       cut++)
    {
      CHECK(smart_band_storage_memory_init(&memory, &storage) ==
            SMART_BAND_PLATFORM_OK);
      CHECK(smart_band_store_init(&store, &storage) == 0);
      CHECK(smart_band_store_commit(&store, &spec, stale_payload,
                                    sizeof(stale_payload), NULL) ==
            SMART_BAND_STORE_OK);
      CHECK(smart_band_store_commit(&store, &spec, old_payload,
                                    sizeof(old_payload), NULL) ==
            SMART_BAND_STORE_OK);
      CHECK(smart_band_storage_fault_arm(
              &memory.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
              SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION, 1, cut, 0) ==
            SMART_BAND_PLATFORM_OK);
      CHECK(smart_band_store_commit(&store, &spec, new_payload,
                                    sizeof(new_payload), NULL) ==
            SMART_BAND_STORE_BACKEND_ERROR);
      smart_band_store_deinit(&store);
      CHECK(smart_band_store_init(&store, &storage) == 0);
      CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                                  &output_size, &generation) >=
            SMART_BAND_STORE_OK);
      CHECK((generation == 2 && output_size == sizeof(old_payload) &&
             memcmp(output, old_payload, sizeof(old_payload)) == 0) ||
            (generation == 3 && output_size == sizeof(new_payload) &&
             memcmp(output, new_payload, sizeof(new_payload)) == 0));
    }

  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_store_commit(&store, &spec, old_payload,
                                sizeof(old_payload), NULL) ==
        SMART_BAND_STORE_OK);
  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_FLUSH,
          SMART_BAND_STORAGE_FAULT_IO, 1, 0, 0) == SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_commit(&store, &spec, new_payload,
                                sizeof(new_payload), NULL) ==
        SMART_BAND_STORE_BACKEND_ERROR);
  CHECK(store.last_backend_result == SMART_BAND_PLATFORM_IO);
  smart_band_store_deinit(&store);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, &generation) ==
        SMART_BAND_STORE_OK);
  CHECK(generation == 2 && memcmp(output, new_payload,
                                  sizeof(new_payload)) == 0);

  for (kind = SMART_BAND_STORAGE_FAULT_IO;
       kind <= SMART_BAND_STORAGE_FAULT_READ_ONLY; kind++)
    {
      smart_band_platform_result_t expected =
        kind == SMART_BAND_STORAGE_FAULT_NO_SPACE ?
          SMART_BAND_PLATFORM_NO_SPACE :
        kind == SMART_BAND_STORAGE_FAULT_READ_ONLY ?
          SMART_BAND_PLATFORM_READ_ONLY : SMART_BAND_PLATFORM_IO;
      CHECK(smart_band_storage_fault_arm(
              &memory.fault, SMART_BAND_STORAGE_OPERATION_WRITE, kind,
              1, 0, 0) == SMART_BAND_PLATFORM_OK);
      CHECK(smart_band_store_commit(&store, &spec, old_payload,
                                    sizeof(old_payload), NULL) ==
            SMART_BAND_STORE_BACKEND_ERROR);
      CHECK(store.last_backend_result == expected);
    }
  return 0;
}

static int test_store_ambiguous_and_exhausted(void)
{
  static const uint8_t left[] = {1};
  static const uint8_t right[] = {2};
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_store_record_spec_t spec = make_spec(0);
  uint8_t frame[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE];
  uint8_t output[8];
  size_t frame_size;
  size_t output_size;

  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(encode_record(0, 7, left, sizeof(left), frame, &frame_size) == 0);
  CHECK(storage.ops->write(storage.context, SLOT_A, frame, frame_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(encode_record(0, 7, right, sizeof(right), frame, &frame_size) == 0);
  CHECK(storage.ops->write(storage.context, SLOT_B, frame, frame_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, NULL) ==
        SMART_BAND_STORE_DEGRADED);
  CHECK(store.slot_states[0] == SMART_BAND_STORE_SLOT_CORRUPT);
  CHECK(store.slot_states[1] == SMART_BAND_STORE_SLOT_CORRUPT);

  smart_band_storage_memory_clear(&memory);
  CHECK(encode_record(0, UINT64_MAX, left, sizeof(left), frame,
                      &frame_size) == 0);
  CHECK(storage.ops->write(storage.context, SLOT_A, frame, frame_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_commit(&store, &spec, right, sizeof(right), NULL) ==
        SMART_BAND_STORE_GENERATION_EXHAUSTED);
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, NULL) == SMART_BAND_STORE_OK);
  CHECK(output_size == sizeof(left) && output[0] == left[0]);
  return 0;
}

static int test_store_falls_back_from_unsupported_newest(void)
{
  static const uint8_t older[] = {0x41, 0x42};
  static const uint8_t newer[] = {0x51, 0x52};
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_store_record_spec_t spec = make_spec(0);
  smart_band_storage_record_t record;
  uint8_t frame[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE];
  uint8_t output[8];
  size_t frame_size;
  size_t output_size;
  uint64_t generation;

  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(encode_record(0, 5, older, sizeof(older), frame, &frame_size) == 0);
  CHECK(storage.ops->write(storage.context, SLOT_A, frame, frame_size) ==
        SMART_BAND_PLATFORM_OK);
  memset(&record, 0, sizeof(record));
  record.record_type = SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT;
  record.schema_major = 2;
  record.schema_minor = 0;
  record.generation = 6;
  record.payload = newer;
  record.payload_length = sizeof(newer);
  CHECK(smart_band_storage_codec_encode(&record, frame, sizeof(frame),
                                        &frame_size) ==
        SMART_BAND_STORAGE_CODEC_OK);
  CHECK(storage.ops->write(storage.context, SLOT_B, frame, frame_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->flush(storage.context) == SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, &generation) ==
        SMART_BAND_STORE_RECOVERED);
  CHECK(generation == 5 && store.selected_slot == 0);
  CHECK(store.slot_states[1] == SMART_BAND_STORE_SLOT_UNSUPPORTED);
  CHECK(output_size == sizeof(older) &&
        memcmp(output, older, sizeof(older)) == 0);

  smart_band_storage_memory_clear(&memory);
  spec = make_spec(1);
  spec.migrate = reject_migration;
  CHECK(encode_record(1, 5, older, sizeof(older), frame, &frame_size) == 0);
  CHECK(storage.ops->write(storage.context, SLOT_A, frame, frame_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(encode_record(0, 6, newer, sizeof(newer), frame, &frame_size) == 0);
  CHECK(storage.ops->write(storage.context, SLOT_B, frame, frame_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->flush(storage.context) == SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, &generation) ==
        SMART_BAND_STORE_RECOVERED);
  CHECK(generation == 5 && store.selected_slot == 0);
  CHECK(store.slot_states[1] == SMART_BAND_STORE_SLOT_UNSUPPORTED);
  CHECK(output_size == sizeof(older) &&
        memcmp(output, older, sizeof(older)) == 0);
  return 0;
}

static int test_migration_error_contracts(void)
{
  static const uint8_t current_payload[] = {0x61, 0x62};
  static const uint8_t old_payload[] = {0x71, 0x72};
  uint8_t suffix = 0x73;
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_store_record_spec_t spec = make_spec(1);
  uint8_t frame[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE];
  uint8_t output[8];
  size_t frame_size;
  size_t output_size;

  spec.migrate = migrate_minor;
  spec.migrate_context = &suffix;
  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(encode_record(0, 6, old_payload, sizeof(old_payload), frame,
                      &frame_size) == 0);
  CHECK(storage.ops->write(storage.context, SLOT_B, frame, frame_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_load(&store, &spec, output, 2, &output_size,
                              NULL) == SMART_BAND_STORE_BUFFER_TOO_SMALL);
  CHECK(output_size == 3);
  CHECK(store.selected_slot == SMART_BAND_STORE_NO_SLOT);

  smart_band_storage_memory_clear(&memory);
  CHECK(encode_record(1, 5, current_payload, sizeof(current_payload), frame,
                      &frame_size) == 0);
  CHECK(storage.ops->write(storage.context, SLOT_A, frame, frame_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(encode_record(0, 6, old_payload, sizeof(old_payload), frame,
                      &frame_size) == 0);
  CHECK(storage.ops->write(storage.context, SLOT_B, frame, frame_size) ==
        SMART_BAND_PLATFORM_OK);
  memset(output, 0xee, sizeof(output));
  CHECK(smart_band_store_load(&store, &spec, output, 2, &output_size,
                              NULL) == SMART_BAND_STORE_BUFFER_TOO_SMALL);
  CHECK(output_size == 3);
  CHECK(store.selected_slot == SMART_BAND_STORE_NO_SLOT);
  CHECK(output[0] == 0xee && output[1] == 0xee);

  spec.migrate = oversized_migration;
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, NULL) ==
        SMART_BAND_STORE_INVALID);
  CHECK(output_size == sizeof(output) + 1);
  CHECK(store.selected_slot == SMART_BAND_STORE_NO_SLOT);
  spec.migrate = invalid_migration;
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, NULL) ==
        SMART_BAND_STORE_INVALID);
  CHECK(store.selected_slot == SMART_BAND_STORE_NO_SLOT);
  return 0;
}

static int test_store_invalid_and_degraded_backends(void)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_storage_t unavailable;
  smart_band_store_t store;
  smart_band_store_record_spec_t spec = make_spec(0);
  uint8_t output[8];
  size_t output_size = 99;

  memset(&unavailable, 0, sizeof(unavailable));
  unavailable.ops = &g_unavailable_ops;
  CHECK(smart_band_store_init(NULL, &unavailable) == -1);
  CHECK(smart_band_store_init(&store, NULL) == -1);
  CHECK(store.last_result == SMART_BAND_STORE_UNAVAILABLE);
  CHECK(smart_band_store_init(&store, &unavailable) == 0);
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, NULL) ==
        SMART_BAND_STORE_UNAVAILABLE);
  CHECK(output_size == sizeof(g_defaults));
  CHECK(memcmp(output, g_defaults, sizeof(g_defaults)) == 0);
  CHECK(store.slot_states[0] == SMART_BAND_STORE_SLOT_UNAVAILABLE);
  CHECK(store.slot_states[1] == SMART_BAND_STORE_SLOT_UNAVAILABLE);
  CHECK(smart_band_store_commit(&store, &spec, output, output_size, NULL) ==
        SMART_BAND_STORE_UNAVAILABLE);
  smart_band_store_deinit(&store);

  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_storage_fault_arm(
          &memory.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_IO, 1, 0, 0) == SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, NULL) ==
        SMART_BAND_STORE_DEGRADED);
  CHECK(store.slot_states[0] == SMART_BAND_STORE_SLOT_IO_ERROR);
  CHECK(store.slot_states[1] == SMART_BAND_STORE_SLOT_MISSING);
  CHECK(store.last_backend_result == SMART_BAND_PLATFORM_IO);
  CHECK(smart_band_store_load(&store, &spec, output, 2, &output_size,
                              NULL) == SMART_BAND_STORE_BUFFER_TOO_SMALL);

  spec.slot_object_ids[1] = spec.slot_object_ids[0];
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, NULL) ==
        SMART_BAND_STORE_INVALID);
  CHECK(smart_band_store_commit(&store, &spec, output, 1, NULL) ==
        SMART_BAND_STORE_INVALID);
  return 0;
}

static int test_commit_aborts_before_write_on_uncertain_reads(void)
{
  static const uint8_t slot_a_payload[] = {0x10, 0x11};
  static const uint8_t slot_b_payload[] = {0x20, 0x21};
  static const uint8_t attempted_payload[] = {0x30, 0x31};
  smart_band_storage_memory_t memory;
  smart_band_storage_t underlying;
  smart_band_storage_t wrapped;
  counting_storage_t counter;
  smart_band_store_t store;
  smart_band_store_record_spec_t spec = make_spec(0);
  uint8_t frame[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE];
  uint8_t before_a[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE];
  uint8_t before_b[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE];
  uint8_t after[SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE];
  size_t frame_size;
  size_t before_a_size;
  size_t before_b_size;
  size_t after_size;

  CHECK(smart_band_storage_memory_init(&memory, &underlying) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(encode_record(0, 10, slot_a_payload, sizeof(slot_a_payload),
                      frame, &frame_size) == 0);
  CHECK(underlying.ops->write(underlying.context, SLOT_A, frame,
                              frame_size) == SMART_BAND_PLATFORM_OK);
  CHECK(encode_record(0, 11, slot_b_payload, sizeof(slot_b_payload),
                      frame, &frame_size) == 0);
  CHECK(underlying.ops->write(underlying.context, SLOT_B, frame,
                              frame_size) == SMART_BAND_PLATFORM_OK);
  CHECK(underlying.ops->flush(underlying.context) == SMART_BAND_PLATFORM_OK);
  CHECK(underlying.ops->read(underlying.context, SLOT_A, before_a,
                             sizeof(before_a), &before_a_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(underlying.ops->read(underlying.context, SLOT_B, before_b,
                             sizeof(before_b), &before_b_size) ==
        SMART_BAND_PLATFORM_OK);

  memset(&counter, 0, sizeof(counter));
  counter.underlying = underlying;
  counter.failed_object_id = SLOT_A;
  counter.read_failure = SMART_BAND_PLATFORM_IO;
  wrapped.ops = &g_counting_ops;
  wrapped.context = &counter;
  CHECK(smart_band_store_init(&store, &wrapped) == 0);
  CHECK(smart_band_store_commit(&store, &spec, attempted_payload,
                                sizeof(attempted_payload), NULL) ==
        SMART_BAND_STORE_BACKEND_ERROR);
  CHECK(store.last_backend_result == SMART_BAND_PLATFORM_IO);
  CHECK(counter.reads == 1 && counter.writes == 0 && counter.flushes == 0);
  CHECK(underlying.ops->read(underlying.context, SLOT_A, after,
                             sizeof(after), &after_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(after_size == before_a_size &&
        memcmp(after, before_a, after_size) == 0);
  CHECK(underlying.ops->read(underlying.context, SLOT_B, after,
                             sizeof(after), &after_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(after_size == before_b_size &&
        memcmp(after, before_b, after_size) == 0);

  smart_band_store_deinit(&store);
  counter.failed_object_id = SLOT_B;
  counter.read_failure = SMART_BAND_PLATFORM_UNAVAILABLE;
  counter.reads = 0;
  counter.writes = 0;
  counter.flushes = 0;
  CHECK(smart_band_store_init(&store, &wrapped) == 0);
  CHECK(smart_band_store_commit(&store, &spec, attempted_payload,
                                sizeof(attempted_payload), NULL) ==
        SMART_BAND_STORE_UNAVAILABLE);
  CHECK(store.last_backend_result == SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(counter.reads == 2 && counter.writes == 0 && counter.flushes == 0);
  CHECK(underlying.ops->read(underlying.context, SLOT_A, after,
                             sizeof(after), &after_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(after_size == before_a_size &&
        memcmp(after, before_a, after_size) == 0);
  CHECK(underlying.ops->read(underlying.context, SLOT_B, after,
                             sizeof(after), &after_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(after_size == before_b_size &&
        memcmp(after, before_b, after_size) == 0);
  return 0;
}

static int test_file_backend(const char *directory)
{
  static const uint8_t first[] = {9, 8, 7};
  static const uint8_t second[] = {6, 5, 4, 3};
  smart_band_storage_file_t files;
  smart_band_storage_file_t broken;
  smart_band_storage_file_t missing;
  smart_band_storage_file_t reopened;
  smart_band_storage_t storage;
  smart_band_storage_t temporary_storage;
  smart_band_storage_t reopened_storage;
  smart_band_store_t store;
  smart_band_store_record_spec_t spec = make_spec(0);
  uint8_t output[16];
  char long_path[SMART_BAND_STORAGE_FILE_PATH_CAPACITY];
  char missing_path[SMART_BAND_STORAGE_FILE_PATH_CAPACITY];
  char regular_file_path[SMART_BAND_STORAGE_FILE_PATH_CAPACITY];
  size_t directory_length = strlen(directory);
  size_t output_size;
  size_t actual_size;
  size_t index;
  uint64_t generation;

  CHECK(smart_band_storage_file_init(NULL, directory, &storage) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(smart_band_storage_file_init(&files, NULL, &storage) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(smart_band_storage_file_init(&files, directory, NULL) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(smart_band_storage_file_init(&files, "", &storage) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(smart_band_storage_file_init(&files, "../bad", &storage) ==
        SMART_BAND_PLATFORM_INVALID);
  memset(long_path, 'x', sizeof(long_path));
  long_path[sizeof(long_path) - 1] = '\0';
  CHECK(smart_band_storage_file_init(&files, long_path, &storage) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(directory_length + sizeof("/missing-directory") <
        sizeof(missing_path));
  memcpy(missing_path, directory, directory_length);
  memcpy(missing_path + directory_length, "/missing-directory",
         sizeof("/missing-directory"));
  CHECK(smart_band_storage_file_init(&files, missing_path, &storage) ==
        SMART_BAND_PLATFORM_NOT_FOUND);
  CHECK(smart_band_storage_file_init(&files, directory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(NULL, 1, output, sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(storage.ops->read(storage.context, 1, output, sizeof(output), NULL) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(storage.ops->read(storage.context, 1, NULL, 1, &actual_size) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(storage.ops->write(NULL, 1, first, sizeof(first)) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(storage.ops->write(storage.context, 1, NULL, 1) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(storage.ops->write(
          storage.context, 1, first,
          SMART_BAND_STORAGE_FILE_OBJECT_CAPACITY + 1) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(storage.ops->flush(NULL) == SMART_BAND_PLATFORM_INVALID);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000001), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_NOT_FOUND);
  CHECK(storage.ops->write(storage.context, UINT32_C(0xf0000001), first,
                           sizeof(first)) == SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->flush(storage.context) == SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000001), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(actual_size == sizeof(first) &&
        memcmp(output, first, sizeof(first)) == 0);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000001), output,
                          2, &actual_size) == SMART_BAND_PLATFORM_INVALID);
  CHECK(actual_size == sizeof(first));
  CHECK(directory_length + sizeof("/object-f0000001.bin") <
        sizeof(regular_file_path));
  memcpy(regular_file_path, directory, directory_length);
  memcpy(regular_file_path + directory_length, "/object-f0000001.bin",
         sizeof("/object-f0000001.bin"));
  CHECK(smart_band_storage_file_init(&broken, regular_file_path,
                                     &temporary_storage) ==
        SMART_BAND_PLATFORM_INVALID);

  for (index = SMART_BAND_STORAGE_FAULT_IO;
       index <= SMART_BAND_STORAGE_FAULT_READ_ONLY; index++)
    {
      smart_band_platform_result_t expected =
        index == SMART_BAND_STORAGE_FAULT_NO_SPACE ?
          SMART_BAND_PLATFORM_NO_SPACE :
        index == SMART_BAND_STORAGE_FAULT_READ_ONLY ?
          SMART_BAND_PLATFORM_READ_ONLY : SMART_BAND_PLATFORM_IO;
      CHECK(smart_band_storage_fault_arm(
              &files.fault, SMART_BAND_STORAGE_OPERATION_READ,
              (smart_band_storage_fault_kind_t)index, 1, 0, 0) ==
            SMART_BAND_PLATFORM_OK);
      CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000001), output,
                              sizeof(output), &actual_size) == expected);
    }

  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_CORRUPT, 1, 1, 0x20) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000001), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(output[1] == (uint8_t)(first[1] ^ 0x20));
  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_TRUNCATE, 1, 2, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000001), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(actual_size == 2);
  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_TRUNCATE, 1, 99, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000001), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(actual_size == 2);
  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_CORRUPT, 1, 99, 0x20) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000001), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(actual_size == 2);
  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_TRUNCATE, 1, 1, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf00000fe), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_NOT_FOUND);
  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_READ,
          SMART_BAND_STORAGE_FAULT_CORRUPT, 1, 1, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf00000fd), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_NOT_FOUND);

  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_SHORT_WRITE, 1, 2, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context, UINT32_C(0xf0000002), second,
                           sizeof(second)) == SMART_BAND_PLATFORM_IO);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000002), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(actual_size == 2 && memcmp(output, second, 2) == 0);
  CHECK(storage.ops->write(storage.context, UINT32_C(0xf0000003), second,
                           sizeof(second)) == SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION, 1, 2, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context, UINT32_C(0xf0000003), first,
                           sizeof(first)) == SMART_BAND_PLATFORM_IO);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000003), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(actual_size == sizeof(second));
  CHECK(output[0] == first[0] && output[1] == first[1] &&
        output[2] == second[2] && output[3] == second[3]);

  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_TRUNCATE, 1, 2, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context, UINT32_C(0xf0000004), second,
                           sizeof(second)) == SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000004), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(actual_size == 2);
  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_TRUNCATE, 1, 99, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context, UINT32_C(0xf0000004), second,
                           sizeof(second)) == SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000004), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(actual_size == sizeof(second));
  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_CORRUPT, 1, 1, 0x80) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context, UINT32_C(0xf0000005), second,
                           sizeof(second)) == SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000005), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(output[1] == (uint8_t)(second[1] ^ 0x80));
  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_CORRUPT, 1, 99, 0x80) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context, UINT32_C(0xf0000005), second,
                           sizeof(second)) == SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->read(storage.context, UINT32_C(0xf0000005), output,
                          sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(memcmp(output, second, sizeof(second)) == 0);
  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_FLUSH,
          SMART_BAND_STORAGE_FAULT_IO, 1, 0, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->flush(storage.context) == SMART_BAND_PLATFORM_IO);
  CHECK(files.dirty_count > 0);
  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_FLUSH,
          SMART_BAND_STORAGE_FAULT_NO_SPACE, 1, 0, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->flush(storage.context) == SMART_BAND_PLATFORM_NO_SPACE);
  CHECK(files.dirty_count > 0);
  CHECK(smart_band_storage_fault_arm(
          &files.fault, SMART_BAND_STORAGE_OPERATION_FLUSH,
          SMART_BAND_STORAGE_FAULT_READ_ONLY, 1, 0, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->flush(storage.context) == SMART_BAND_PLATFORM_READ_ONLY);
  CHECK(files.dirty_count > 0);
  CHECK(storage.ops->flush(storage.context) == SMART_BAND_PLATFORM_OK);
  CHECK(files.dirty_count == 0);

  CHECK(storage.ops->write(storage.context, UINT32_C(0xe0000000), first,
                           1) == SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context, UINT32_C(0xe0000000), first,
                           1) == SMART_BAND_PLATFORM_OK);
  CHECK(files.dirty_count == 1);
  for (index = 1; index < SMART_BAND_STORAGE_FILE_MAX_DIRTY_OBJECTS; index++)
    {
      CHECK(storage.ops->write(storage.context,
                               (uint32_t)(UINT32_C(0xe0000000) + index),
                               first, 1) == SMART_BAND_PLATFORM_OK);
    }
  CHECK(files.dirty_count == SMART_BAND_STORAGE_FILE_MAX_DIRTY_OBJECTS);
  CHECK(storage.ops->write(storage.context, UINT32_C(0xe0000010), first,
                           1) == SMART_BAND_PLATFORM_NO_SPACE);
  CHECK(storage.ops->flush(storage.context) == SMART_BAND_PLATFORM_OK);

  CHECK(smart_band_storage_file_init(&broken, directory,
                                     &temporary_storage) ==
        SMART_BAND_PLATFORM_OK);
  memset(broken.base_directory, 'x', sizeof(broken.base_directory));
  broken.base_directory[sizeof(broken.base_directory) - 1] = '\0';
  CHECK(temporary_storage.ops->read(temporary_storage.context, 1, output,
                                    sizeof(output), &actual_size) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(temporary_storage.ops->write(temporary_storage.context, 1, first,
                                     sizeof(first)) ==
        SMART_BAND_PLATFORM_INVALID);
  broken.dirty_objects[0] = 1;
  broken.dirty_count = 1;
  CHECK(temporary_storage.ops->flush(temporary_storage.context) ==
        SMART_BAND_PLATFORM_INVALID);

  CHECK(smart_band_storage_file_init(&missing, directory,
                                     &temporary_storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(directory_length + sizeof("/missing-child") <
        sizeof(missing.base_directory));
  memcpy(missing.base_directory, directory, directory_length);
  memcpy(missing.base_directory + directory_length, "/missing-child",
         sizeof("/missing-child"));
  CHECK(temporary_storage.ops->write(temporary_storage.context, 1, first,
                                     sizeof(first)) ==
        SMART_BAND_PLATFORM_NOT_FOUND);
  CHECK(missing.dirty_count == 0);
  missing.dirty_objects[0] = UINT32_C(0xdeadbeef);
  missing.dirty_count = 1;
  memcpy(missing.base_directory, directory, directory_length + 1);
  CHECK(temporary_storage.ops->flush(temporary_storage.context) ==
        SMART_BAND_PLATFORM_NOT_FOUND);

#ifndef _WIN32
  {
    smart_band_storage_file_t posix_files;
    smart_band_storage_t posix_storage;
    smart_band_platform_result_t result;
    int restore_result;
    char full_link[SMART_BAND_STORAGE_FILE_PATH_CAPACITY];

    if (chmod(regular_file_path, 0000) == 0)
      {
        result = storage.ops->read(storage.context, UINT32_C(0xf0000001),
                                   output, sizeof(output), &actual_size);
        restore_result = chmod(regular_file_path, 0600);
        CHECK(restore_result == 0);
        CHECK(result == SMART_BAND_PLATFORM_OK ||
              result == SMART_BAND_PLATFORM_READ_ONLY);
      }

    if (access("/dev/full", F_OK) == 0)
      {
        CHECK(directory_length + sizeof("/object-d0000001.bin") <
              sizeof(full_link));
        memcpy(full_link, directory, directory_length);
        memcpy(full_link + directory_length, "/object-d0000001.bin",
               sizeof("/object-d0000001.bin"));
        if (symlink("/dev/full", full_link) == 0)
          {
            CHECK(smart_band_storage_file_init(&posix_files, directory,
                                               &posix_storage) ==
                  SMART_BAND_PLATFORM_OK);
            result = posix_storage.ops->write(
              posix_storage.context, UINT32_C(0xd0000001), first,
              sizeof(first));
            restore_result = unlink(full_link);
            CHECK(restore_result == 0);
            CHECK(result == SMART_BAND_PLATFORM_NO_SPACE);
          }
      }
  }
#endif

  CHECK(smart_band_store_init(&store, &storage) == 0);
  CHECK(smart_band_store_commit(&store, &spec, first, sizeof(first),
                                &generation) == SMART_BAND_STORE_OK);
  CHECK(smart_band_store_commit(&store, &spec, second, sizeof(second),
                                &generation) == SMART_BAND_STORE_OK);
  CHECK(generation == 2);
  smart_band_store_deinit(&store);
  CHECK(smart_band_storage_file_init(&reopened, directory,
                                     &reopened_storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_init(&store, &reopened_storage) == 0);
  CHECK(smart_band_store_load(&store, &spec, output, sizeof(output),
                              &output_size, &generation) ==
        SMART_BAND_STORE_OK);
  CHECK(generation == 2 && output_size == sizeof(second));
  CHECK(memcmp(output, second, sizeof(second)) == 0);

  CHECK(smart_band_storage_fault_arm(
          &reopened.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_NO_SPACE, 1, 0, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_commit(&store, &spec, first, sizeof(first), NULL) ==
        SMART_BAND_STORE_BACKEND_ERROR);
  CHECK(store.last_backend_result == SMART_BAND_PLATFORM_NO_SPACE);
  CHECK(smart_band_storage_fault_arm(
          &reopened.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
          SMART_BAND_STORAGE_FAULT_READ_ONLY, 1, 0, 0) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_store_commit(&store, &spec, first, sizeof(first), NULL) ==
        SMART_BAND_STORE_BACKEND_ERROR);
  CHECK(store.last_backend_result == SMART_BAND_PLATFORM_READ_ONLY);
  return 0;
}

int main(int argc, char **argv)
{
  CHECK(argc == 2);
  CHECK(test_codec_golden_vector() == 0);
  CHECK(test_codec_rejections() == 0);
  CHECK(test_memory_backend() == 0);
  CHECK(test_store_selection_migration_and_corruption() == 0);
  CHECK(test_store_fault_matrix_and_crash_sweep() == 0);
  CHECK(test_store_ambiguous_and_exhausted() == 0);
  CHECK(test_store_falls_back_from_unsupported_newest() == 0);
  CHECK(test_migration_error_contracts() == 0);
  CHECK(test_store_invalid_and_degraded_backends() == 0);
  CHECK(test_commit_aborts_before_write_on_uncertain_reads() == 0);
  CHECK(test_file_backend(argv[1]) == 0);
  puts("versioned storage codec, A/B store, and backend tests passed");
  return 0;
}
