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

static bool sequence_before(const smart_band_event_t *left,
                            const smart_band_event_t *right)
{
  if (left->ingress_sequence == 0u || right->ingress_sequence == 0u ||
      left->ingress_sequence == right->ingress_sequence)
    {
      return false;
    }

  return right->ingress_sequence - left->ingress_sequence <
         (UINT64_MAX / 2u) + 1u;
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
        if (event->payload.notification_received.type <
              SMART_BAND_NOTIFICATION_TYPE_CALL ||
            event->payload.notification_received.type >
              SMART_BAND_NOTIFICATION_TYPE_SYSTEM ||
            event->payload.notification_received.priority <
              SMART_BAND_NOTIFICATION_PRIORITY_LOW ||
            event->payload.notification_received.priority >
              SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL)
          {
            return SMART_BAND_EVENT_PRIORITY_LOW;
          }

        if (event->payload.notification_received.type ==
              SMART_BAND_NOTIFICATION_TYPE_CALL &&
            event->payload.notification_received.priority <
              SMART_BAND_NOTIFICATION_PRIORITY_HIGH)
          {
            return SMART_BAND_EVENT_PRIORITY_HIGH;
          }

        switch (event->payload.notification_received.priority)
          {
            case SMART_BAND_NOTIFICATION_PRIORITY_LOW:
              return SMART_BAND_EVENT_PRIORITY_LOW;
            case SMART_BAND_NOTIFICATION_PRIORITY_NORMAL:
              return SMART_BAND_EVENT_PRIORITY_NORMAL;
            case SMART_BAND_NOTIFICATION_PRIORITY_HIGH:
              return SMART_BAND_EVENT_PRIORITY_HIGH;
            case SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL:
              return SMART_BAND_EVENT_PRIORITY_CRITICAL;
            default:
              return SMART_BAND_EVENT_PRIORITY_LOW;
          }
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

      if (incoming_priority <
          smart_band_event_priority(&queue->items[lowest_index]))
        {
          queue->dropped++;
          return false;
        }

      if (incoming_priority ==
          smart_band_event_priority(&queue->items[lowest_index]))
        {
          size_t latest_index = 0u;
          bool found_later = false;

          for (index = 0u; index < queue->count; index++)
            {
              if (smart_band_event_priority(&queue->items[index]) ==
                    incoming_priority &&
                  sequence_before(event, &queue->items[index]) &&
                  (!found_later || sequence_before(
                     &queue->items[latest_index], &queue->items[index])))
                {
                  latest_index = index;
                  found_later = true;
                }
            }

          if (!found_later)
            {
              queue->dropped++;
              return false;
            }
          lowest_index = latest_index;
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
            smart_band_event_priority(&queue->items[selected]) ||
          (smart_band_event_priority(&queue->items[index]) ==
             smart_band_event_priority(&queue->items[selected]) &&
           sequence_before(&queue->items[index], &queue->items[selected])))
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
  size_t selected = 0u;
  bool found = false;

  if (queue == NULL || event == NULL || type == SMART_BAND_EVENT_NONE)
    {
      return false;
    }

  for (index = 0; index < queue->count; index++)
    {
      if (queue->items[index].type == type)
        {
          if (!found || sequence_before(&queue->items[index],
                                        &queue->items[selected]))
            {
              selected = index;
              found = true;
            }
        }
    }

  if (!found)
    {
      return false;
    }
  *event = queue->items[selected];
  remove_at(queue, selected);
  return true;
}

bool smart_band_event_queue_take_next_notification(
  smart_band_event_queue_t *queue, smart_band_event_t *event)
{
  size_t index;
  size_t selected = 0u;
  bool found = false;

  if (queue == NULL || event == NULL)
    {
      return false;
    }

  for (index = 0u; index < queue->count; index++)
    {
      if (queue->items[index].type ==
            SMART_BAND_EVENT_NOTIFICATION_RECEIVED ||
          queue->items[index].type == SMART_BAND_EVENT_NOTIFICATION_ACTION)
        {
          if (!found || sequence_before(&queue->items[index],
                                        &queue->items[selected]))
            {
              selected = index;
              found = true;
            }
        }
    }

  if (!found)
    {
      return false;
    }
  *event = queue->items[selected];
  remove_at(queue, selected);
  return true;
}

bool smart_band_event_queue_take_next_domain(
  smart_band_event_queue_t *queue, smart_band_event_t *event)
{
  size_t index;
  size_t selected = 0u;
  bool found = false;

  if (queue == NULL || event == NULL)
    {
      return false;
    }

  for (index = 0u; index < queue->count; index++)
    {
      smart_band_event_type_t type = queue->items[index].type;

      if (type == SMART_BAND_EVENT_WORKOUT_COMMAND ||
          type == SMART_BAND_EVENT_WORKOUT_CHECKPOINT ||
          type == SMART_BAND_EVENT_NOTIFICATION_RECEIVED ||
          type == SMART_BAND_EVENT_NOTIFICATION_ACTION)
        {
          if (!found || sequence_before(&queue->items[index],
                                        &queue->items[selected]))
            {
              selected = index;
              found = true;
            }
        }
    }

  if (!found)
    {
      return false;
    }
  *event = queue->items[selected];
  remove_at(queue, selected);
  return true;
}

size_t smart_band_event_queue_count(const smart_band_event_queue_t *queue)
{
  return queue == NULL ? 0 : queue->count;
}
