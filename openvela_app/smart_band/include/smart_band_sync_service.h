#ifndef SMART_BAND_SYNC_SERVICE_H
#define SMART_BAND_SYNC_SERVICE_H

#include "smart_band_history.h"
#include "smart_band_sync_protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_SYNC_FEATURE_HISTORY UINT32_C(0x00000001)
#define SMART_BAND_SYNC_HISTORY_RECORD_SIZE ((size_t)28)
#define SMART_BAND_SYNC_HISTORY_DATA_PAYLOAD_SIZE \
  ((size_t)5 + SMART_BAND_SYNC_HISTORY_RECORD_SIZE)
#define SMART_BAND_SYNC_HISTORY_MIN_MTU \
  (SMART_BAND_SYNC_PROTOCOL_HEADER_SIZE + \
   SMART_BAND_SYNC_HISTORY_DATA_PAYLOAD_SIZE + \
   SMART_BAND_SYNC_PROTOCOL_CRC_SIZE)

typedef enum
{
  SMART_BAND_SYNC_MESSAGE_CAPABILITIES_REQUEST = 1,
  SMART_BAND_SYNC_MESSAGE_CAPABILITIES_RESPONSE = 2,
  SMART_BAND_SYNC_MESSAGE_HISTORY_REQUEST = 3,
  SMART_BAND_SYNC_MESSAGE_HISTORY_DATA = 4,
  SMART_BAND_SYNC_MESSAGE_HISTORY_ACK = 5
} smart_band_sync_message_type_t;

typedef enum
{
  SMART_BAND_SYNC_SERVICE_OK = 0,
  SMART_BAND_SYNC_SERVICE_COMPLETE,
  SMART_BAND_SYNC_SERVICE_DUPLICATE,
  SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT,
  SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR,
  SMART_BAND_SYNC_SERVICE_WRONG_TRANSACTION,
  SMART_BAND_SYNC_SERVICE_OUT_OF_ORDER,
  SMART_BAND_SYNC_SERVICE_BAD_CURSOR,
  SMART_BAND_SYNC_SERVICE_UNSUPPORTED_MTU,
  SMART_BAND_SYNC_SERVICE_BUFFER_TOO_SMALL
} smart_band_sync_service_result_t;

typedef struct
{
  uint8_t protocol_major;
  uint8_t protocol_minor;
  uint32_t feature_bits;
  uint16_t mtu;
} smart_band_sync_capabilities_t;

typedef struct
{
  smart_band_daily_summary_t records[SMART_BAND_HISTORY_DAILY_CAPACITY];
  size_t record_count;
  size_t cursor;
  uint32_t transaction_id;
  uint16_t mtu;
  size_t last_sent_cursor;
  bool awaiting_ack;
  bool has_last_ack;
  bool active;
  bool initialized;
} smart_band_sync_history_server_t;

typedef struct
{
  smart_band_daily_summary_t records[SMART_BAND_HISTORY_DAILY_CAPACITY];
  size_t record_count;
  size_t next_cursor;
  size_t expected_total;
  uint32_t transaction_id;
  bool total_locked;
} smart_band_sync_history_client_t;

smart_band_sync_service_result_t smart_band_sync_encode_capabilities(
  uint32_t transaction_id, uint16_t mtu, uint8_t *output,
  size_t output_capacity, size_t *output_size);
smart_band_sync_service_result_t smart_band_sync_decode_capabilities(
  const uint8_t *encoded, size_t encoded_size, uint32_t transaction_id,
  smart_band_sync_capabilities_t *capabilities);
smart_band_sync_service_result_t smart_band_sync_encode_history_request(
  uint32_t transaction_id, size_t resume_cursor, uint8_t *output,
  size_t output_capacity, size_t *output_size);

void smart_band_sync_history_server_init(
  smart_band_sync_history_server_t *server);
/* A transaction owns an immutable snapshot. Reusing its transaction ID resumes
 * that snapshot; callers must choose a new ID to observe newer live history. */
smart_band_sync_service_result_t smart_band_sync_history_server_begin(
  smart_band_sync_history_server_t *server,
  const smart_band_history_t *history, uint32_t transaction_id,
  size_t resume_cursor, uint16_t mtu);
smart_band_sync_service_result_t smart_band_sync_history_server_request(
  smart_band_sync_history_server_t *server,
  const smart_band_history_t *history, const uint8_t *encoded,
  size_t encoded_size, uint16_t mtu);
smart_band_sync_service_result_t smart_band_sync_history_server_next(
  smart_band_sync_history_server_t *server, uint8_t *output,
  size_t output_capacity, size_t *output_size);
smart_band_sync_service_result_t smart_band_sync_history_server_ack(
  smart_band_sync_history_server_t *server, const uint8_t *encoded,
  size_t encoded_size);

void smart_band_sync_history_client_init(
  smart_band_sync_history_client_t *client, uint32_t transaction_id);
smart_band_sync_service_result_t smart_band_sync_history_client_accept(
  smart_band_sync_history_client_t *client, const uint8_t *encoded,
  size_t encoded_size, uint8_t *ack_output, size_t ack_capacity,
  size_t *ack_size);

bool smart_band_sync_daily_equal(const smart_band_daily_summary_t *left,
                                 const smart_band_daily_summary_t *right);

#ifdef __cplusplus
}
#endif

#endif
