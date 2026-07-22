#include "smart_band_event.h"

#include <string.h>

static bool inbox_lock(smart_band_event_inbox_t *inbox)
{
  return inbox->lock.lock != NULL &&
         inbox->lock.lock(inbox->lock.context);
}

static void inbox_unlock(smart_band_event_inbox_t *inbox)
{
  inbox->lock.unlock(inbox->lock.context);
}

static size_t inbox_physical_index(const smart_band_event_inbox_t *inbox,
                                   size_t logical_index)
{
  return (inbox->head + logical_index) % SMART_BAND_EVENT_QUEUE_CAPACITY;
}

static void inbox_remove_at(smart_band_event_inbox_t *inbox,
                            size_t logical_index)
{
  size_t index;

  for (index = logical_index; index + 1u < inbox->count; index++)
    {
      inbox->items[inbox_physical_index(inbox, index)] =
        inbox->items[inbox_physical_index(inbox, index + 1u)];
    }

  inbox->count--;
}

bool smart_band_event_inbox_init(smart_band_event_inbox_t *inbox,
                                 const smart_band_event_lock_t *lock)
{
  if (inbox == NULL || (lock != NULL &&
      ((lock->lock == NULL) != (lock->unlock == NULL))))
    {
      return false;
    }

  memset(inbox, 0, sizeof(*inbox));
  if (lock != NULL)
    {
      inbox->lock = *lock;
      inbox->accepting = lock->lock != NULL;
    }

  return true;
}

bool smart_band_event_inbox_close(smart_band_event_inbox_t *inbox)
{
  if (inbox == NULL || !inbox_lock(inbox))
    {
      return false;
    }

  inbox->accepting = false;
  inbox_unlock(inbox);
  return true;
}

bool smart_band_event_inbox_post(smart_band_event_inbox_t *inbox,
                                 const smart_band_event_t *event)
{
  size_t tail;
  size_t index;
  size_t lowest_index = 0u;
  smart_band_event_priority_t incoming_priority;
  bool accepted = false;

  if (inbox == NULL || event == NULL || !inbox_lock(inbox))
    {
      return false;
    }

  incoming_priority = smart_band_event_priority(event);
  if (inbox->accepting && inbox->count == SMART_BAND_EVENT_QUEUE_CAPACITY)
    {
      for (index = 1u; index < inbox->count; index++)
        {
          if (smart_band_event_priority(
                &inbox->items[inbox_physical_index(inbox, index)]) <
              smart_band_event_priority(
                &inbox->items[inbox_physical_index(inbox, lowest_index)]))
            {
              lowest_index = index;
            }
        }

      if (incoming_priority > smart_band_event_priority(
            &inbox->items[inbox_physical_index(inbox, lowest_index)]))
        {
          inbox_remove_at(inbox, lowest_index);
          inbox->evicted++;
        }
    }

  if (inbox->accepting && inbox->count < SMART_BAND_EVENT_QUEUE_CAPACITY)
    {
      tail = (inbox->head + inbox->count) % SMART_BAND_EVENT_QUEUE_CAPACITY;
      inbox->items[tail] = *event;
      inbox->count++;
      accepted = true;
    }
  else if (inbox->accepting)
    {
      inbox->dropped++;
    }

  inbox_unlock(inbox);
  return accepted;
}

bool smart_band_event_inbox_pop(smart_band_event_inbox_t *inbox,
                                smart_band_event_t *event)
{
  bool available = false;

  if (inbox == NULL || event == NULL || !inbox_lock(inbox))
    {
      return false;
    }

  if (inbox->count > 0)
    {
      *event = inbox->items[inbox->head];
      inbox->head = (inbox->head + 1) % SMART_BAND_EVENT_QUEUE_CAPACITY;
      inbox->count--;
      available = true;
    }

  inbox_unlock(inbox);
  return available;
}
