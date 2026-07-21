#include "smart_band_sync_protocol.h"

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

static const uint8_t g_payload[] = {0xde, 0xad, 0xbe, 0xef};
static const uint8_t g_golden[] = {
  0x53, 0x42, 0x01, 0x00, 0x01, 0x03, 0x00, 0x00,
  0x04, 0x00, 0x78, 0x56, 0x34, 0x12, 0x01, 0x02,
  0x03, 0x04, 0xde, 0xad, 0xbe, 0xef, 0xc8, 0xa6
};

static smart_band_sync_frame_t make_frame(const uint8_t *payload,
                                          size_t payload_length)
{
  smart_band_sync_frame_t frame;

  memset(&frame, 0, sizeof(frame));
  frame.type = SMART_BAND_SYNC_FRAME_DATA;
  frame.flags = SMART_BAND_SYNC_FLAG_MORE_CHUNKS |
                SMART_BAND_SYNC_FLAG_ACK_REQUIRED;
  frame.status = SMART_BAND_SYNC_STATUS_OK;
  frame.transaction_id = UINT32_C(0x12345678);
  frame.sequence = UINT16_C(0x0201);
  frame.chunk_index = UINT16_C(0x0403);
  frame.payload = payload;
  frame.payload_length = payload_length;
  return frame;
}

static int test_crc_and_golden_vector(void)
{
  static const char check_text[] = "123456789";
  smart_band_sync_frame_t frame = make_frame(g_payload, sizeof(g_payload));
  smart_band_sync_frame_view_t view;
  uint8_t encoded[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  size_t encoded_size = 0;

  CHECK(smart_band_sync_protocol_crc16(check_text, 9) == UINT16_C(0x29b1));
  CHECK(smart_band_sync_protocol_crc16(NULL, 0) == UINT16_C(0xffff));
  CHECK(smart_band_sync_protocol_crc16(NULL, 1) == 0);
  CHECK(smart_band_sync_protocol_encode(&frame, encoded, sizeof(encoded),
                                        &encoded_size) ==
        SMART_BAND_SYNC_PROTOCOL_OK);
  CHECK(encoded_size == sizeof(g_golden));
  CHECK(memcmp(encoded, g_golden, sizeof(g_golden)) == 0);

  memset(&view, 0xa5, sizeof(view));
  CHECK(smart_band_sync_protocol_decode(g_golden, sizeof(g_golden), &view) ==
        SMART_BAND_SYNC_PROTOCOL_OK);
  CHECK(view.type == SMART_BAND_SYNC_FRAME_DATA);
  CHECK(view.flags == (SMART_BAND_SYNC_FLAG_MORE_CHUNKS |
                       SMART_BAND_SYNC_FLAG_ACK_REQUIRED));
  CHECK(view.status == SMART_BAND_SYNC_STATUS_OK);
  CHECK(view.transaction_id == UINT32_C(0x12345678));
  CHECK(view.sequence == UINT16_C(0x0201));
  CHECK(view.chunk_index == UINT16_C(0x0403));
  CHECK(view.payload == g_golden + SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE);
  CHECK(view.payload_length == sizeof(g_payload));
  CHECK(memcmp(view.payload, g_payload, sizeof(g_payload)) == 0);
  return 0;
}

static int test_empty_and_maximum_payloads(void)
{
  uint8_t payload[SMART_BAND_SYNC_PROTOCOL_MAX_PAYLOAD_SIZE];
  uint8_t encoded[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  smart_band_sync_frame_t frame;
  smart_band_sync_frame_view_t view;
  size_t encoded_size = 0;
  size_t index;

  frame = make_frame(NULL, 0);
  frame.flags = 0;
  frame.type = SMART_BAND_SYNC_FRAME_CONTROL;
  CHECK(smart_band_sync_protocol_encode(&frame, encoded, sizeof(encoded),
                                        &encoded_size) ==
        SMART_BAND_SYNC_PROTOCOL_OK);
  CHECK(encoded_size == SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE +
                        SMART_BAND_SYNC_PROTOCOL_CRC_SIZE);
  CHECK(smart_band_sync_protocol_decode(encoded, encoded_size, &view) ==
        SMART_BAND_SYNC_PROTOCOL_OK);
  CHECK(view.payload_length == 0);
  CHECK(view.payload == encoded + SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE);

  for (index = 0; index < sizeof(payload); index++)
    {
      payload[index] = (uint8_t)(index ^ (index >> 3));
    }
  frame = make_frame(payload, sizeof(payload));
  frame.type = SMART_BAND_SYNC_FRAME_ERROR;
  frame.status = SMART_BAND_SYNC_STATUS_INTERNAL_ERROR;
  CHECK(smart_band_sync_protocol_encode(&frame, encoded, sizeof(encoded),
                                        &encoded_size) ==
        SMART_BAND_SYNC_PROTOCOL_OK);
  CHECK(encoded_size == SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE);
  CHECK(smart_band_sync_protocol_decode(encoded, encoded_size, &view) ==
        SMART_BAND_SYNC_PROTOCOL_OK);
  CHECK(view.payload_length == sizeof(payload));
  CHECK(memcmp(view.payload, payload, sizeof(payload)) == 0);
  return 0;
}

static int test_encode_errors_and_capacity(void)
{
  smart_band_sync_frame_t frame = make_frame(g_payload, sizeof(g_payload));
  uint8_t encoded[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  size_t encoded_size = 99;
  size_t capacity;
  size_t required = sizeof(g_golden);

  CHECK(smart_band_sync_protocol_encode(NULL, encoded, sizeof(encoded),
                                        &encoded_size) ==
        SMART_BAND_SYNC_PROTOCOL_INVALID_ARGUMENT);
  CHECK(encoded_size == 0);
  CHECK(smart_band_sync_protocol_encode(&frame, NULL, sizeof(encoded),
                                        &encoded_size) ==
        SMART_BAND_SYNC_PROTOCOL_INVALID_ARGUMENT);
  CHECK(smart_band_sync_protocol_encode(&frame, encoded, sizeof(encoded),
                                        NULL) ==
        SMART_BAND_SYNC_PROTOCOL_INVALID_ARGUMENT);

  frame.payload = NULL;
  CHECK(smart_band_sync_protocol_encode(&frame, encoded, sizeof(encoded),
                                        &encoded_size) ==
        SMART_BAND_SYNC_PROTOCOL_INVALID_ARGUMENT);
  frame = make_frame(g_payload, sizeof(g_payload));
  frame.type = 0;
  CHECK(smart_band_sync_protocol_encode(&frame, encoded, sizeof(encoded),
                                        &encoded_size) ==
        SMART_BAND_SYNC_PROTOCOL_BAD_TYPE);
  frame.type = 5;
  CHECK(smart_band_sync_protocol_encode(&frame, encoded, sizeof(encoded),
                                        &encoded_size) ==
        SMART_BAND_SYNC_PROTOCOL_BAD_TYPE);
  frame = make_frame(g_payload, sizeof(g_payload));
  frame.flags = UINT8_C(0x80);
  CHECK(smart_band_sync_protocol_encode(&frame, encoded, sizeof(encoded),
                                        &encoded_size) ==
        SMART_BAND_SYNC_PROTOCOL_BAD_FLAGS);
  frame = make_frame(g_payload, sizeof(g_payload));
  frame.status = 5;
  CHECK(smart_band_sync_protocol_encode(&frame, encoded, sizeof(encoded),
                                        &encoded_size) ==
        SMART_BAND_SYNC_PROTOCOL_BAD_STATUS);
  frame = make_frame(g_payload,
                     SMART_BAND_SYNC_PROTOCOL_MAX_PAYLOAD_SIZE + 1u);
  CHECK(smart_band_sync_protocol_encode(&frame, encoded, sizeof(encoded),
                                        &encoded_size) ==
        SMART_BAND_SYNC_PROTOCOL_BOUNDS);

  frame = make_frame(g_payload, sizeof(g_payload));
  for (capacity = 0; capacity < required; capacity++)
    {
      memset(encoded, 0xa5, sizeof(encoded));
      encoded_size = 0;
      CHECK(smart_band_sync_protocol_encode(&frame, encoded, capacity,
                                            &encoded_size) ==
            SMART_BAND_SYNC_PROTOCOL_BUFFER_TOO_SMALL);
      CHECK(encoded_size == required);
      CHECK(encoded[0] == 0xa5);
    }
  return 0;
}

static int test_decode_errors_and_lengths(void)
{
  smart_band_sync_frame_view_t view;
  uint8_t mutated[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE + 1u];
  size_t prefix;

  memset(&view, 0xa5, sizeof(view));
  CHECK(smart_band_sync_protocol_decode(NULL, 0, &view) ==
        SMART_BAND_SYNC_PROTOCOL_INVALID_ARGUMENT);
  CHECK(view.payload == NULL && view.payload_length == 0);
  CHECK(smart_band_sync_protocol_decode(g_golden, sizeof(g_golden), NULL) ==
        SMART_BAND_SYNC_PROTOCOL_INVALID_ARGUMENT);

  for (prefix = 0; prefix < sizeof(g_golden); prefix++)
    {
      CHECK(smart_band_sync_protocol_decode(g_golden, prefix, &view) ==
            SMART_BAND_SYNC_PROTOCOL_TRUNCATED);
      CHECK(view.payload == NULL && view.payload_length == 0);
    }

  memcpy(mutated, g_golden, sizeof(g_golden));
  mutated[sizeof(g_golden)] = 0;
  CHECK(smart_band_sync_protocol_decode(mutated, sizeof(g_golden) + 1u,
                                        &view) ==
        SMART_BAND_SYNC_PROTOCOL_TRAILING_DATA);

  memcpy(mutated, g_golden, sizeof(g_golden));
  mutated[8] = 3;
  mutated[9] = 0;
  CHECK(smart_band_sync_protocol_decode(mutated, sizeof(g_golden), &view) ==
        SMART_BAND_SYNC_PROTOCOL_TRAILING_DATA);
  memcpy(mutated, g_golden, sizeof(g_golden));
  mutated[8] = 5;
  mutated[9] = 0;
  CHECK(smart_band_sync_protocol_decode(mutated, sizeof(g_golden), &view) ==
        SMART_BAND_SYNC_PROTOCOL_TRUNCATED);
  memcpy(mutated, g_golden, sizeof(g_golden));
  mutated[8] = 0xe1;
  mutated[9] = 0;
  CHECK(smart_band_sync_protocol_decode(mutated, sizeof(g_golden), &view) ==
        SMART_BAND_SYNC_PROTOCOL_BOUNDS);
  return 0;
}

static int test_decode_semantic_errors(void)
{
  smart_band_sync_frame_view_t view;
  uint8_t mutated[sizeof(g_golden)];

#define CHECK_MUTATION(offset, value, expected)                              \
  do                                                                         \
    {                                                                        \
      memcpy(mutated, g_golden, sizeof(mutated));                            \
      mutated[(offset)] = (value);                                           \
      CHECK(smart_band_sync_protocol_decode(mutated, sizeof(mutated),         \
                                            &view) == (expected));            \
    }                                                                        \
  while (0)

  CHECK_MUTATION(0, 0, SMART_BAND_SYNC_PROTOCOL_BAD_MAGIC);
  CHECK_MUTATION(1, 0, SMART_BAND_SYNC_PROTOCOL_BAD_MAGIC);
  CHECK_MUTATION(2, 2, SMART_BAND_SYNC_PROTOCOL_BAD_VERSION);
  CHECK_MUTATION(3, 1, SMART_BAND_SYNC_PROTOCOL_BAD_VERSION);
  CHECK_MUTATION(4, 0, SMART_BAND_SYNC_PROTOCOL_BAD_TYPE);
  CHECK_MUTATION(4, 5, SMART_BAND_SYNC_PROTOCOL_BAD_TYPE);
  CHECK_MUTATION(5, 0x80, SMART_BAND_SYNC_PROTOCOL_BAD_FLAGS);
  CHECK_MUTATION(6, 5, SMART_BAND_SYNC_PROTOCOL_BAD_STATUS);
  CHECK_MUTATION(7, 1, SMART_BAND_SYNC_PROTOCOL_BAD_RESERVED);
  CHECK_MUTATION(10, 0, SMART_BAND_SYNC_PROTOCOL_BAD_CRC);
  CHECK_MUTATION(18, 0, SMART_BAND_SYNC_PROTOCOL_BAD_CRC);
  CHECK_MUTATION(22, 0, SMART_BAND_SYNC_PROTOCOL_BAD_CRC);

#undef CHECK_MUTATION
  return 0;
}

static int test_every_byte_corruption(void)
{
  smart_band_sync_frame_view_t view;
  uint8_t mutated[sizeof(g_golden)];
  size_t index;

  for (index = 0; index < sizeof(g_golden); index++)
    {
      memcpy(mutated, g_golden, sizeof(mutated));
      mutated[index] ^= UINT8_C(0x80);
      CHECK(smart_band_sync_protocol_decode(mutated, sizeof(mutated), &view) !=
            SMART_BAND_SYNC_PROTOCOL_OK);
    }
  return 0;
}

static int test_aliasing_and_zero_copy_view(void)
{
  uint8_t buffer[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  uint8_t original[32];
  smart_band_sync_frame_t frame;
  smart_band_sync_frame_view_t view;
  size_t encoded_size = 0;
  size_t index;

  for (index = 0; index < sizeof(original); index++)
    {
      original[index] = (uint8_t)(0xc0u + index);
      buffer[index] = original[index];
    }
  frame = make_frame(buffer, sizeof(original));
  frame.type = SMART_BAND_SYNC_FRAME_ACK;
  frame.status = SMART_BAND_SYNC_STATUS_BUSY;
  CHECK(smart_band_sync_protocol_encode(&frame, buffer, sizeof(buffer),
                                        &encoded_size) ==
        SMART_BAND_SYNC_PROTOCOL_OK);
  CHECK(smart_band_sync_protocol_decode(buffer, encoded_size, &view) ==
        SMART_BAND_SYNC_PROTOCOL_OK);
  CHECK(view.payload == buffer + SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE);
  CHECK(view.payload_length == sizeof(original));
  CHECK(memcmp(view.payload, original, sizeof(original)) == 0);
  return 0;
}

static uint32_t next_random(uint32_t *state)
{
  uint32_t value = *state;

  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  *state = value;
  return value;
}

static int test_deterministic_malformed_frames(void)
{
  smart_band_sync_frame_view_t view;
  uint8_t mutated[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE + 1u];
  uint32_t state = UINT32_C(0x6d2b79f5);
  size_t iteration;

  for (iteration = 0; iteration < 10000u; iteration++)
    {
      uint32_t random = next_random(&state);
      size_t size = sizeof(g_golden);

      memcpy(mutated, g_golden, sizeof(g_golden));
      switch (random % 10u)
        {
          case 0:
            size = next_random(&state) % sizeof(g_golden);
            break;
          case 1:
            mutated[size++] = (uint8_t)next_random(&state);
            break;
          case 2:
            {
              size_t index = next_random(&state) % sizeof(g_golden);
              uint8_t mask = (uint8_t)(next_random(&state) | 1u);
              mutated[index] ^= mask;
            }
            break;
          case 3:
            mutated[0] = (uint8_t)(next_random(&state) ^ UINT8_C(0x53));
            if (mutated[0] == UINT8_C(0x53))
              {
                mutated[0] = 0;
              }
            break;
          case 4:
            mutated[2] = (uint8_t)(2u + (next_random(&state) & 0x7fu));
            break;
          case 5:
            mutated[3] = (uint8_t)(1u + (next_random(&state) & 0x7fu));
            break;
          case 6:
            mutated[4] = 0;
            break;
          case 7:
            mutated[5] |= UINT8_C(0x80);
            break;
          case 8:
            mutated[6] = UINT8_C(0xff);
            break;
          default:
            mutated[7] = (uint8_t)(1u + (next_random(&state) & 0x7fu));
            break;
        }
      CHECK(smart_band_sync_protocol_decode(mutated, size, &view) !=
            SMART_BAND_SYNC_PROTOCOL_OK);
      CHECK(view.payload == NULL && view.payload_length == 0);
    }
  return 0;
}

int main(void)
{
  CHECK(test_crc_and_golden_vector() == 0);
  CHECK(test_empty_and_maximum_payloads() == 0);
  CHECK(test_encode_errors_and_capacity() == 0);
  CHECK(test_decode_errors_and_lengths() == 0);
  CHECK(test_decode_semantic_errors() == 0);
  CHECK(test_every_byte_corruption() == 0);
  CHECK(test_aliasing_and_zero_copy_view() == 0);
  CHECK(test_deterministic_malformed_frames() == 0);
  puts("sync protocol v1 envelope tests passed (10000 malformed frames)");
  return 0;
}
