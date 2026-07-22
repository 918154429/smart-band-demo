#ifndef SMART_BAND_NOTIFICATION_SERVICE_H
#define SMART_BAND_NOTIFICATION_SERVICE_H

#include "smart_band_event.h"
#include "smart_band_notification_model.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_NOTIFICATION_OVERLAY_TIMEOUT_MS UINT32_C(5000)
#define SMART_BAND_NOTIFICATION_EFFECT_CAPACITY 16u
#define SMART_BAND_NOTIFICATION_CALL_BACKLOG_CAPACITY 16u

typedef enum
{
  SMART_BAND_NOTIFICATION_SERVICE_INVALID = 0,
  SMART_BAND_NOTIFICATION_SERVICE_APPLIED,
  SMART_BAND_NOTIFICATION_SERVICE_NO_CHANGE,
  SMART_BAND_NOTIFICATION_SERVICE_REJECTED
} smart_band_notification_service_result_t;

typedef struct
{
  uint32_t received;
  uint32_t added;
  uint32_t updated;
  uint32_t rejected;
  uint32_t duplicates;
  uint32_t actions;
  uint32_t invalid;
  uint32_t presentations;
} smart_band_notification_service_stats_t;

typedef struct
{
  uint32_t notification_id;
  uint32_t generation;
  smart_band_notification_priority_t priority;
  smart_band_notification_haptic_t haptic;
  bool is_call;
  bool haptic_pending;
  bool wake_pending;
} smart_band_notification_effect_t;

typedef struct
{
  uint32_t notification_id;
  uint32_t generation;
  uint32_t received_ms;
} smart_band_notification_call_backlog_item_t;

typedef struct
{
  smart_band_notification_model_t model;
  smart_band_notification_policy_t policy;
  smart_band_notification_presentation_t pending_presentation;
  smart_band_notification_effect_t
    effects[SMART_BAND_NOTIFICATION_EFFECT_CAPACITY];
  smart_band_notification_call_backlog_item_t
    call_backlog[SMART_BAND_NOTIFICATION_CALL_BACKLOG_CAPACITY];
  smart_band_notification_service_stats_t stats;
  smart_band_notification_put_result_t last_put_result;
  smart_band_notification_action_result_t last_action_result;
  uint32_t pending_notification_id;
  uint32_t pending_generation;
  uint32_t pending_started_ms;
  uint32_t pending_deadline_ms;
  uint32_t next_generation;
  size_t effect_count;
  size_t call_backlog_count;
  smart_band_notification_type_t pending_notification_type;
  smart_band_notification_priority_t pending_notification_priority;
  bool presentation_acknowledged;
  bool presentation_has_deadline;
  bool presentation_pending;
  bool initialized;
} smart_band_notification_service_t;

int smart_band_notification_service_init(
  smart_band_notification_service_t *service);
void smart_band_notification_service_reset(
  smart_band_notification_service_t *service);
void smart_band_notification_service_set_policy(
  smart_band_notification_service_t *service,
  const smart_band_notification_policy_t *policy, uint32_t monotonic_ms);
bool smart_band_notification_service_tick(
  smart_band_notification_service_t *service, uint32_t monotonic_ms);
smart_band_notification_service_result_t
smart_band_notification_service_process(
  smart_band_notification_service_t *service,
  const smart_band_event_t *event);
/* peek returns only an unacknowledged visual generation. The active query
 * remains true until timeout, policy suppression, or a notification action. */
bool smart_band_notification_service_peek_presentation(
  smart_band_notification_service_t *service, uint32_t *notification_id,
  uint32_t *generation,
  smart_band_notification_presentation_t *presentation);
bool smart_band_notification_service_get_active_presentation(
  const smart_band_notification_service_t *service,
  uint32_t *notification_id, uint32_t *generation,
  smart_band_notification_presentation_t *presentation);
bool smart_band_notification_service_ack_presentation(
  smart_band_notification_service_t *service, uint32_t notification_id,
  uint32_t generation);
bool smart_band_notification_service_peek_haptic(
  smart_band_notification_service_t *service, uint32_t *notification_id,
  uint32_t *generation, smart_band_notification_haptic_t *haptic);
bool smart_band_notification_service_ack_haptic(
  smart_band_notification_service_t *service, uint32_t notification_id,
  uint32_t generation);
bool smart_band_notification_service_peek_wake(
  smart_band_notification_service_t *service, uint32_t *notification_id,
  uint32_t *generation);
bool smart_band_notification_service_ack_wake(
  smart_band_notification_service_t *service, uint32_t notification_id,
  uint32_t generation);
bool smart_band_notification_event_received(
  const smart_band_notification_input_t *input, uint32_t monotonic_ms,
  smart_band_event_t *event);
bool smart_band_notification_event_action(
  uint32_t id, smart_band_notification_command_t command,
  uint32_t monotonic_ms, smart_band_event_t *event);

#ifdef __cplusplus
}
#endif

#endif
