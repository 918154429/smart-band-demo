#include "notification_demo.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

bool smart_band_notification_demo_build(
  uint32_t seed, uint32_t sequence,
  smart_band_notification_t *notification)
{
  uint32_t mixed;

  if (notification == NULL)
    {
      return false;
    }

  memset(notification, 0, sizeof(*notification));
  mixed = seed ^ (sequence * UINT32_C(2654435761));
  notification->id = mixed == 0u ? UINT32_MAX : mixed;
  notification->type =
    (smart_band_notification_type_t)(sequence % 4u);
  if (notification->type == SMART_BAND_NOTIFICATION_TYPE_CALL)
    {
      notification->priority = SMART_BAND_NOTIFICATION_PRIORITY_HIGH;
    }
  else if (notification->type == SMART_BAND_NOTIFICATION_TYPE_SYSTEM)
    {
      notification->priority = SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL;
    }
  else
    {
      notification->priority = SMART_BAND_NOTIFICATION_PRIORITY_NORMAL;
    }

  (void)snprintf(notification->source, sizeof(notification->source),
                 "demo-%" PRIu32, seed);
  (void)snprintf(notification->title, sizeof(notification->title),
                 "notification-%" PRIu32, sequence);
  (void)snprintf(notification->body, sizeof(notification->body),
                 "seed=%" PRIu32 " sequence=%" PRIu32, seed, sequence);
  notification->wall_timestamp = UINT64_C(1700000000) + sequence;
  return true;
}

smart_band_notification_put_result_t smart_band_notification_demo_inject(
  smart_band_notification_model_t *model, uint32_t seed, uint32_t sequence)
{
  smart_band_notification_t notification;
  smart_band_notification_input_t input;

  if (!smart_band_notification_demo_build(seed, sequence, &notification))
    {
      return SMART_BAND_NOTIFICATION_PUT_INVALID;
    }

  input.id = notification.id;
  input.type = notification.type;
  input.priority = notification.priority;
  input.source = notification.source;
  input.title = notification.title;
  input.body = notification.body;
  input.wall_timestamp = notification.wall_timestamp;
  return smart_band_notification_put(model, &input);
}
