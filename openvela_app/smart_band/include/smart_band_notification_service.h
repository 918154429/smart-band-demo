#ifndef SMART_BAND_NOTIFICATION_SERVICE_H
#define SMART_BAND_NOTIFICATION_SERVICE_H

#include "smart_band_event.h"
#include "smart_band_notification_model.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
  smart_band_notification_model_t model;
  smart_band_notification_policy_t policy;
  smart_band_notification_presentation_t pending_presentation;
  smart_band_notification_service_stats_t stats;
  smart_band_notification_put_result_t last_put_result;
  smart_band_notification_action_result_t last_action_result;
  uint32_t pending_notification_id;
  smart_band_notification_type_t pending_notification_type;
  smart_band_notification_priority_t pending_notification_priority;
  bool presentation_pending;
  bool initialized;
} smart_band_notification_service_t;

int smart_band_notification_service_init(
  smart_band_notification_service_t *service);
void smart_band_notification_service_reset(
  smart_band_notification_service_t *service);
void smart_band_notification_service_set_policy(
  smart_band_notification_service_t *service,
  const smart_band_notification_policy_t *policy);
smart_band_notification_service_result_t
smart_band_notification_service_process(
  smart_band_notification_service_t *service,
  const smart_band_event_t *event);
bool smart_band_notification_service_peek_presentation(
  smart_band_notification_service_t *service, uint32_t *notification_id,
  smart_band_notification_presentation_t *presentation);
bool smart_band_notification_service_ack_presentation(
  smart_band_notification_service_t *service, uint32_t notification_id);
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
