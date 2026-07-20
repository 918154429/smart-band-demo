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
  bool accepted = false;

  if (inbox == NULL || event == NULL || !inbox_lock(inbox))
    {
      return false;
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
