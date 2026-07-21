#include "notification_demo.h"

#include <inttypes.h>
#include <stdio.h>

smart_band_notification_put_result_t smart_band_notification_demo_inject(
  smart_band_notification_model_t *model, uint32_t seed, uint32_t sequence)
{
  smart_band_notification_input_t input;
  char source[SMART_BAND_NOTIFICATION_SOURCE_CAPACITY];
  char title[SMART_BAND_NOTIFICATION_TITLE_CAPACITY];
  char body[SMART_BAND_NOTIFICATION_BODY_CAPACITY];
  uint32_t mixed = seed ^ (sequence * UINT32_C(2654435761));

  input.id = mixed == 0u ? UINT32_MAX : mixed;
  input.type = (smart_band_notification_type_t)(sequence % 4u);
  if (input.type == SMART_BAND_NOTIFICATION_TYPE_CALL)
    {
      input.priority = SMART_BAND_NOTIFICATION_PRIORITY_HIGH;
    }
  else if (input.type == SMART_BAND_NOTIFICATION_TYPE_SYSTEM)
    {
      input.priority = SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL;
    }
  else
    {
      input.priority = SMART_BAND_NOTIFICATION_PRIORITY_NORMAL;
    }

  (void)snprintf(source, sizeof(source), "demo-%" PRIu32, seed);
  (void)snprintf(title, sizeof(title), "notification-%" PRIu32, sequence);
  (void)snprintf(body, sizeof(body), "seed=%" PRIu32 " sequence=%" PRIu32,
                 seed, sequence);
  input.source = source;
  input.title = title;
  input.body = body;
  input.wall_timestamp = UINT64_C(1700000000) + sequence;
  return smart_band_notification_put(model, &input);
}
