#ifndef SMART_BAND_STORAGE_CODEC_H
#define SMART_BAND_STORAGE_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The encoded bytes begin with "SBST". */
#define SMART_BAND_STORAGE_CODEC_MAGIC UINT32_C(0x54534253)
#define SMART_BAND_STORAGE_CODEC_FORMAT_MAJOR UINT8_C(1)
#define SMART_BAND_STORAGE_CODEC_FORMAT_MINOR UINT8_C(0)
#define SMART_BAND_STORAGE_CODEC_HEADER_SIZE ((size_t)36)
#define SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE ((size_t)512)
#define SMART_BAND_STORAGE_CODEC_MAX_RECORD_SIZE \
  (SMART_BAND_STORAGE_CODEC_HEADER_SIZE + \
   SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE)
#define SMART_BAND_STORAGE_CODEC_RECORD_TYPE_ANY UINT16_C(0)

/*
 * Version 1 header, in bytes (all integer fields are little-endian):
 *   0  u32 magic              16 u32 payload length
 *   4  u8  format major       20 u64 generation
 *   5  u8  format minor       28 u32 payload CRC32
 *   6  u16 header size        32 u32 header CRC32 over bytes [0, 32)
 *   8  u16 record type        36 payload
 *  10  u16 schema major
 *  12  u16 schema minor
 *  14  u16 reserved (zero)
 */

typedef enum
{
  /* Reserved format IDs; their payload schemas are defined by later slices. */
  SMART_BAND_STORAGE_RECORD_SETTINGS = 1,
  SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT = 2,
  SMART_BAND_STORAGE_RECORD_DAILY_HISTORY = 3,
  SMART_BAND_STORAGE_RECORD_WORKOUT_HISTORY = 4,
  SMART_BAND_STORAGE_RECORD_TRANSACTION_STAGE = 5,
  SMART_BAND_STORAGE_RECORD_TRANSACTION_MANIFEST = 6
} smart_band_storage_record_type_t;

typedef enum
{
  SMART_BAND_STORAGE_CODEC_OK = 0,
  SMART_BAND_STORAGE_CODEC_INVALID_ARGUMENT,
  SMART_BAND_STORAGE_CODEC_BUFFER_TOO_SMALL,
  SMART_BAND_STORAGE_CODEC_BOUNDS,
  SMART_BAND_STORAGE_CODEC_TRUNCATED,
  SMART_BAND_STORAGE_CODEC_TRAILING_DATA,
  SMART_BAND_STORAGE_CODEC_BAD_MAGIC,
  SMART_BAND_STORAGE_CODEC_UNSUPPORTED_FORMAT,
  SMART_BAND_STORAGE_CODEC_UNSUPPORTED_SCHEMA,
  SMART_BAND_STORAGE_CODEC_UNEXPECTED_RECORD_TYPE,
  SMART_BAND_STORAGE_CODEC_MALFORMED_HEADER,
  SMART_BAND_STORAGE_CODEC_BAD_HEADER_CRC,
  SMART_BAND_STORAGE_CODEC_BAD_PAYLOAD_CRC
} smart_band_storage_codec_result_t;

typedef struct
{
  uint16_t record_type;
  uint16_t schema_major;
  uint16_t schema_minor;
  uint64_t generation;
  const uint8_t *payload;
  size_t payload_length;
} smart_band_storage_record_t;

typedef struct
{
  uint16_t expected_record_type;
  uint16_t accepted_schema_major;
  uint16_t maximum_schema_minor;
} smart_band_storage_decode_options_t;

typedef struct
{
  uint16_t record_type;
  uint16_t schema_major;
  uint16_t schema_minor;
  uint64_t generation;
  const uint8_t *payload;
  size_t payload_length;
} smart_band_storage_record_view_t;

/* IEEE 802.3 CRC-32 (polynomial 0xedb88320, reflected input/output). */
uint32_t smart_band_storage_codec_crc32(const void *data, size_t size);

smart_band_storage_codec_result_t
smart_band_storage_codec_encode(const smart_band_storage_record_t *record,
                                uint8_t *output, size_t output_capacity,
                                size_t *encoded_size);

smart_band_storage_codec_result_t
smart_band_storage_codec_decode(
  const uint8_t *encoded, size_t encoded_size,
  /* NULL, or zero option fields, accepts any valid type/schema. */
  const smart_band_storage_decode_options_t *options,
  smart_band_storage_record_view_t *record);

#ifdef __cplusplus
}
#endif

#endif
