#include "smart_band_sync_service.h"

#include <limits.h>
#include <string.h>

#define CAPS_PAYLOAD_SIZE 9u
#define ACK_PAYLOAD_SIZE 3u

_Static_assert(CHAR_BIT == 8, "sync service requires 8-bit bytes");
_Static_assert(SMART_BAND_SYNC_HISTORY_MIN_MTU == 53u,
               "history message MTU contract drifted");

static uint16_t read_le16(const uint8_t *source)
{
  return (uint16_t)((uint16_t)source[0] |
                    (uint16_t)((uint16_t)source[1] << 8));
}

static uint32_t read_le32(const uint8_t *source)
{
  return (uint32_t)source[0] | ((uint32_t)source[1] << 8) |
         ((uint32_t)source[2] << 16) | ((uint32_t)source[3] << 24);
}

static int32_t read_le_i32(const uint8_t *source)
{
  uint32_t value = read_le32(source);

  return value <= (uint32_t)INT32_MAX ? (int32_t)value :
         -1 - (int32_t)(UINT32_MAX - value);
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

static void encode_daily(const smart_band_daily_summary_t *record,
                         uint8_t *output)
{
  write_le32(output, (uint32_t)record->day_key);
  write_le32(output + 4u, record->steps);
  write_le32(output + 8u, record->active_seconds);
  write_le32(output + 12u, record->calories_milli_kcal);
  write_le32(output + 16u, record->heart_weighted_bpm_seconds);
  write_le32(output + 20u, record->heart_duration_seconds);
  output[24] = record->heart_min_bpm;
  output[25] = record->heart_max_bpm;
  output[26] = record->source_flags;
  output[27] = record->flags;
}

static void decode_daily(const uint8_t *input,
                         smart_band_daily_summary_t *record)
{
  record->day_key = read_le_i32(input);
  record->steps = read_le32(input + 4u);
  record->active_seconds = read_le32(input + 8u);
  record->calories_milli_kcal = read_le32(input + 12u);
  record->heart_weighted_bpm_seconds = read_le32(input + 16u);
  record->heart_duration_seconds = read_le32(input + 20u);
  record->heart_min_bpm = input[24];
  record->heart_max_bpm = input[25];
  record->source_flags = input[26];
  record->flags = input[27];
}

static smart_band_sync_service_result_t encode_frame(
  const smart_band_sync_frame_t *frame, uint8_t *output,
  size_t output_capacity, size_t *output_size)
{
  smart_band_sync_protocol_result_t result = smart_band_sync_protocol_encode(
    frame, output, output_capacity, output_size);

  if (result == SMART_BAND_SYNC_PROTOCOL_OK)
    {
      return SMART_BAND_SYNC_SERVICE_OK;
    }
  return result == SMART_BAND_SYNC_PROTOCOL_BUFFER_TOO_SMALL ?
         SMART_BAND_SYNC_SERVICE_BUFFER_TOO_SMALL :
         SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR;
}

smart_band_sync_service_result_t smart_band_sync_encode_capabilities(
  uint32_t transaction_id, uint16_t mtu, uint8_t *output,
  size_t output_capacity, size_t *output_size)
{
  uint8_t payload[CAPS_PAYLOAD_SIZE];
  smart_band_sync_frame_t frame;

  if (mtu < SMART_BAND_SYNC_HISTORY_MIN_MTU)
    {
      return SMART_BAND_SYNC_SERVICE_UNSUPPORTED_MTU;
    }
  payload[0] = SMART_BAND_SYNC_MESSAGE_CAPABILITIES_RESPONSE;
  payload[1] = SMART_BAND_SYNC_PROTOCOL_MAJOR;
  payload[2] = SMART_BAND_SYNC_PROTOCOL_MINOR;
  write_le32(payload + 3u, SMART_BAND_SYNC_FEATURE_HISTORY);
  write_le16(payload + 7u, mtu);
  memset(&frame, 0, sizeof(frame));
  frame.type = SMART_BAND_SYNC_FRAME_CONTROL;
  frame.status = SMART_BAND_SYNC_STATUS_OK;
  frame.transaction_id = transaction_id;
  frame.payload = payload;
  frame.payload_length = sizeof(payload);
  return encode_frame(&frame, output, output_capacity, output_size);
}

smart_band_sync_service_result_t smart_band_sync_decode_capabilities(
  const uint8_t *encoded, size_t encoded_size, uint32_t transaction_id,
  smart_band_sync_capabilities_t *capabilities)
{
  smart_band_sync_frame_view_t frame;

  if (capabilities == NULL)
    {
      return SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT;
    }
  memset(capabilities, 0, sizeof(*capabilities));
  if (smart_band_sync_protocol_decode(encoded, encoded_size, &frame) !=
      SMART_BAND_SYNC_PROTOCOL_OK ||
      frame.type != SMART_BAND_SYNC_FRAME_CONTROL ||
      frame.status != SMART_BAND_SYNC_STATUS_OK ||
      frame.flags != 0u || frame.sequence != 0u || frame.chunk_index != 0u ||
      frame.payload_length != CAPS_PAYLOAD_SIZE ||
      frame.payload[0] != SMART_BAND_SYNC_MESSAGE_CAPABILITIES_RESPONSE)
    {
      return SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR;
    }
  if (frame.transaction_id != transaction_id)
    {
      return SMART_BAND_SYNC_SERVICE_WRONG_TRANSACTION;
    }
  capabilities->protocol_major = frame.payload[1];
  capabilities->protocol_minor = frame.payload[2];
  capabilities->feature_bits = read_le32(frame.payload + 3u);
  capabilities->mtu = read_le16(frame.payload + 7u);
  if (capabilities->protocol_major != SMART_BAND_SYNC_PROTOCOL_MAJOR ||
      capabilities->protocol_minor > SMART_BAND_SYNC_PROTOCOL_MINOR ||
      capabilities->mtu < SMART_BAND_SYNC_HISTORY_MIN_MTU)
    {
      memset(capabilities, 0, sizeof(*capabilities));
      return SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR;
    }
  return SMART_BAND_SYNC_SERVICE_OK;
}

smart_band_sync_service_result_t smart_band_sync_encode_history_request(
  uint32_t transaction_id, size_t resume_cursor, uint8_t *output,
  size_t output_capacity, size_t *output_size)
{
  uint8_t payload[3];
  smart_band_sync_frame_t frame;

  if (transaction_id == 0u || resume_cursor > UINT16_MAX)
    {
      return SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT;
    }
  payload[0] = SMART_BAND_SYNC_MESSAGE_HISTORY_REQUEST;
  write_le16(payload + 1u, (uint16_t)resume_cursor);
  memset(&frame, 0, sizeof(frame));
  frame.type = SMART_BAND_SYNC_FRAME_CONTROL;
  frame.status = SMART_BAND_SYNC_STATUS_OK;
  frame.transaction_id = transaction_id;
  frame.sequence = (uint16_t)resume_cursor;
  frame.chunk_index = (uint16_t)resume_cursor;
  frame.payload = payload;
  frame.payload_length = sizeof(payload);
  return encode_frame(&frame, output, output_capacity, output_size);
}

void smart_band_sync_history_server_init(
  smart_band_sync_history_server_t *server)
{
  if (server != NULL)
    {
      memset(server, 0, sizeof(*server));
      server->initialized = true;
    }
}

smart_band_sync_service_result_t smart_band_sync_history_server_begin(
  smart_band_sync_history_server_t *server,
  const smart_band_history_t *history, uint32_t transaction_id,
  size_t resume_cursor, uint16_t mtu)
{
  size_t count;

  if (server == NULL || history == NULL || transaction_id == 0u ||
      !server->initialized)
    {
      return SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT;
    }
  if (mtu < SMART_BAND_SYNC_HISTORY_MIN_MTU)
    {
      return SMART_BAND_SYNC_SERVICE_UNSUPPORTED_MTU;
    }
  count = server->active && server->transaction_id == transaction_id ?
          server->record_count : history->daily_count;
  if (count > SMART_BAND_HISTORY_DAILY_CAPACITY)
    {
      return SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR;
    }
  if (resume_cursor > count)
    {
      return SMART_BAND_SYNC_SERVICE_BAD_CURSOR;
    }
  if (server->active && server->transaction_id == transaction_id)
    {
      if (resume_cursor > server->cursor &&
          (!server->awaiting_ack || resume_cursor != server->cursor + 1u))
        {
          return SMART_BAND_SYNC_SERVICE_OUT_OF_ORDER;
        }
    }
  else
    {
      memset(server->records, 0, sizeof(server->records));
      if (count != 0u)
        {
          memcpy(server->records,
                 &history->daily[history->daily_count - count],
                 count * sizeof(server->records[0]));
        }
      server->record_count = count;
      server->transaction_id = transaction_id;
    }
  server->cursor = resume_cursor;
  server->mtu = mtu;
  server->last_sent_cursor = 0u;
  server->awaiting_ack = false;
  server->has_last_ack = resume_cursor != 0u;
  server->active = true;
  server->initialized = true;
  return resume_cursor == count ? SMART_BAND_SYNC_SERVICE_COMPLETE :
                                 SMART_BAND_SYNC_SERVICE_OK;
}

smart_band_sync_service_result_t smart_band_sync_history_server_request(
  smart_band_sync_history_server_t *server,
  const smart_band_history_t *history, const uint8_t *encoded,
  size_t encoded_size, uint16_t mtu)
{
  smart_band_sync_frame_view_t frame;
  size_t cursor;

  if (server == NULL || history == NULL || !server->initialized)
    {
      return SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT;
    }
  if (smart_band_sync_protocol_decode(encoded, encoded_size, &frame) !=
      SMART_BAND_SYNC_PROTOCOL_OK ||
      frame.type != SMART_BAND_SYNC_FRAME_CONTROL ||
      frame.status != SMART_BAND_SYNC_STATUS_OK || frame.flags != 0u ||
      frame.payload_length != 3u ||
      frame.payload[0] != SMART_BAND_SYNC_MESSAGE_HISTORY_REQUEST)
    {
      return SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR;
    }
  cursor = read_le16(frame.payload + 1u);
  if (frame.sequence != cursor || frame.chunk_index != cursor)
    {
      return SMART_BAND_SYNC_SERVICE_OUT_OF_ORDER;
    }
  return smart_band_sync_history_server_begin(
    server, history, frame.transaction_id, cursor, mtu);
}

smart_band_sync_service_result_t smart_band_sync_history_server_next(
  smart_band_sync_history_server_t *server, uint8_t *output,
  size_t output_capacity, size_t *output_size)
{
  uint8_t payload[SMART_BAND_SYNC_HISTORY_DATA_PAYLOAD_SIZE];
  smart_band_sync_frame_t frame;

  if (server == NULL || output == NULL || output_size == NULL ||
      !server->initialized || !server->active)
    {
      return SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT;
    }
  *output_size = 0u;
  if (server->cursor >= server->record_count)
    {
      return SMART_BAND_SYNC_SERVICE_COMPLETE;
    }
  payload[0] = SMART_BAND_SYNC_MESSAGE_HISTORY_DATA;
  write_le16(payload + 1u, (uint16_t)server->cursor);
  write_le16(payload + 3u, (uint16_t)server->record_count);
  encode_daily(&server->records[server->cursor], payload + 5u);
  memset(&frame, 0, sizeof(frame));
  frame.type = SMART_BAND_SYNC_FRAME_DATA;
  frame.flags = SMART_BAND_SYNC_FLAG_ACK_REQUIRED;
  if (server->cursor + 1u < server->record_count)
    {
      frame.flags |= SMART_BAND_SYNC_FLAG_MORE_CHUNKS;
    }
  frame.status = SMART_BAND_SYNC_STATUS_OK;
  frame.transaction_id = server->transaction_id;
  frame.sequence = (uint16_t)server->cursor;
  frame.chunk_index = (uint16_t)server->cursor;
  frame.payload = payload;
  frame.payload_length = sizeof(payload);
  if (output_capacity > server->mtu)
    {
      output_capacity = server->mtu;
    }
  {
    smart_band_sync_service_result_t result = encode_frame(
      &frame, output, output_capacity, output_size);

    if (result == SMART_BAND_SYNC_SERVICE_OK)
      {
        server->last_sent_cursor = server->cursor;
        server->awaiting_ack = true;
      }
    return result;
  }
}

smart_band_sync_service_result_t smart_band_sync_history_server_ack(
  smart_band_sync_history_server_t *server, const uint8_t *encoded,
  size_t encoded_size)
{
  smart_band_sync_frame_view_t frame;
  size_t next_cursor;

  if (server == NULL || !server->initialized || !server->active)
    {
      return SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT;
    }
  if (smart_band_sync_protocol_decode(encoded, encoded_size, &frame) !=
      SMART_BAND_SYNC_PROTOCOL_OK || frame.type != SMART_BAND_SYNC_FRAME_ACK ||
      frame.status != SMART_BAND_SYNC_STATUS_OK ||
      frame.flags != 0u || frame.chunk_index != frame.sequence ||
      frame.payload_length != ACK_PAYLOAD_SIZE ||
      frame.payload[0] != SMART_BAND_SYNC_MESSAGE_HISTORY_ACK)
    {
      return SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR;
    }
  if (frame.transaction_id != server->transaction_id)
    {
      return SMART_BAND_SYNC_SERVICE_WRONG_TRANSACTION;
    }
  next_cursor = read_le16(frame.payload + 1u);
  if (!server->awaiting_ack)
    {
      if (server->has_last_ack && next_cursor == server->cursor &&
          frame.sequence + 1u == next_cursor)
        {
          return SMART_BAND_SYNC_SERVICE_DUPLICATE;
        }
      return SMART_BAND_SYNC_SERVICE_OUT_OF_ORDER;
    }
  if (next_cursor != server->cursor + 1u ||
      frame.sequence != (uint16_t)server->last_sent_cursor ||
      server->last_sent_cursor != server->cursor)
    {
      return SMART_BAND_SYNC_SERVICE_OUT_OF_ORDER;
    }
  server->cursor = next_cursor;
  server->awaiting_ack = false;
  server->has_last_ack = true;
  return server->cursor == server->record_count ?
         SMART_BAND_SYNC_SERVICE_COMPLETE : SMART_BAND_SYNC_SERVICE_OK;
}

void smart_band_sync_history_client_init(
  smart_band_sync_history_client_t *client, uint32_t transaction_id)
{
  if (client != NULL)
    {
      memset(client, 0, sizeof(*client));
      client->transaction_id = transaction_id;
    }
}

smart_band_sync_service_result_t smart_band_sync_history_client_accept(
  smart_band_sync_history_client_t *client, const uint8_t *encoded,
  size_t encoded_size, uint8_t *ack_output, size_t ack_capacity,
  size_t *ack_size)
{
  smart_band_sync_frame_view_t received;
  smart_band_sync_frame_t ack;
  smart_band_daily_summary_t record;
  uint8_t payload[ACK_PAYLOAD_SIZE];
  size_t cursor;
  size_t total;
  size_t committed_cursor;
  bool duplicate;
  smart_band_sync_service_result_t result;

  if (client == NULL || ack_output == NULL || ack_size == NULL)
    {
      return SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT;
    }
  *ack_size = 0u;
  if (smart_band_sync_protocol_decode(encoded, encoded_size, &received) !=
      SMART_BAND_SYNC_PROTOCOL_OK ||
      received.type != SMART_BAND_SYNC_FRAME_DATA ||
      received.status != SMART_BAND_SYNC_STATUS_OK ||
      (received.flags & SMART_BAND_SYNC_FLAG_ACK_REQUIRED) == 0u ||
      received.payload_length != SMART_BAND_SYNC_HISTORY_DATA_PAYLOAD_SIZE ||
      received.payload[0] != SMART_BAND_SYNC_MESSAGE_HISTORY_DATA)
    {
      return SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR;
    }
  if (received.transaction_id != client->transaction_id)
    {
      return SMART_BAND_SYNC_SERVICE_WRONG_TRANSACTION;
    }
  cursor = read_le16(received.payload + 1u);
  total = read_le16(received.payload + 3u);
  if (total == 0u || total > SMART_BAND_HISTORY_DAILY_CAPACITY ||
      cursor >= total ||
      received.sequence != cursor || received.chunk_index != cursor)
    {
      return SMART_BAND_SYNC_SERVICE_BAD_CURSOR;
    }
  if (((received.flags & SMART_BAND_SYNC_FLAG_MORE_CHUNKS) != 0u) !=
      (cursor + 1u < total))
    {
      return SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR;
    }
  if (client->total_locked && total != client->expected_total)
    {
      return SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR;
    }
  decode_daily(received.payload + 5u, &record);
  duplicate = cursor < client->next_cursor;
  if (cursor > client->next_cursor ||
      (duplicate && (cursor >= client->record_count ||
                     !smart_band_sync_daily_equal(&client->records[cursor],
                                                  &record))))
    {
      return SMART_BAND_SYNC_SERVICE_OUT_OF_ORDER;
    }
  committed_cursor = duplicate ? client->next_cursor :
                                 client->next_cursor + 1u;
  payload[0] = SMART_BAND_SYNC_MESSAGE_HISTORY_ACK;
  write_le16(payload + 1u, (uint16_t)committed_cursor);
  memset(&ack, 0, sizeof(ack));
  ack.type = SMART_BAND_SYNC_FRAME_ACK;
  ack.status = SMART_BAND_SYNC_STATUS_OK;
  ack.transaction_id = client->transaction_id;
  ack.sequence = received.sequence;
  ack.chunk_index = received.chunk_index;
  ack.payload = payload;
  ack.payload_length = sizeof(payload);
  result = encode_frame(&ack, ack_output, ack_capacity, ack_size);
  if (result != SMART_BAND_SYNC_SERVICE_OK)
    {
      return result;
    }
  if (!duplicate)
    {
      client->records[cursor] = record;
      client->next_cursor = committed_cursor;
      client->record_count = committed_cursor;
      if (!client->total_locked)
        {
          client->expected_total = total;
          client->total_locked = true;
        }
    }
  return duplicate ? SMART_BAND_SYNC_SERVICE_DUPLICATE :
         (committed_cursor == total ? SMART_BAND_SYNC_SERVICE_COMPLETE :
                                        SMART_BAND_SYNC_SERVICE_OK);
}

bool smart_band_sync_daily_equal(const smart_band_daily_summary_t *left,
                                 const smart_band_daily_summary_t *right)
{
  return left != NULL && right != NULL &&
         left->day_key == right->day_key && left->steps == right->steps &&
         left->active_seconds == right->active_seconds &&
         left->calories_milli_kcal == right->calories_milli_kcal &&
         left->heart_weighted_bpm_seconds ==
           right->heart_weighted_bpm_seconds &&
         left->heart_duration_seconds == right->heart_duration_seconds &&
         left->heart_min_bpm == right->heart_min_bpm &&
         left->heart_max_bpm == right->heart_max_bpm &&
         left->source_flags == right->source_flags &&
         left->flags == right->flags;
}
