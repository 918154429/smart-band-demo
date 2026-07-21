#ifndef SMART_BAND_SYNC_PROTOCOL_H
#define SMART_BAND_SYNC_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_SYNC_PROTOCOL_MAGIC UINT16_C(0x4253)
#define SMART_BAND_SYNC_PROTOCOL_MAJOR UINT8_C(1)
#define SMART_BAND_SYNC_PROTOCOL_MINOR UINT8_C(0)
#define SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE ((size_t)18)
#define SMART_BAND_SYNC_PROTOCOL_CRC_SIZE ((size_t)2)
#define SMART_BAND_SYNC_PROTOCOL_MAX_PAYLOAD_SIZE ((size_t)224)
#define SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE \
  (SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE + \
   SMART_BAND_SYNC_PROTOCOL_MAX_PAYLOAD_SIZE + \
   SMART_BAND_SYNC_PROTOCOL_CRC_SIZE)

#define SMART_BAND_SYNC_FLAG_MORE_CHUNKS UINT8_C(0x01)
#define SMART_BAND_SYNC_FLAG_ACK_REQUIRED UINT8_C(0x02)
#define SMART_BAND_SYNC_FLAG_VALID_MASK \
  (SMART_BAND_SYNC_FLAG_MORE_CHUNKS | SMART_BAND_SYNC_FLAG_ACK_REQUIRED)

typedef enum
{
  SMART_BAND_SYNC_FRAME_DATA = 1,
  SMART_BAND_SYNC_FRAME_CONTROL = 2,
  SMART_BAND_SYNC_FRAME_ACK = 3,
  SMART_BAND_SYNC_FRAME_ERROR = 4
} smart_band_sync_frame_type_t;

typedef enum
{
  SMART_BAND_SYNC_STATUS_OK = 0,
  SMART_BAND_SYNC_STATUS_INVALID_REQUEST = 1,
  SMART_BAND_SYNC_STATUS_UNSUPPORTED = 2,
  SMART_BAND_SYNC_STATUS_BUSY = 3,
  SMART_BAND_SYNC_STATUS_INTERNAL_ERROR = 4
} smart_band_sync_status_t;

typedef enum
{
  SMART_BAND_SYNC_PROTOCOL_OK = 0,
  SMART_BAND_SYNC_PROTOCOL_INVALID_ARGUMENT,
  SMART_BAND_SYNC_PROTOCOL_BUFFER_TOO_SMALL,
  SMART_BAND_SYNC_PROTOCOL_BOUNDS,
  SMART_BAND_SYNC_PROTOCOL_TRUNCATED,
  SMART_BAND_SYNC_PROTOCOL_TRAILING_DATA,
  SMART_BAND_SYNC_PROTOCOL_BAD_MAGIC,
  SMART_BAND_SYNC_PROTOCOL_BAD_VERSION,
  SMART_BAND_SYNC_PROTOCOL_BAD_TYPE,
  SMART_BAND_SYNC_PROTOCOL_BAD_FLAGS,
  SMART_BAND_SYNC_PROTOCOL_BAD_STATUS,
  SMART_BAND_SYNC_PROTOCOL_BAD_RESERVED,
  SMART_BAND_SYNC_PROTOCOL_BAD_CRC
} smart_band_sync_protocol_result_t;

typedef struct
{
  uint8_t type;
  uint8_t flags;
  uint8_t status;
  uint32_t transaction_id;
  uint16_t sequence;
  uint16_t chunk_index;
  const uint8_t *payload;
  size_t payload_length;
} smart_band_sync_frame_t;

typedef smart_band_sync_frame_t smart_band_sync_frame_view_t;

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xffff, no reflection/xorout. */
uint16_t smart_band_sync_protocol_crc16(const void *data, size_t size);

smart_band_sync_protocol_result_t
smart_band_sync_protocol_encode(const smart_band_sync_frame_t *frame,
                                uint8_t *output, size_t output_capacity,
                                size_t *encoded_size);

smart_band_sync_protocol_result_t
smart_band_sync_protocol_decode(const uint8_t *encoded, size_t encoded_size,
                                smart_band_sync_frame_view_t *frame);

#ifdef __cplusplus
}
#endif

#endif
