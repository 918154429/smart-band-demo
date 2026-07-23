#include "smart_band_sync_loopback.h"

#include "smart_band_platform.h"

#include <string.h>

static void loopback_clear_delivery(smart_band_sync_loopback_t *loopback)
{
  memset(loopback->frames, 0, sizeof(loopback->frames));
  memset(&loopback->held_frame, 0, sizeof(loopback->held_frame));
  loopback->head = 0u;
  loopback->count = 0u;
  loopback->holding_reorder_frame = false;
}

static smart_band_platform_result_t
loopback_start(void *context, smart_band_event_sink_t event_sink,
               void *event_context)
{
  smart_band_sync_loopback_t *loopback = context;

  if (loopback == NULL || event_sink == NULL || loopback->started)
    {
      return loopback != NULL && loopback->started ?
             SMART_BAND_PLATFORM_BUSY : SMART_BAND_PLATFORM_INVALID;
    }

  loopback->event_sink = event_sink;
  loopback->event_context = event_context;
  loopback->started = true;
  return SMART_BAND_PLATFORM_OK;
}

static smart_band_platform_result_t loopback_stop(void *context)
{
  smart_band_sync_loopback_t *loopback = context;

  if (loopback == NULL)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  loopback->started = false;
  loopback->event_sink = NULL;
  loopback->event_context = NULL;
  loopback_clear_delivery(loopback);
  return SMART_BAND_PLATFORM_OK;
}

static smart_band_platform_result_t
enqueue_frame(smart_band_sync_loopback_t *loopback, const void *buffer,
              size_t size)
{
  smart_band_event_t event;
  size_t tail;

  if (loopback->count == SMART_BAND_SYNC_LOOPBACK_CAPACITY)
    {
      return SMART_BAND_PLATFORM_BUSY;
    }

  memset(&event, 0, sizeof(event));
  event.type = SMART_BAND_EVENT_SYNC_REQUEST;
  event.payload.generic.value = (int32_t)size;
  tail = (loopback->head + loopback->count) %
         SMART_BAND_SYNC_LOOPBACK_CAPACITY;
  memcpy(loopback->frames[tail].data, buffer, size);
  loopback->frames[tail].size = size;
  loopback->count++;
  if (!loopback->event_sink(loopback->event_context, &event))
    {
      loopback->count--;
      loopback->frames[tail].size = 0;
      return SMART_BAND_PLATFORM_BUSY;
    }

  return SMART_BAND_PLATFORM_OK;
}

static smart_band_platform_result_t
loopback_send(void *context, const void *buffer, size_t size)
{
  smart_band_sync_loopback_t *loopback = context;
  smart_band_platform_result_t result;

  if (loopback == NULL || buffer == NULL || size == 0 ||
      size > SMART_BAND_SYNC_LOOPBACK_MTU)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }
  if (!loopback->started)
    {
      return SMART_BAND_PLATFORM_BUSY;
    }
  if (loopback->faults.disconnect_next)
    {
      loopback->faults.disconnect_next = false;
      loopback->started = false;
      loopback->event_sink = NULL;
      loopback->event_context = NULL;
      loopback_clear_delivery(loopback);
      return SMART_BAND_PLATFORM_BUSY;
    }
  if (loopback->faults.drop_next)
    {
      loopback->faults.drop_next = false;
      loopback->dropped_frames++;
      return SMART_BAND_PLATFORM_OK;
    }
  if (loopback->faults.reorder_next_pair &&
      !loopback->holding_reorder_frame)
    {
      memcpy(loopback->held_frame.data, buffer, size);
      loopback->held_frame.size = size;
      loopback->holding_reorder_frame = true;
      return SMART_BAND_PLATFORM_OK;
    }
  if (loopback->holding_reorder_frame)
    {
      if (loopback->count + 2u > SMART_BAND_SYNC_LOOPBACK_CAPACITY)
        {
          return SMART_BAND_PLATFORM_BUSY;
        }
      result = enqueue_frame(loopback, buffer, size);
      if (result != SMART_BAND_PLATFORM_OK)
        {
          return result;
        }
      result = enqueue_frame(loopback, loopback->held_frame.data,
                             loopback->held_frame.size);
      if (result == SMART_BAND_PLATFORM_OK)
        {
          loopback->holding_reorder_frame = false;
          loopback->faults.reorder_next_pair = false;
          loopback->held_frame.size = 0u;
        }
      return result;
    }
  if (loopback->faults.duplicate_next)
    {
      if (loopback->count + 2u > SMART_BAND_SYNC_LOOPBACK_CAPACITY)
        {
          return SMART_BAND_PLATFORM_BUSY;
        }
      loopback->faults.duplicate_next = false;
      result = enqueue_frame(loopback, buffer, size);
      if (result != SMART_BAND_PLATFORM_OK)
        {
          return result;
        }
      result = enqueue_frame(loopback, buffer, size);
      if (result == SMART_BAND_PLATFORM_OK)
        {
          loopback->duplicated_frames++;
        }
      return result;
    }
  return enqueue_frame(loopback, buffer, size);
}

static smart_band_platform_result_t
loopback_poll(void *context, void *buffer, size_t capacity,
              size_t *actual_size)
{
  smart_band_sync_loopback_t *loopback = context;
  smart_band_sync_loopback_frame_t *frame;

  if (actual_size != NULL)
    {
      *actual_size = 0;
    }

  if (loopback == NULL || buffer == NULL || actual_size == NULL)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  if (!loopback->started || loopback->count == 0)
    {
      return SMART_BAND_PLATFORM_BUSY;
    }

  if (loopback->faults.delay_polls != 0u)
    {
      loopback->faults.delay_polls--;
      return SMART_BAND_PLATFORM_BUSY;
    }

  frame = &loopback->frames[loopback->head];
  if (capacity < frame->size)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  memcpy(buffer, frame->data, frame->size);
  *actual_size = frame->size;
  loopback->head = (loopback->head + 1) % SMART_BAND_SYNC_LOOPBACK_CAPACITY;
  loopback->count--;
  return SMART_BAND_PLATFORM_OK;
}

static smart_band_sync_transport_state_t loopback_status(void *context)
{
  smart_band_sync_loopback_t *loopback = context;

  return loopback != NULL && loopback->started ?
         SMART_BAND_SYNC_STARTED : SMART_BAND_SYNC_STOPPED;
}

static size_t loopback_mtu(void *context)
{
  return context == NULL ? 0 : SMART_BAND_SYNC_LOOPBACK_MTU;
}

static const smart_band_sync_transport_ops_t g_loopback_ops =
{
  loopback_start,
  loopback_stop,
  loopback_send,
  loopback_poll,
  loopback_status,
  loopback_mtu
};

void smart_band_sync_loopback_init(smart_band_sync_loopback_t *loopback)
{
  if (loopback != NULL)
    {
      memset(loopback, 0, sizeof(*loopback));
    }
}

void smart_band_sync_loopback_set_faults(
  smart_band_sync_loopback_t *loopback,
  const smart_band_sync_loopback_faults_t *faults)
{
  if (loopback != NULL)
    {
      if (faults == NULL)
        {
          memset(&loopback->faults, 0, sizeof(loopback->faults));
          loopback->holding_reorder_frame = false;
          loopback->held_frame.size = 0u;
        }
      else
        {
          loopback->faults = *faults;
          if (!faults->reorder_next_pair)
            {
              loopback->holding_reorder_frame = false;
              loopback->held_frame.size = 0u;
            }
        }
    }
}

smart_band_sync_transport_t
smart_band_sync_loopback_transport(smart_band_sync_loopback_t *loopback)
{
  smart_band_sync_transport_t transport;

  transport.ops = &g_loopback_ops;
  transport.context = loopback;
  return transport;
}
