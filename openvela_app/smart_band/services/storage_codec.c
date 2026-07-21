#include "smart_band_storage_codec.h"

#include <limits.h>
#include <string.h>

#define HEADER_MAGIC_OFFSET 0u
#define HEADER_FORMAT_MAJOR_OFFSET 4u
#define HEADER_FORMAT_MINOR_OFFSET 5u
#define HEADER_SIZE_OFFSET 6u
#define HEADER_RECORD_TYPE_OFFSET 8u
#define HEADER_SCHEMA_MAJOR_OFFSET 10u
#define HEADER_SCHEMA_MINOR_OFFSET 12u
#define HEADER_RESERVED_OFFSET 14u
#define HEADER_PAYLOAD_LENGTH_OFFSET 16u
#define HEADER_GENERATION_OFFSET 20u
#define HEADER_PAYLOAD_CRC_OFFSET 28u
#define HEADER_CRC_OFFSET 32u

_Static_assert(CHAR_BIT == 8, "storage codec requires 8-bit bytes");
_Static_assert(SMART_BAND_STORAGE_CODEC_HEADER_SIZE == HEADER_CRC_OFFSET + 4u,
               "storage codec header layout mismatch");

static uint16_t read_le16(const uint8_t *source)
{
  return (uint16_t)((uint16_t)source[0] |
                    (uint16_t)((uint16_t)source[1] << 8));
}

static uint32_t read_le32(const uint8_t *source)
{
  return (uint32_t)source[0] |
         ((uint32_t)source[1] << 8) |
         ((uint32_t)source[2] << 16) |
         ((uint32_t)source[3] << 24);
}

static uint64_t read_le64(const uint8_t *source)
{
  uint64_t low = read_le32(source);
  uint64_t high = read_le32(source + 4);

  return low | (high << 32);
}

static void write_le16(uint8_t *destination, uint16_t value)
{
  destination[0] = (uint8_t)value;
  destination[1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *destination, uint32_t value)
{
  destination[0] = (uint8_t)value;
  destination[1] = (uint8_t)(value >> 8);
  destination[2] = (uint8_t)(value >> 16);
  destination[3] = (uint8_t)(value >> 24);
}

static void write_le64(uint8_t *destination, uint64_t value)
{
  write_le32(destination, (uint32_t)value);
  write_le32(destination + 4, (uint32_t)(value >> 32));
}

uint32_t smart_band_storage_codec_crc32(const void *data, size_t size)
{
  const uint8_t *bytes = data;
  uint32_t crc = UINT32_C(0xffffffff);
  size_t index;

  if (data == NULL && size != 0)
    {
      return 0;
    }

  for (index = 0; index < size; index++)
    {
      unsigned int bit;

      crc ^= bytes[index];
      for (bit = 0; bit < 8u; bit++)
        {
          uint32_t mask = (uint32_t)(0u - (crc & 1u));

          crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }

  return crc ^ UINT32_C(0xffffffff);
}

smart_band_storage_codec_result_t
smart_band_storage_codec_encode(const smart_band_storage_record_t *record,
                                uint8_t *output, size_t output_capacity,
                                size_t *encoded_size)
{
  size_t required_size;
  uint32_t payload_crc;
  uint32_t header_crc;

  if (encoded_size != NULL)
    {
      *encoded_size = 0;
    }

  if (record == NULL || output == NULL || encoded_size == NULL ||
      record->record_type == SMART_BAND_STORAGE_CODEC_RECORD_TYPE_ANY ||
      record->schema_major == 0 ||
      (record->payload == NULL && record->payload_length != 0))
    {
      return SMART_BAND_STORAGE_CODEC_INVALID_ARGUMENT;
    }

  if (record->payload_length > SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE)
    {
      return SMART_BAND_STORAGE_CODEC_BOUNDS;
    }

  required_size = SMART_BAND_STORAGE_CODEC_HEADER_SIZE +
                  record->payload_length;
  *encoded_size = required_size;
  if (output_capacity < required_size)
    {
      return SMART_BAND_STORAGE_CODEC_BUFFER_TOO_SMALL;
    }

  if (record->payload_length != 0)
    {
      memmove(output + SMART_BAND_STORAGE_CODEC_HEADER_SIZE, record->payload,
              record->payload_length);
    }

  payload_crc = smart_band_storage_codec_crc32(
    output + SMART_BAND_STORAGE_CODEC_HEADER_SIZE, record->payload_length);
  memset(output, 0, SMART_BAND_STORAGE_CODEC_HEADER_SIZE);
  write_le32(output + HEADER_MAGIC_OFFSET, SMART_BAND_STORAGE_CODEC_MAGIC);
  output[HEADER_FORMAT_MAJOR_OFFSET] =
    SMART_BAND_STORAGE_CODEC_FORMAT_MAJOR;
  output[HEADER_FORMAT_MINOR_OFFSET] =
    SMART_BAND_STORAGE_CODEC_FORMAT_MINOR;
  write_le16(output + HEADER_SIZE_OFFSET,
             (uint16_t)SMART_BAND_STORAGE_CODEC_HEADER_SIZE);
  write_le16(output + HEADER_RECORD_TYPE_OFFSET, record->record_type);
  write_le16(output + HEADER_SCHEMA_MAJOR_OFFSET, record->schema_major);
  write_le16(output + HEADER_SCHEMA_MINOR_OFFSET, record->schema_minor);
  write_le32(output + HEADER_PAYLOAD_LENGTH_OFFSET,
             (uint32_t)record->payload_length);
  write_le64(output + HEADER_GENERATION_OFFSET, record->generation);
  write_le32(output + HEADER_PAYLOAD_CRC_OFFSET, payload_crc);
  header_crc = smart_band_storage_codec_crc32(output, HEADER_CRC_OFFSET);
  write_le32(output + HEADER_CRC_OFFSET, header_crc);
  return SMART_BAND_STORAGE_CODEC_OK;
}

smart_band_storage_codec_result_t
smart_band_storage_codec_decode(
  const uint8_t *encoded, size_t encoded_size,
  const smart_band_storage_decode_options_t *options,
  smart_band_storage_record_view_t *record)
{
  uint16_t record_type;
  uint16_t schema_major;
  uint16_t schema_minor;
  uint32_t payload_length;
  uint32_t expected_crc;
  size_t available_payload;

  if (record != NULL)
    {
      memset(record, 0, sizeof(*record));
    }

  if (encoded == NULL || record == NULL)
    {
      return SMART_BAND_STORAGE_CODEC_INVALID_ARGUMENT;
    }

  if (encoded_size < SMART_BAND_STORAGE_CODEC_HEADER_SIZE)
    {
      return SMART_BAND_STORAGE_CODEC_TRUNCATED;
    }

  if (read_le32(encoded + HEADER_MAGIC_OFFSET) !=
      SMART_BAND_STORAGE_CODEC_MAGIC)
    {
      return SMART_BAND_STORAGE_CODEC_BAD_MAGIC;
    }

  if (encoded[HEADER_FORMAT_MAJOR_OFFSET] !=
      SMART_BAND_STORAGE_CODEC_FORMAT_MAJOR ||
      encoded[HEADER_FORMAT_MINOR_OFFSET] >
      SMART_BAND_STORAGE_CODEC_FORMAT_MINOR)
    {
      return SMART_BAND_STORAGE_CODEC_UNSUPPORTED_FORMAT;
    }

  if (read_le16(encoded + HEADER_SIZE_OFFSET) !=
      SMART_BAND_STORAGE_CODEC_HEADER_SIZE)
    {
      return SMART_BAND_STORAGE_CODEC_MALFORMED_HEADER;
    }

  expected_crc = read_le32(encoded + HEADER_CRC_OFFSET);
  if (smart_band_storage_codec_crc32(encoded, HEADER_CRC_OFFSET) !=
      expected_crc)
    {
      return SMART_BAND_STORAGE_CODEC_BAD_HEADER_CRC;
    }

  if (read_le16(encoded + HEADER_RESERVED_OFFSET) != 0)
    {
      return SMART_BAND_STORAGE_CODEC_MALFORMED_HEADER;
    }

  record_type = read_le16(encoded + HEADER_RECORD_TYPE_OFFSET);
  schema_major = read_le16(encoded + HEADER_SCHEMA_MAJOR_OFFSET);
  schema_minor = read_le16(encoded + HEADER_SCHEMA_MINOR_OFFSET);
  if (record_type == SMART_BAND_STORAGE_CODEC_RECORD_TYPE_ANY ||
      schema_major == 0)
    {
      return SMART_BAND_STORAGE_CODEC_MALFORMED_HEADER;
    }

  payload_length = read_le32(encoded + HEADER_PAYLOAD_LENGTH_OFFSET);
  if ((size_t)payload_length > SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE)
    {
      return SMART_BAND_STORAGE_CODEC_BOUNDS;
    }

  available_payload = encoded_size - SMART_BAND_STORAGE_CODEC_HEADER_SIZE;
  if ((size_t)payload_length > available_payload)
    {
      return SMART_BAND_STORAGE_CODEC_TRUNCATED;
    }

  if ((size_t)payload_length < available_payload)
    {
      return SMART_BAND_STORAGE_CODEC_TRAILING_DATA;
    }

  expected_crc = read_le32(encoded + HEADER_PAYLOAD_CRC_OFFSET);
  if (smart_band_storage_codec_crc32(
        encoded + SMART_BAND_STORAGE_CODEC_HEADER_SIZE, payload_length) !=
      expected_crc)
    {
      return SMART_BAND_STORAGE_CODEC_BAD_PAYLOAD_CRC;
    }

  if (options != NULL &&
      options->expected_record_type !=
        SMART_BAND_STORAGE_CODEC_RECORD_TYPE_ANY &&
      record_type != options->expected_record_type)
    {
      return SMART_BAND_STORAGE_CODEC_UNEXPECTED_RECORD_TYPE;
    }

  if (options != NULL && options->accepted_schema_major != 0 &&
      (schema_major != options->accepted_schema_major ||
       schema_minor > options->maximum_schema_minor))
    {
      return SMART_BAND_STORAGE_CODEC_UNSUPPORTED_SCHEMA;
    }

  record->record_type = record_type;
  record->schema_major = schema_major;
  record->schema_minor = schema_minor;
  record->generation = read_le64(encoded + HEADER_GENERATION_OFFSET);
  record->payload = encoded + SMART_BAND_STORAGE_CODEC_HEADER_SIZE;
  record->payload_length = payload_length;
  return SMART_BAND_STORAGE_CODEC_OK;
}
