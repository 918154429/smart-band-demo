#include "smart_band_sync_loopback.h"
#include "smart_band_sync_service.h"

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

typedef struct
{
  size_t calls;
} event_counter_t;

static const uint8_t g_history_request_golden[] = {
  0x53, 0x42, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x03, 0x00, 0xd4, 0xc3, 0xb2, 0xa1, 0x00, 0x00,
  0x00, 0x00, 0x03, 0x00, 0x00, 0x75, 0xd1
};

static const uint8_t g_history_data_golden[] = {
  0x53, 0x42, 0x01, 0x00, 0x01, 0x03, 0x00, 0x00,
  0x21, 0x00, 0xd4, 0xc3, 0xb2, 0xa1, 0x00, 0x00,
  0x00, 0x00, 0x04, 0x00, 0x00, 0x07, 0x00, 0x5d,
  0x27, 0x35, 0x01, 0x10, 0x27, 0x00, 0x00, 0x08,
  0x07, 0x00, 0x00, 0x60, 0x5b, 0x03, 0x00, 0x38,
  0xc7, 0x00, 0x00, 0x58, 0x02, 0x00, 0x00, 0x37,
  0x82, 0x03, 0x03, 0x3a, 0xe4
};

static const uint8_t g_history_ack_golden[] = {
  0x53, 0x42, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x03, 0x00, 0xd4, 0xc3, 0xb2, 0xa1, 0x00, 0x00,
  0x00, 0x00, 0x05, 0x01, 0x00, 0x06, 0x40
};

static void refresh_crc(uint8_t *frame, size_t size)
{
  uint16_t crc = smart_band_sync_protocol_crc16(
    frame, size - SMART_BAND_SYNC_PROTOCOL_CRC_SIZE);

  frame[size - 2u] = (uint8_t)crc;
  frame[size - 1u] = (uint8_t)(crc >> 8);
}

static bool accept_event(void *context, const smart_band_event_t *event)
{
  event_counter_t *counter = context;

  if (counter == NULL || event == NULL ||
      event->type != SMART_BAND_EVENT_SYNC_REQUEST)
    {
      return false;
    }
  counter->calls++;
  return true;
}

static smart_band_daily_summary_t make_day(unsigned int index)
{
  smart_band_daily_summary_t day;

  memset(&day, 0, sizeof(day));
  day.day_key = (int32_t)(20260701u + index);
  day.steps = UINT32_C(10000) + index * UINT32_C(137);
  day.active_seconds = UINT32_C(1800) + index * UINT32_C(11);
  day.calories_milli_kcal = UINT32_C(220000) + index * UINT32_C(1234);
  day.heart_weighted_bpm_seconds = UINT32_C(51000) + index * UINT32_C(97);
  day.heart_duration_seconds = UINT32_C(600) + index;
  day.heart_min_bpm = (uint8_t)(55u + index);
  day.heart_max_bpm = (uint8_t)(130u + index);
  day.source_flags = (uint8_t)(SMART_BAND_HISTORY_SOURCE_SENSOR |
                               SMART_BAND_HISTORY_SOURCE_DERIVED);
  day.flags = (uint8_t)(SMART_BAND_HISTORY_DAY_COMPLETE |
                        SMART_BAND_HISTORY_DAY_HEART_VALID);
  return day;
}

static int test_capabilities_golden_and_malformed(void)
{
  static const uint8_t golden[] = {
    0x53, 0x42, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x44, 0x33, 0x22, 0x11, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x40, 0x00, 0xe8, 0x1e
  };
  smart_band_sync_capabilities_t capabilities;
  uint8_t encoded[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  uint8_t malformed[sizeof(golden)];
  size_t encoded_size = 0u;
  size_t prefix;

  CHECK(smart_band_sync_encode_capabilities(
          UINT32_C(0x11223344), SMART_BAND_SYNC_LOOPBACK_MTU, encoded,
          sizeof(encoded), &encoded_size) == SMART_BAND_SYNC_SERVICE_OK);
  CHECK(encoded_size == sizeof(golden));
  CHECK(memcmp(encoded, golden, sizeof(golden)) == 0);
  CHECK(smart_band_sync_decode_capabilities(
          golden, sizeof(golden), UINT32_C(0x11223344), &capabilities) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(capabilities.protocol_major == SMART_BAND_SYNC_PROTOCOL_MAJOR);
  CHECK(capabilities.protocol_minor == SMART_BAND_SYNC_PROTOCOL_MINOR);
  CHECK(capabilities.feature_bits == SMART_BAND_SYNC_FEATURE_HISTORY);
  CHECK(capabilities.mtu == SMART_BAND_SYNC_LOOPBACK_MTU);
  CHECK(smart_band_sync_decode_capabilities(
          golden, sizeof(golden), UINT32_C(0x11223345), &capabilities) ==
        SMART_BAND_SYNC_SERVICE_WRONG_TRANSACTION);
  CHECK(smart_band_sync_decode_capabilities(
          golden, sizeof(golden), UINT32_C(0x11223344), NULL) ==
        SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT);
  CHECK(smart_band_sync_encode_capabilities(
          1u, SMART_BAND_SYNC_HISTORY_MIN_MTU - 1u, encoded,
          sizeof(encoded), &encoded_size) ==
        SMART_BAND_SYNC_SERVICE_UNSUPPORTED_MTU);
  CHECK(smart_band_sync_encode_capabilities(
          1u, SMART_BAND_SYNC_LOOPBACK_MTU, encoded, 1u, &encoded_size) ==
        SMART_BAND_SYNC_SERVICE_BUFFER_TOO_SMALL);
  for (prefix = 0u; prefix < sizeof(golden); prefix++)
    {
      CHECK(smart_band_sync_decode_capabilities(
              golden, prefix, UINT32_C(0x11223344), &capabilities) ==
            SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR);
    }
  memcpy(malformed, golden, sizeof(malformed));
  malformed[18] = SMART_BAND_SYNC_MESSAGE_HISTORY_DATA;
  refresh_crc(malformed, sizeof(malformed));
  CHECK(smart_band_sync_decode_capabilities(
          malformed, sizeof(malformed), UINT32_C(0x11223344),
          &capabilities) == SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR);
  memcpy(malformed, golden, sizeof(malformed));
  malformed[19] = SMART_BAND_SYNC_PROTOCOL_MAJOR + 1u;
  refresh_crc(malformed, sizeof(malformed));
  CHECK(smart_band_sync_decode_capabilities(
          malformed, sizeof(malformed), UINT32_C(0x11223344),
          &capabilities) == SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR);
  memcpy(malformed, golden, sizeof(malformed));
  malformed[25] = (uint8_t)(SMART_BAND_SYNC_HISTORY_MIN_MTU - 1u);
  refresh_crc(malformed, sizeof(malformed));
  CHECK(smart_band_sync_decode_capabilities(
          malformed, sizeof(malformed), UINT32_C(0x11223344),
          &capabilities) == SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR);
  return 0;
}

static int start_link(smart_band_sync_loopback_t *loopback,
                      smart_band_sync_transport_t *transport,
                      event_counter_t *counter)
{
  smart_band_sync_loopback_init(loopback);
  *transport = smart_band_sync_loopback_transport(loopback);
  return transport->ops->start(transport->context, accept_event, counter) ==
         SMART_BAND_PLATFORM_OK ? 0 : 1;
}

static int poll_frame(smart_band_sync_transport_t *transport,
                      uint8_t *frame, size_t *frame_size)
{
  return transport->ops->poll(transport->context, frame,
                              SMART_BAND_SYNC_LOOPBACK_MTU, frame_size) ==
         SMART_BAND_PLATFORM_OK ? 0 : 1;
}

static int test_faulted_history_sync_resume_and_idempotence(void)
{
  smart_band_history_t history;
  smart_band_sync_history_server_t server;
  smart_band_sync_history_server_t future;
  smart_band_sync_history_client_t client;
  smart_band_sync_loopback_t downlink;
  smart_band_sync_loopback_t uplink;
  smart_band_sync_transport_t down_transport;
  smart_band_sync_transport_t up_transport;
  smart_band_sync_loopback_faults_t faults;
  event_counter_t down_events = {0u};
  event_counter_t up_events = {0u};
  uint8_t data[SMART_BAND_SYNC_LOOPBACK_MTU];
  uint8_t received[SMART_BAND_SYNC_LOOPBACK_MTU];
  uint8_t ack[SMART_BAND_SYNC_LOOPBACK_MTU];
  uint8_t received_ack[SMART_BAND_SYNC_LOOPBACK_MTU];
  size_t data_size = 0u;
  size_t received_size = 0u;
  size_t ack_size = 0u;
  size_t received_ack_size = 0u;
  uint8_t request[SMART_BAND_SYNC_LOOPBACK_MTU];
  size_t request_size = 0u;
  size_t index;
  const uint32_t transaction = UINT32_C(0xa1b2c3d4);

  memset(&history, 0, sizeof(history));
  history.daily_count = 7u;
  for (index = 0u; index < history.daily_count; index++)
    {
      history.daily[index] = make_day((unsigned int)index);
    }
  CHECK(start_link(&downlink, &down_transport, &down_events) == 0);
  CHECK(start_link(&uplink, &up_transport, &up_events) == 0);
  smart_band_sync_history_server_init(&server);
  smart_band_sync_history_client_init(&client, transaction);
  CHECK(smart_band_sync_encode_history_request(
          transaction, 0u, request, sizeof(request), &request_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(request_size == sizeof(g_history_request_golden));
  CHECK(memcmp(request, g_history_request_golden, request_size) == 0);
  CHECK(smart_band_sync_history_server_request(
          &server, &history, request, request_size,
          SMART_BAND_SYNC_LOOPBACK_MTU) == SMART_BAND_SYNC_SERVICE_OK);

  /* A lost first data frame is retried byte-for-byte. */
  CHECK(smart_band_sync_history_server_next(
          &server, data, sizeof(data), &data_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(data_size == sizeof(g_history_data_golden));
  CHECK(memcmp(data, g_history_data_golden, data_size) == 0);
  memset(&faults, 0, sizeof(faults));
  faults.drop_next = true;
  smart_band_sync_loopback_set_faults(&downlink, &faults);
  CHECK(down_transport.ops->send(down_transport.context, data, data_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(downlink.dropped_frames == 1u && downlink.count == 0u);
  smart_band_sync_loopback_set_faults(&downlink, NULL);
  CHECK(down_transport.ops->send(down_transport.context, data, data_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(poll_frame(&down_transport, received, &received_size) == 0);
  CHECK(smart_band_sync_history_client_accept(
          &client, received, received_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(ack_size == sizeof(g_history_ack_golden));
  CHECK(memcmp(ack, g_history_ack_golden, ack_size) == 0);

  /* Lose the ack; server resends and client emits an idempotent ack. */
  memset(&faults, 0, sizeof(faults));
  faults.drop_next = true;
  smart_band_sync_loopback_set_faults(&uplink, &faults);
  CHECK(up_transport.ops->send(up_transport.context, ack, ack_size) ==
        SMART_BAND_PLATFORM_OK);
  smart_band_sync_loopback_set_faults(&uplink, NULL);
  CHECK(smart_band_sync_history_server_next(
          &server, data, sizeof(data), &data_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(down_transport.ops->send(down_transport.context, data, data_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(poll_frame(&down_transport, received, &received_size) == 0);
  CHECK(smart_band_sync_history_client_accept(
          &client, received, received_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_DUPLICATE);
  CHECK(up_transport.ops->send(up_transport.context, ack, ack_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(poll_frame(&up_transport, received_ack, &received_ack_size) == 0);
  CHECK(smart_band_sync_history_server_ack(
          &server, received_ack, received_ack_size) ==
        SMART_BAND_SYNC_SERVICE_OK);

  /* Duplicate the next data frame and prove both data and ack are idempotent. */
  CHECK(smart_band_sync_history_server_next(
          &server, data, sizeof(data), &data_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  memset(&faults, 0, sizeof(faults));
  faults.duplicate_next = true;
  smart_band_sync_loopback_set_faults(&downlink, &faults);
  CHECK(down_transport.ops->send(down_transport.context, data, data_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(downlink.duplicated_frames == 1u);
  CHECK(poll_frame(&down_transport, received, &received_size) == 0);
  CHECK(smart_band_sync_history_client_accept(
          &client, received, received_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(up_transport.ops->send(up_transport.context, ack, ack_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(poll_frame(&up_transport, received_ack, &received_ack_size) == 0);
  CHECK(smart_band_sync_history_server_ack(
          &server, received_ack, received_ack_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(poll_frame(&down_transport, received, &received_size) == 0);
  CHECK(smart_band_sync_history_client_accept(
          &client, received, received_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_DUPLICATE);
  CHECK(up_transport.ops->send(up_transport.context, ack, ack_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(poll_frame(&up_transport, received_ack, &received_ack_size) == 0);
  CHECK(smart_band_sync_history_server_ack(
          &server, received_ack, received_ack_size) ==
        SMART_BAND_SYNC_SERVICE_DUPLICATE);

  /* Reorder a future chunk ahead of the expected one; it is rejected. */
  future = server;
  future.cursor++;
  CHECK(smart_band_sync_history_server_next(
          &server, data, sizeof(data), &data_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  memset(&faults, 0, sizeof(faults));
  faults.reorder_next_pair = true;
  smart_band_sync_loopback_set_faults(&downlink, &faults);
  CHECK(down_transport.ops->send(down_transport.context, data, data_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_sync_history_server_next(
          &future, data, sizeof(data), &data_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(down_transport.ops->send(down_transport.context, data, data_size) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(poll_frame(&down_transport, received, &received_size) == 0);
  ack_size = 99u;
  CHECK(smart_band_sync_history_client_accept(
          &client, received, received_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_OUT_OF_ORDER);
  CHECK(ack_size == 0u);
  CHECK(poll_frame(&down_transport, received, &received_size) == 0);
  CHECK(smart_band_sync_history_client_accept(
          &client, received, received_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(smart_band_sync_history_server_ack(&server, ack, ack_size) ==
        SMART_BAND_SYNC_SERVICE_OK);

  /* Poll delay does not consume the queued chunk. */
  CHECK(smart_band_sync_history_server_next(
          &server, data, sizeof(data), &data_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  memset(&faults, 0, sizeof(faults));
  faults.delay_polls = 2u;
  smart_band_sync_loopback_set_faults(&downlink, &faults);
  CHECK(down_transport.ops->send(down_transport.context, data, data_size) ==
        SMART_BAND_PLATFORM_OK);
  received_size = 99u;
  CHECK(down_transport.ops->poll(down_transport.context, received,
                                 sizeof(received), &received_size) ==
        SMART_BAND_PLATFORM_BUSY);
  CHECK(received_size == 0u);
  CHECK(down_transport.ops->poll(down_transport.context, received,
                                 sizeof(received), &received_size) ==
        SMART_BAND_PLATFORM_BUSY);
  CHECK(poll_frame(&down_transport, received, &received_size) == 0);
  CHECK(smart_band_sync_history_client_accept(
          &client, received, received_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(smart_band_sync_history_server_ack(&server, ack, ack_size) ==
        SMART_BAND_SYNC_SERVICE_OK);

  /* Disconnect before send, reconnect, and resume from the acknowledged cursor. */
  CHECK(smart_band_sync_history_server_next(
          &server, data, sizeof(data), &data_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  memset(&faults, 0, sizeof(faults));
  faults.disconnect_next = true;
  smart_band_sync_loopback_set_faults(&downlink, &faults);
  CHECK(down_transport.ops->send(down_transport.context, data, data_size) ==
        SMART_BAND_PLATFORM_BUSY);
  CHECK(down_transport.ops->status(down_transport.context) ==
        SMART_BAND_SYNC_STOPPED);
  smart_band_sync_loopback_set_faults(&downlink, NULL);
  CHECK(down_transport.ops->start(down_transport.context, accept_event,
                                  &down_events) == SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_sync_encode_history_request(
          transaction, client.next_cursor, request, sizeof(request),
          &request_size) == SMART_BAND_SYNC_SERVICE_OK);
  CHECK(smart_band_sync_history_server_request(
          &server, &history, request, request_size,
          SMART_BAND_SYNC_LOOPBACK_MTU) == SMART_BAND_SYNC_SERVICE_OK);

  while (client.next_cursor < history.daily_count)
    {
      smart_band_sync_service_result_t client_result;
      smart_band_sync_service_result_t server_result;

      CHECK(smart_band_sync_history_server_next(
              &server, data, sizeof(data), &data_size) ==
            SMART_BAND_SYNC_SERVICE_OK);
      CHECK(down_transport.ops->send(down_transport.context, data,
                                     data_size) == SMART_BAND_PLATFORM_OK);
      CHECK(poll_frame(&down_transport, received, &received_size) == 0);
      client_result = smart_band_sync_history_client_accept(
        &client, received, received_size, ack, sizeof(ack), &ack_size);
      CHECK(client_result == SMART_BAND_SYNC_SERVICE_OK ||
            client_result == SMART_BAND_SYNC_SERVICE_COMPLETE);
      server_result = smart_band_sync_history_server_ack(
        &server, ack, ack_size);
      CHECK(server_result == SMART_BAND_SYNC_SERVICE_OK ||
            server_result == SMART_BAND_SYNC_SERVICE_COMPLETE);
    }
  CHECK(client.record_count == 7u);
  CHECK(server.cursor == 7u);
  for (index = 0u; index < 7u; index++)
    {
      CHECK(smart_band_sync_daily_equal(&client.records[index],
                                        &history.daily[index]));
    }
  CHECK(down_events.calls >= 7u);
  return 0;
}

static int test_contract_rejections(void)
{
  smart_band_history_t history;
  smart_band_sync_history_server_t server;
  smart_band_sync_history_client_t client;
  uint8_t frame[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  uint8_t ack[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  size_t frame_size = 0u;
  size_t ack_size = 0u;

  memset(&history, 0, sizeof(history));
  history.daily_count = 1u;
  history.daily[0] = make_day(0u);
  CHECK(smart_band_sync_history_server_begin(
          NULL, &history, 1u, 0u, SMART_BAND_SYNC_LOOPBACK_MTU) ==
        SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT);
  smart_band_sync_history_server_init(&server);
  CHECK(smart_band_sync_history_server_begin(
          &server, &history, 0u, 0u, SMART_BAND_SYNC_LOOPBACK_MTU) ==
        SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT);
  CHECK(smart_band_sync_history_server_begin(
          &server, &history, 1u, 2u, SMART_BAND_SYNC_LOOPBACK_MTU) ==
        SMART_BAND_SYNC_SERVICE_BAD_CURSOR);
  CHECK(smart_band_sync_history_server_begin(
          &server, &history, 1u, 0u,
          SMART_BAND_SYNC_HISTORY_MIN_MTU - 1u) ==
        SMART_BAND_SYNC_SERVICE_UNSUPPORTED_MTU);
  CHECK(smart_band_sync_encode_history_request(
          0u, 0u, frame, sizeof(frame), &frame_size) ==
        SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT);
  CHECK(smart_band_sync_encode_history_request(
          1u, (size_t)UINT16_MAX + 1u, frame, sizeof(frame), &frame_size) ==
        SMART_BAND_SYNC_SERVICE_INVALID_ARGUMENT);
  CHECK(smart_band_sync_history_server_begin(
          &server, &history, 1u, 0u, SMART_BAND_SYNC_LOOPBACK_MTU) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(smart_band_sync_history_server_next(
          &server, frame, 1u, &frame_size) ==
        SMART_BAND_SYNC_SERVICE_BUFFER_TOO_SMALL);
  CHECK(smart_band_sync_history_server_next(
          &server, frame, sizeof(frame), &frame_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  smart_band_sync_history_client_init(&client, 2u);
  CHECK(smart_band_sync_history_client_accept(
          &client, frame, frame_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_WRONG_TRANSACTION);
  smart_band_sync_history_client_init(&client, 1u);
  frame[frame_size - 1u] ^= UINT8_C(0x80);
  CHECK(smart_band_sync_history_client_accept(
          &client, frame, frame_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR);
  return 0;
}

static int test_ack_state_total_lock_and_atomic_commit(void)
{
  smart_band_history_t history;
  smart_band_history_t empty_history;
  smart_band_sync_history_server_t server;
  smart_band_sync_history_client_t client;
  uint8_t data[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  uint8_t second[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  uint8_t ack[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  size_t data_size = 0u;
  size_t second_size = 0u;
  size_t ack_size = 0u;
  size_t saved_cursor;
  size_t saved_count;
  const uint32_t transaction = UINT32_C(0xa1b2c3d4);

  memset(&history, 0, sizeof(history));
  memset(&empty_history, 0, sizeof(empty_history));
  history.daily_count = 2u;
  history.daily[0] = make_day(0u);
  history.daily[1] = make_day(1u);
  smart_band_sync_history_server_init(&server);
  CHECK(smart_band_sync_history_server_begin(
          &server, &history, transaction, 0u,
          SMART_BAND_SYNC_LOOPBACK_MTU) == SMART_BAND_SYNC_SERVICE_OK);

  /* A syntactically valid ACK cannot advance before a data frame was sent. */
  CHECK(smart_band_sync_history_server_ack(
          &server, g_history_ack_golden, sizeof(g_history_ack_golden)) ==
        SMART_BAND_SYNC_SERVICE_OUT_OF_ORDER);
  CHECK(server.cursor == 0u && !server.awaiting_ack);

  CHECK(smart_band_sync_history_server_next(
          &server, data, sizeof(data), &data_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(server.awaiting_ack && server.last_sent_cursor == 0u);
  smart_band_sync_history_client_init(&client, transaction);

  /* An undersized ACK buffer must leave every client commit field untouched. */
  ack_size = 99u;
  CHECK(smart_band_sync_history_client_accept(
          &client, data, data_size, ack, 1u, &ack_size) ==
        SMART_BAND_SYNC_SERVICE_BUFFER_TOO_SMALL);
  CHECK(client.next_cursor == 0u && client.record_count == 0u);
  CHECK(!client.total_locked && client.expected_total == 0u);
  CHECK(ack_size == sizeof(g_history_ack_golden));

  CHECK(smart_band_sync_history_client_accept(
          &client, data, data_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(client.next_cursor == 1u && client.record_count == 1u);
  CHECK(client.total_locked && client.expected_total == 2u);
  CHECK(smart_band_sync_history_server_ack(&server, ack, ack_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(!server.awaiting_ack && server.cursor == 1u);

  CHECK(smart_band_sync_history_server_next(
          &server, second, sizeof(second), &second_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  /* Keep CRC valid while changing only the advertised snapshot total. */
  second[21] = 3u;
  second[22] = 0u;
  refresh_crc(second, second_size);
  saved_cursor = client.next_cursor;
  saved_count = client.record_count;
  ack_size = 99u;
  CHECK(smart_band_sync_history_client_accept(
          &client, second, second_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_PROTOCOL_ERROR);
  CHECK(ack_size == 0u);
  CHECK(client.next_cursor == saved_cursor &&
        client.record_count == saved_count && client.expected_total == 2u);

  /* Once the only record is ACKed, a fabricated cursor past completion fails. */
  history.daily_count = 1u;
  smart_band_sync_history_server_init(&server);
  smart_band_sync_history_client_init(&client, transaction);
  CHECK(smart_band_sync_history_server_begin(
          &server, &history, transaction, 0u,
          SMART_BAND_SYNC_LOOPBACK_MTU) == SMART_BAND_SYNC_SERVICE_OK);
  CHECK(smart_band_sync_history_server_next(
          &server, data, sizeof(data), &data_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(smart_band_sync_history_client_accept(
          &client, data, data_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_COMPLETE);
  CHECK(smart_band_sync_history_server_ack(&server, ack, ack_size) ==
        SMART_BAND_SYNC_SERVICE_COMPLETE);
  ack[14] = 1u;
  ack[16] = 1u;
  ack[19] = 2u;
  refresh_crc(ack, ack_size);
  CHECK(smart_band_sync_history_server_ack(&server, ack, ack_size) ==
        SMART_BAND_SYNC_SERVICE_OUT_OF_ORDER);
  CHECK(server.cursor == 1u && !server.awaiting_ack);

  /* Empty snapshots and completed snapshots cannot consume arbitrary ACKs. */
  smart_band_sync_history_server_init(&server);
  CHECK(smart_band_sync_history_server_begin(
          &server, &empty_history, transaction, 0u,
          SMART_BAND_SYNC_LOOPBACK_MTU) == SMART_BAND_SYNC_SERVICE_COMPLETE);
  CHECK(smart_band_sync_history_server_next(
          &server, data, sizeof(data), &data_size) ==
        SMART_BAND_SYNC_SERVICE_COMPLETE);
  CHECK(smart_band_sync_history_server_ack(
          &server, g_history_ack_golden, sizeof(g_history_ack_golden)) ==
        SMART_BAND_SYNC_SERVICE_OUT_OF_ORDER);
  CHECK(server.cursor == 0u && !server.awaiting_ack);
  return 0;
}

static int test_full_snapshot_survives_rolling_history(void)
{
  smart_band_history_t history;
  smart_band_daily_summary_t original[SMART_BAND_HISTORY_DAILY_CAPACITY];
  smart_band_sync_history_server_t server;
  smart_band_sync_history_client_t client;
  smart_band_sync_history_client_t fresh_client;
  uint8_t request[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  uint8_t data[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  uint8_t ack[SMART_BAND_SYNC_PROTOCOL_MAX_FRAME_SIZE];
  size_t request_size = 0u;
  size_t data_size = 0u;
  size_t ack_size = 0u;
  size_t index;
  const uint32_t transaction = UINT32_C(0x55667788);
  const uint32_t fresh_transaction = UINT32_C(0x55667789);

  memset(&history, 0, sizeof(history));
  history.daily_count = SMART_BAND_HISTORY_DAILY_CAPACITY;
  for (index = 0u; index < SMART_BAND_HISTORY_DAILY_CAPACITY; index++)
    {
      history.daily[index] = make_day((unsigned int)index);
      original[index] = history.daily[index];
    }
  smart_band_sync_history_server_init(&server);
  smart_band_sync_history_client_init(&client, transaction);
  CHECK(smart_band_sync_encode_history_request(
          transaction, 0u, request, sizeof(request), &request_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(smart_band_sync_history_server_request(
          &server, &history, request, request_size,
          SMART_BAND_SYNC_LOOPBACK_MTU) == SMART_BAND_SYNC_SERVICE_OK);

  for (index = 0u; index < 5u; index++)
    {
      CHECK(smart_band_sync_history_server_next(
              &server, data, sizeof(data), &data_size) ==
            SMART_BAND_SYNC_SERVICE_OK);
      CHECK(smart_band_sync_history_client_accept(
              &client, data, data_size, ack, sizeof(ack), &ack_size) ==
            SMART_BAND_SYNC_SERVICE_OK);
      CHECK(smart_band_sync_history_server_ack(&server, ack, ack_size) ==
            SMART_BAND_SYNC_SERVICE_OK);
    }

  /* The live 30-day window rolls while disconnected. */
  memmove(&history.daily[0], &history.daily[1],
          (SMART_BAND_HISTORY_DAILY_CAPACITY - 1u) *
            sizeof(history.daily[0]));
  history.daily[SMART_BAND_HISTORY_DAILY_CAPACITY - 1u] = make_day(99u);

  CHECK(smart_band_sync_encode_history_request(
          transaction, client.next_cursor, request, sizeof(request),
          &request_size) == SMART_BAND_SYNC_SERVICE_OK);
  CHECK(smart_band_sync_history_server_request(
          &server, &history, request, request_size,
          SMART_BAND_SYNC_LOOPBACK_MTU) == SMART_BAND_SYNC_SERVICE_OK);
  while (client.next_cursor < SMART_BAND_HISTORY_DAILY_CAPACITY)
    {
      smart_band_sync_service_result_t result;

      CHECK(smart_band_sync_history_server_next(
              &server, data, sizeof(data), &data_size) ==
            SMART_BAND_SYNC_SERVICE_OK);
      result = smart_band_sync_history_client_accept(
        &client, data, data_size, ack, sizeof(ack), &ack_size);
      CHECK(result == SMART_BAND_SYNC_SERVICE_OK ||
            result == SMART_BAND_SYNC_SERVICE_COMPLETE);
      result = smart_band_sync_history_server_ack(&server, ack, ack_size);
      CHECK(result == SMART_BAND_SYNC_SERVICE_OK ||
            result == SMART_BAND_SYNC_SERVICE_COMPLETE);
    }
  CHECK(client.record_count == SMART_BAND_HISTORY_DAILY_CAPACITY);
  CHECK(client.expected_total == SMART_BAND_HISTORY_DAILY_CAPACITY);
  for (index = 0u; index < SMART_BAND_HISTORY_DAILY_CAPACITY; index++)
    {
      CHECK(smart_band_sync_daily_equal(&client.records[index],
                                        &original[index]));
    }

  /* A new transaction receives the newly rolled live snapshot. */
  CHECK(smart_band_sync_encode_history_request(
          fresh_transaction, 0u, request, sizeof(request), &request_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(smart_band_sync_history_server_request(
          &server, &history, request, request_size,
          SMART_BAND_SYNC_LOOPBACK_MTU) == SMART_BAND_SYNC_SERVICE_OK);
  CHECK(smart_band_sync_history_server_next(
          &server, data, sizeof(data), &data_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  smart_band_sync_history_client_init(&fresh_client, fresh_transaction);
  CHECK(smart_band_sync_history_client_accept(
          &fresh_client, data, data_size, ack, sizeof(ack), &ack_size) ==
        SMART_BAND_SYNC_SERVICE_OK);
  CHECK(smart_band_sync_daily_equal(&fresh_client.records[0],
                                    &history.daily[0]));
  CHECK(!smart_band_sync_daily_equal(&fresh_client.records[0], &original[0]));
  return 0;
}

static int test_loopback_reconnect_discards_stale_delivery(void)
{
  smart_band_sync_loopback_t loopback;
  smart_band_sync_transport_t transport;
  smart_band_sync_loopback_faults_t faults;
  event_counter_t events = {0u};
  uint8_t first[] = {1u};
  uint8_t second[] = {2u};
  uint8_t output[SMART_BAND_SYNC_LOOPBACK_MTU];
  size_t actual = 0u;

  CHECK(start_link(&loopback, &transport, &events) == 0);
  CHECK(transport.ops->send(transport.context, first, sizeof(first)) ==
        SMART_BAND_PLATFORM_OK);
  memset(&faults, 0, sizeof(faults));
  faults.reorder_next_pair = true;
  smart_band_sync_loopback_set_faults(&loopback, &faults);
  CHECK(transport.ops->send(transport.context, second, sizeof(second)) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(loopback.count == 1u && loopback.holding_reorder_frame);
  CHECK(transport.ops->stop(transport.context) == SMART_BAND_PLATFORM_OK);
  CHECK(loopback.count == 0u && loopback.head == 0u);
  CHECK(!loopback.holding_reorder_frame && loopback.held_frame.size == 0u);
  smart_band_sync_loopback_set_faults(&loopback, NULL);
  CHECK(transport.ops->start(transport.context, accept_event, &events) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(transport.ops->poll(transport.context, output, sizeof(output),
                            &actual) == SMART_BAND_PLATFORM_BUSY);

  CHECK(transport.ops->send(transport.context, first, sizeof(first)) ==
        SMART_BAND_PLATFORM_OK);
  memset(&faults, 0, sizeof(faults));
  faults.disconnect_next = true;
  smart_band_sync_loopback_set_faults(&loopback, &faults);
  CHECK(transport.ops->send(transport.context, second, sizeof(second)) ==
        SMART_BAND_PLATFORM_BUSY);
  CHECK(loopback.count == 0u && !loopback.holding_reorder_frame);
  CHECK(transport.ops->status(transport.context) == SMART_BAND_SYNC_STOPPED);
  smart_band_sync_loopback_set_faults(&loopback, NULL);
  CHECK(transport.ops->start(transport.context, accept_event, &events) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(transport.ops->poll(transport.context, output, sizeof(output),
                            &actual) == SMART_BAND_PLATFORM_BUSY);
  return 0;
}

int main(void)
{
  CHECK(test_capabilities_golden_and_malformed() == 0);
  CHECK(test_faulted_history_sync_resume_and_idempotence() == 0);
  CHECK(test_contract_rejections() == 0);
  CHECK(test_ack_state_total_lock_and_atomic_commit() == 0);
  CHECK(test_full_snapshot_survives_rolling_history() == 0);
  CHECK(test_loopback_reconnect_discards_stale_delivery() == 0);
  puts("Q6 history sync service + faulted loopback tests passed");
  return 0;
}
