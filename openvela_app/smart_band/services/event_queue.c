#include "smart_band_event.h"

#include <string.h>

static void remove_at(smart_band_event_queue_t *queue, size_t index)
{
  if (index + 1 < queue->count)
    {
      memmove(&queue->items[index], &queue->items[index + 1],
              (queue->count - index - 1) * sizeof(queue->items[0]));
    }

  queue->count--;
}

smart_band_event_priority_t
smart_band_event_priority(const smart_band_event_t *event)
{
  if (event == NULL)
    {
      return SMART_BAND_EVENT_PRIORITY_LOW;
    }

  switch (event->type)
    {
      case SMART_BAND_EVENT_WORKOUT_CHECKPOINT:
      case SMART_BAND_EVENT_STORAGE_FLUSH_REQUEST:
        return SMART_BAND_EVENT_PRIORITY_CRITICAL;
      case SMART_BAND_EVENT_WORKOUT_COMMAND:
      case SMART_BAND_EVENT_NOTIFICATION_ACTION:
      case SMART_BAND_EVENT_POWER_TIMEOUT:
      case SMART_BAND_EVENT_BLE_CONNECTED:
      case SMART_BAND_EVENT_BLE_DISCONNECTED:
        return SMART_BAND_EVENT_PRIORITY_HIGH;
      case SMART_BAND_EVENT_NOTIFICATION_RECEIVED:
        return event->payload.notification.kind ==
               SMART_BAND_NOTIFICATION_CALL ?
               SMART_BAND_EVENT_PRIORITY_HIGH :
               SMART_BAND_EVENT_PRIORITY_NORMAL;
      case SMART_BAND_EVENT_TOUCH_ACTIVITY:
      case SMART_BAND_EVENT_WRIST_RAISED:
      case SMART_BAND_EVENT_SYNC_REQUEST:
      case SMART_BAND_EVENT_SYNC_ACK:
        return SMART_BAND_EVENT_PRIORITY_NORMAL;
      case SMART_BAND_EVENT_METRICS_UPDATED:
      case SMART_BAND_EVENT_NONE:
      default:
        return SMART_BAND_EVENT_PRIORITY_LOW;
    }
}

void smart_band_event_queue_init(smart_band_event_queue_t *queue)
{
  if (queue != NULL)
    {
      memset(queue, 0, sizeof(*queue));
    }
}

bool smart_band_event_queue_push(smart_band_event_queue_t *queue,
                                 const smart_band_event_t *event)
{
  size_t index;
  size_t lowest_index = 0;
  smart_band_event_priority_t incoming_priority;

  if (queue == NULL || event == NULL || event->type <= SMART_BAND_EVENT_NONE ||
      event->type > SMART_BAND_EVENT_STORAGE_FLUSH_REQUEST)
    {
      return false;
    }

  if (event->type == SMART_BAND_EVENT_METRICS_UPDATED)
    {
      for (index = 0; index < queue->count; index++)
        {
          if (queue->items[index].type ==
              SMART_BAND_EVENT_METRICS_UPDATED)
            {
              queue->items[index].monotonic_ms = event->monotonic_ms;
              queue->items[index].payload.metrics.mask |=
                event->payload.metrics.mask;
              queue->coalesced++;
              return true;
            }
        }
    }

  incoming_priority = smart_band_event_priority(event);
  if (queue->count == SMART_BAND_EVENT_QUEUE_CAPACITY)
    {
      for (index = 1; index < queue->count; index++)
        {
          if (smart_band_event_priority(&queue->items[index]) <
              smart_band_event_priority(&queue->items[lowest_index]))
            {
              lowest_index = index;
            }
        }

      if (incoming_priority <=
          smart_band_event_priority(&queue->items[lowest_index]))
        {
          queue->dropped++;
          return false;
        }

      remove_at(queue, lowest_index);
      queue->evicted++;
    }

  queue->items[queue->count++] = *event;
  return true;
}

bool smart_band_event_queue_pop(smart_band_event_queue_t *queue,
                                smart_band_event_t *event)
{
  size_t index;
  size_t selected = 0;

  if (queue == NULL || event == NULL || queue->count == 0)
    {
      return false;
    }

  for (index = 1; index < queue->count; index++)
    {
      if (smart_band_event_priority(&queue->items[index]) >
          smart_band_event_priority(&queue->items[selected]))
        {
          selected = index;
        }
    }

  *event = queue->items[selected];
  remove_at(queue, selected);
  return true;
}

bool smart_band_event_queue_take(smart_band_event_queue_t *queue,
                                 smart_band_event_type_t type,
                                 smart_band_event_t *event)
{
  size_t index;

  if (queue == NULL || event == NULL || type == SMART_BAND_EVENT_NONE)
    {
      return false;
    }

  for (index = 0; index < queue->count; index++)
    {
      if (queue->items[index].type == type)
        {
          *event = queue->items[index];
          remove_at(queue, index);
          return true;
        }
    }

  return false;
}

size_t smart_band_event_queue_count(const smart_band_event_queue_t *queue)
{
  return queue == NULL ? 0 : queue->count;
}
