#ifndef SMART_BAND_EVENT_H
#define SMART_BAND_EVENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_EVENT_QUEUE_CAPACITY 16

typedef enum
{
  SMART_BAND_EVENT_NONE = 0,
  SMART_BAND_EVENT_METRICS_UPDATED,
  SMART_BAND_EVENT_TOUCH_ACTIVITY,
  SMART_BAND_EVENT_WRIST_RAISED,
  SMART_BAND_EVENT_WORKOUT_COMMAND,
  SMART_BAND_EVENT_WORKOUT_CHECKPOINT,
  SMART_BAND_EVENT_NOTIFICATION_RECEIVED,
  SMART_BAND_EVENT_NOTIFICATION_ACTION,
  SMART_BAND_EVENT_POWER_TIMEOUT,
  SMART_BAND_EVENT_BLE_CONNECTED,
  SMART_BAND_EVENT_BLE_DISCONNECTED,
  SMART_BAND_EVENT_SYNC_REQUEST,
  SMART_BAND_EVENT_SYNC_ACK,
  SMART_BAND_EVENT_STORAGE_FLUSH_REQUEST
} smart_band_event_type_t;

typedef enum
{
  SMART_BAND_EVENT_PRIORITY_LOW = 0,
  SMART_BAND_EVENT_PRIORITY_NORMAL,
  SMART_BAND_EVENT_PRIORITY_HIGH,
  SMART_BAND_EVENT_PRIORITY_CRITICAL
} smart_band_event_priority_t;

typedef enum
{
  SMART_BAND_NOTIFICATION_GENERIC = 0,
  SMART_BAND_NOTIFICATION_CALL
} smart_band_notification_kind_t;

typedef struct
{
  smart_band_event_type_t type;
  uint32_t monotonic_ms;
  union
  {
    struct
    {
      uint32_t mask;
    } metrics;
    struct
    {
      smart_band_notification_kind_t kind;
      uint8_t priority;
    } notification;
    struct
    {
      uint32_t code;
      int32_t value;
    } generic;
  } payload;
} smart_band_event_t;

typedef struct
{
  smart_band_event_t items[SMART_BAND_EVENT_QUEUE_CAPACITY];
  size_t count;
  unsigned int dropped;
  unsigned int evicted;
  unsigned int coalesced;
} smart_band_event_queue_t;

typedef struct
{
  void *context;
  bool (*lock)(void *context);
  void (*unlock)(void *context);
} smart_band_event_lock_t;

typedef struct
{
  smart_band_event_t items[SMART_BAND_EVENT_QUEUE_CAPACITY];
  size_t head;
  size_t count;
  unsigned int dropped;
  smart_band_event_lock_t lock;
  bool accepting;
} smart_band_event_inbox_t;

/* The priority queue remains owned by the UI controller. External callbacks
 * first copy events into the separately locked inbox. */

void smart_band_event_queue_init(smart_band_event_queue_t *queue);
bool smart_band_event_queue_push(smart_band_event_queue_t *queue,
                                 const smart_band_event_t *event);
bool smart_band_event_queue_pop(smart_band_event_queue_t *queue,
                                smart_band_event_t *event);
bool smart_band_event_queue_take(smart_band_event_queue_t *queue,
                                 smart_band_event_type_t type,
                                 smart_band_event_t *event);
size_t smart_band_event_queue_count(const smart_band_event_queue_t *queue);
smart_band_event_priority_t
smart_band_event_priority(const smart_band_event_t *event);
bool smart_band_event_inbox_init(smart_band_event_inbox_t *inbox,
                                 const smart_band_event_lock_t *lock);
bool smart_band_event_inbox_close(smart_band_event_inbox_t *inbox);
bool smart_band_event_inbox_post(smart_band_event_inbox_t *inbox,
                                 const smart_band_event_t *event);
bool smart_band_event_inbox_pop(smart_band_event_inbox_t *inbox,
                                smart_band_event_t *event);

#ifdef __cplusplus
}
#endif

#endif
