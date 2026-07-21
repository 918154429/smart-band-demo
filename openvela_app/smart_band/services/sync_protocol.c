#include "smart_band_sync_protocol.h"

#include <limits.h>
#include <string.h>

#define HEADER_MAGIC_OFFSET 0u
#define HEADER_MAJOR_OFFSET 2u
#define HEADER_MINOR_OFFSET 3u
#define HEADER_TYPE_OFFSET 4u
#define HEADER_FLAGS_OFFSET 5u
#define HEADER_STATUS_OFFSET 6u
#define HEADER_RESERVED_OFFSET 7u
#define HEADER_PAYLOAD_LENGTH_OFFSET 8u
#define HEADER_TRANSACTION_ID_OFFSET 10u
#define HEADER_SEQUENCE_OFFSET 14u
#define HEADER_CHUNK_INDEX_OFFSET 16u

_Static_assert(CHAR_BIT == 8, "sync protocol requires 8-bit bytes");
_Static_assert(SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE == 18u,
               "sync protocol header layout mismatch");
_Static_assert(SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE == 244u,
               "sync protocol frame size mismatch");

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

static int type_is_valid(uint8_t type)
{
  return type >= (uint8_t)SMART_BAND_SYNC_FRAME_DATA &&
         type <= (uint8_t)SMART_BAND_SYNC_FRAME_ERROR;
}

static int flags_are_valid(uint8_t flags)
{
  return (flags & (uint8_t)~SMART_BAND_SYNC_FLAG_VALID_MASK) == 0u;
}

static int status_is_valid(uint8_t status)
{
  return status <= (uint8_t)SMART_BAND_SYNC_STATUS_INTERNAL_ERROR;
}

uint16_t smart_band_sync_protocol_crc16(const void *data, size_t size)
{
  const uint8_t *bytes = data;
  uint16_t crc = UINT16_C(0xffff);
  size_t index;

  if (data == NULL && size != 0u)
    {
      return 0;
    }

  for (index = 0; index < size; index++)
    {
      unsigned int bit;

      crc ^= (uint16_t)((uint16_t)bytes[index] << 8);
      for (bit = 0; bit < 8u; bit++)
        {
          if ((crc & UINT16_C(0x8000)) != 0u)
            {
              crc = (uint16_t)((uint16_t)(crc << 1) ^ UINT16_C(0x1021));
            }
          else
            {
              crc = (uint16_t)(crc << 1);
            }
        }
    }

  return crc;
}

smart_band_sync_protocol_result_t
smart_band_sync_protocol_encode(const smart_band_sync_frame_t *frame,
                                uint8_t *output, size_t output_capacity,
                                size_t *encoded_size)
{
  size_t required_size;
  uint16_t crc;

  if (encoded_size != NULL)
    {
      *encoded_size = 0;
    }

  if (frame == NULL || output == NULL || encoded_size == NULL ||
      (frame->payload == NULL && frame->payload_length != 0u))
    {
      return SMART_BAND_SYNC_PROTOCOL_INVALID_ARGUMENT;
    }

  if (!type_is_valid(frame->type))
    {
      return SMART_BAND_SYNC_PROTOCOL_BAD_TYPE;
    }

  if (!flags_are_valid(frame->flags))
    {
      return SMART_BAND_SYNC_PROTOCOL_BAD_FLAGS;
    }

  if (!status_is_valid(frame->status))
    {
      return SMART_BAND_SYNC_PROTOCOL_BAD_STATUS;
    }

  if (frame->payload_length > SMART_BAND_SYNC_PROTOCOL_MAX_PAYLOAD_SIZE)
    {
      return SMART_BAND_SYNC_PROTOCOL_BOUNDS;
    }

  required_size = SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE +
                  frame->payload_length + SMART_BAND_SYNC_PROTOCOL_CRC_SIZE;
  *encoded_size = required_size;
  if (output_capacity < required_size)
    {
      return SMART_BAND_SYNC_PROTOCOL_BUFFER_TOO_SMALL;
    }

  if (frame->payload_length != 0u)
    {
      memmove(output + SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE, frame->payload,
              frame->payload_length);
    }

  write_le16(output + HEADER_MAGIC_OFFSET, SMART_BAND_SYNC_PROTOCOL_MAGIC);
  output[HEADER_MAJOR_OFFSET] = SMART_BAND_SYNC_PROTOCOL_MAJOR;
  output[HEADER_MINOR_OFFSET] = SMART_BAND_SYNC_PROTOCOL_MINOR;
  output[HEADER_TYPE_OFFSET] = frame->type;
  output[HEADER_FLAGS_OFFSET] = frame->flags;
  output[HEADER_STATUS_OFFSET] = frame->status;
  output[HEADER_RESERVED_OFFSET] = 0;
  write_le16(output + HEADER_PAYLOAD_LENGTH_OFFSET,
             (uint16_t)frame->payload_length);
  write_le32(output + HEADER_TRANSACTION_ID_OFFSET, frame->transaction_id);
  write_le16(output + HEADER_SEQUENCE_OFFSET, frame->sequence);
  write_le16(output + HEADER_CHUNK_INDEX_OFFSET, frame->chunk_index);

  crc = smart_band_sync_protocol_crc16(
    output, SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE + frame->payload_length);
  write_le16(output + SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE +
             frame->payload_length, crc);
  return SMART_BAND_SYNC_PROTOCOL_OK;
}

smart_band_sync_protocol_result_t
smart_band_sync_protocol_decode(const uint8_t *encoded, size_t encoded_size,
                                smart_band_sync_frame_view_t *frame)
{
  uint16_t payload_length;
  size_t required_size;
  uint16_t expected_crc;
  uint16_t actual_crc;

  if (frame != NULL)
    {
      memset(frame, 0, sizeof(*frame));
    }

  if (encoded == NULL || frame == NULL)
    {
      return SMART_BAND_SYNC_PROTOCOL_INVALID_ARGUMENT;
    }

  if (encoded_size < SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE +
                     SMART_BAND_SYNC_PROTOCOL_CRC_SIZE)
    {
      return SMART_BAND_SYNC_PROTOCOL_TRUNCATED;
    }

  if (read_le16(encoded + HEADER_MAGIC_OFFSET) !=
      SMART_BAND_SYNC_PROTOCOL_MAGIC)
    {
      return SMART_BAND_SYNC_PROTOCOL_BAD_MAGIC;
    }

  if (encoded[HEADER_MAJOR_OFFSET] != SMART_BAND_SYNC_PROTOCOL_MAJOR ||
      encoded[HEADER_MINOR_OFFSET] > SMART_BAND_SYNC_PROTOCOL_MINOR)
    {
      return SMART_BAND_SYNC_PROTOCOL_BAD_VERSION;
    }

  if (!type_is_valid(encoded[HEADER_TYPE_OFFSET]))
    {
      return SMART_BAND_SYNC_PROTOCOL_BAD_TYPE;
    }

  if (!flags_are_valid(encoded[HEADER_FLAGS_OFFSET]))
    {
      return SMART_BAND_SYNC_PROTOCOL_BAD_FLAGS;
    }

  if (!status_is_valid(encoded[HEADER_STATUS_OFFSET]))
    {
      return SMART_BAND_SYNC_PROTOCOL_BAD_STATUS;
    }

  if (encoded[HEADER_RESERVED_OFFSET] != 0u)
    {
      return SMART_BAND_SYNC_PROTOCOL_BAD_RESERVED;
    }

  payload_length = read_le16(encoded + HEADER_PAYLOAD_LENGTH_OFFSET);
  if ((size_t)payload_length > SMART_BAND_SYNC_PROTOCOL_MAX_PAYLOAD_SIZE)
    {
      return SMART_BAND_SYNC_PROTOCOL_BOUNDS;
    }

  required_size = SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE +
                  (size_t)payload_length + SMART_BAND_SYNC_PROTOCOL_CRC_SIZE;
  if (encoded_size < required_size)
    {
      return SMART_BAND_SYNC_PROTOCOL_TRUNCATED;
    }

  if (encoded_size > required_size)
    {
      return SMART_BAND_SYNC_PROTOCOL_TRAILING_DATA;
    }

  expected_crc = read_le16(encoded + required_size -
                           SMART_BAND_SYNC_PROTOCOL_CRC_SIZE);
  actual_crc = smart_band_sync_protocol_crc16(
    encoded, required_size - SMART_BAND_SYNC_PROTOCOL_CRC_SIZE);
  if (actual_crc != expected_crc)
    {
      return SMART_BAND_SYNC_PROTOCOL_BAD_CRC;
    }

  frame->type = encoded[HEADER_TYPE_OFFSET];
  frame->flags = encoded[HEADER_FLAGS_OFFSET];
  frame->status = encoded[HEADER_STATUS_OFFSET];
  frame->transaction_id = read_le32(encoded + HEADER_TRANSACTION_ID_OFFSET);
  frame->sequence = read_le16(encoded + HEADER_SEQUENCE_OFFSET);
  frame->chunk_index = read_le16(encoded + HEADER_CHUNK_INDEX_OFFSET);
  frame->payload = encoded + SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE;
  frame->payload_length = payload_length;
  return SMART_BAND_SYNC_PROTOCOL_OK;
}
