#include "smart_band_notification_service.h"

#include <string.h>

static bool valid_type(smart_band_notification_type_t type)
{
  return type >= SMART_BAND_NOTIFICATION_TYPE_CALL &&
         type <= SMART_BAND_NOTIFICATION_TYPE_SYSTEM;
}

static bool valid_priority(smart_band_notification_priority_t priority)
{
  return priority >= SMART_BAND_NOTIFICATION_PRIORITY_LOW &&
         priority <= SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL;
}

static bool valid_command(smart_band_notification_command_t command)
{
  return command >= SMART_BAND_NOTIFICATION_COMMAND_READ &&
         command <= SMART_BAND_NOTIFICATION_COMMAND_DELETE;
}

static void copy_text(char *destination, size_t capacity, const char *source)
{
  size_t length = strlen(source);

  if (length >= capacity)
    {
      length = capacity - 1u;
    }

  memcpy(destination, source, length);
  destination[length] = '\0';
}

static bool terminated_received_payload(const smart_band_event_t *event)
{
  return event->payload.notification_received
           .source[SMART_BAND_NOTIFICATION_SOURCE_CAPACITY - 1u] == '\0' &&
         event->payload.notification_received
           .title[SMART_BAND_NOTIFICATION_TITLE_CAPACITY - 1u] == '\0' &&
         event->payload.notification_received
           .body[SMART_BAND_NOTIFICATION_BODY_CAPACITY - 1u] == '\0';
}

static bool same_content(const smart_band_notification_t *stored,
                         const smart_band_notification_input_t *input)
{
  return stored != NULL && stored->type == input->type &&
         stored->priority == input->priority &&
         stored->wall_timestamp == input->wall_timestamp &&
         strcmp(stored->source, input->source) == 0 &&
         strcmp(stored->title, input->title) == 0 &&
         strcmp(stored->body, input->body) == 0;
}

static void clear_presentation(smart_band_notification_service_t *service)
{
  service->presentation_pending = false;
  service->pending_notification_id = 0u;
  service->pending_notification_type = SMART_BAND_NOTIFICATION_TYPE_APP;
  service->pending_notification_priority =
    SMART_BAND_NOTIFICATION_PRIORITY_LOW;
  memset(&service->pending_presentation, 0,
         sizeof(service->pending_presentation));
}

static bool should_replace_presentation(
  const smart_band_notification_service_t *service,
  const smart_band_notification_t *notification)
{
  if (!service->presentation_pending ||
      service->pending_notification_id == notification->id)
    {
      return true;
    }

  if (service->pending_notification_type ==
        SMART_BAND_NOTIFICATION_TYPE_CALL)
    {
      return false;
    }

  if (notification->type == SMART_BAND_NOTIFICATION_TYPE_CALL)
    {
      return true;
    }

  return notification->priority >= service->pending_notification_priority;
}

static void consider_presentation(
  smart_band_notification_service_t *service,
  const smart_band_notification_t *notification,
  const smart_band_notification_presentation_t *presentation)
{
  if (presentation->center_only ||
      !should_replace_presentation(service, notification))
    {
      return;
    }

  service->pending_presentation = *presentation;
  service->pending_notification_id = notification->id;
  service->pending_notification_type = notification->type;
  service->pending_notification_priority = notification->priority;
  service->presentation_pending = true;
  service->stats.presentations++;
}

int smart_band_notification_service_init(
  smart_band_notification_service_t *service)
{
  if (service == NULL)
    {
      return -1;
    }

  memset(service, 0, sizeof(*service));
  smart_band_notification_model_init(&service->model);
  service->initialized = true;
  return 0;
}

void smart_band_notification_service_reset(
  smart_band_notification_service_t *service)
{
  if (service != NULL)
    {
      memset(service, 0, sizeof(*service));
    }
}

void smart_band_notification_service_set_policy(
  smart_band_notification_service_t *service,
  const smart_band_notification_policy_t *policy)
{
  if (service == NULL || !service->initialized || policy == NULL)
    {
      return;
    }

  service->policy = *policy;
  if (policy->dnd_enabled)
    {
      clear_presentation(service);
    }
  else if (policy->workout_active && service->presentation_pending)
    {
      const smart_band_notification_t *stored =
        smart_band_notification_find(&service->model,
                                     service->pending_notification_id);
      smart_band_notification_presentation_t presentation;

      if (smart_band_notification_decide_presentation(
            stored, policy, &presentation))
        {
          service->pending_presentation = presentation;
          if (presentation.center_only)
            {
              clear_presentation(service);
            }
        }
    }
}

static smart_band_notification_service_result_t process_received(
  smart_band_notification_service_t *service,
  const smart_band_event_t *event)
{
  const smart_band_notification_t *stored;
  smart_band_notification_input_t input;
  smart_band_notification_presentation_t presentation;

  service->stats.received++;
  if (event->payload.notification_received.id == 0u ||
      !valid_type(event->payload.notification_received.type) ||
      !valid_priority(event->payload.notification_received.priority) ||
      !terminated_received_payload(event))
    {
      service->last_put_result = SMART_BAND_NOTIFICATION_PUT_INVALID;
      service->stats.invalid++;
      return SMART_BAND_NOTIFICATION_SERVICE_INVALID;
    }

  input.id = event->payload.notification_received.id;
  input.type = event->payload.notification_received.type;
  input.priority = event->payload.notification_received.priority;
  input.source = event->payload.notification_received.source;
  input.title = event->payload.notification_received.title;
  input.body = event->payload.notification_received.body;
  input.wall_timestamp = event->payload.notification_received.wall_timestamp;
  stored = smart_band_notification_find(&service->model, input.id);
  if (same_content(stored, &input))
    {
      service->last_put_result = SMART_BAND_NOTIFICATION_PUT_UPDATED;
      service->stats.duplicates++;
      return SMART_BAND_NOTIFICATION_SERVICE_NO_CHANGE;
    }

  service->last_put_result = smart_band_notification_put(&service->model,
                                                         &input);
  if (service->last_put_result == SMART_BAND_NOTIFICATION_PUT_ADDED)
    {
      service->stats.added++;
    }
  else if (service->last_put_result == SMART_BAND_NOTIFICATION_PUT_UPDATED)
    {
      service->stats.updated++;
    }
  else if (service->last_put_result == SMART_BAND_NOTIFICATION_PUT_FULL ||
           service->last_put_result == SMART_BAND_NOTIFICATION_PUT_PROTECTED)
    {
      service->stats.rejected++;
      return SMART_BAND_NOTIFICATION_SERVICE_REJECTED;
    }
  else
    {
      service->stats.invalid++;
      return SMART_BAND_NOTIFICATION_SERVICE_INVALID;
    }

  stored = smart_band_notification_find(&service->model, input.id);
  if (!smart_band_notification_decide_presentation(
        stored, &service->policy, &presentation))
    {
      service->stats.invalid++;
      return SMART_BAND_NOTIFICATION_SERVICE_INVALID;
    }

  consider_presentation(service, stored, &presentation);
  return SMART_BAND_NOTIFICATION_SERVICE_APPLIED;
}

static smart_band_notification_service_result_t process_action(
  smart_band_notification_service_t *service,
  const smart_band_event_t *event)
{
  uint32_t id = event->payload.notification_action.id;

  service->stats.actions++;
  service->last_action_result = smart_band_notification_apply(
    &service->model, id, event->payload.notification_action.command);
  if (service->last_action_result == SMART_BAND_NOTIFICATION_ACTION_APPLIED)
    {
      if (service->presentation_pending &&
          service->pending_notification_id == id)
        {
          clear_presentation(service);
        }

      return SMART_BAND_NOTIFICATION_SERVICE_APPLIED;
    }

  if (service->last_action_result ==
      SMART_BAND_NOTIFICATION_ACTION_NO_CHANGE)
    {
      return SMART_BAND_NOTIFICATION_SERVICE_NO_CHANGE;
    }

  if (service->last_action_result ==
      SMART_BAND_NOTIFICATION_ACTION_NOT_FOUND)
    {
      service->stats.rejected++;
      return SMART_BAND_NOTIFICATION_SERVICE_REJECTED;
    }

  service->stats.invalid++;
  return SMART_BAND_NOTIFICATION_SERVICE_INVALID;
}

smart_band_notification_service_result_t
smart_band_notification_service_process(
  smart_band_notification_service_t *service,
  const smart_band_event_t *event)
{
  if (service == NULL || !service->initialized || event == NULL)
    {
      return SMART_BAND_NOTIFICATION_SERVICE_INVALID;
    }

  if (event->type == SMART_BAND_EVENT_NOTIFICATION_RECEIVED)
    {
      return process_received(service, event);
    }

  if (event->type == SMART_BAND_EVENT_NOTIFICATION_ACTION &&
      event->payload.notification_action.id != 0u &&
      valid_command(event->payload.notification_action.command))
    {
      return process_action(service, event);
    }

  service->stats.invalid++;
  return SMART_BAND_NOTIFICATION_SERVICE_INVALID;
}

bool smart_band_notification_service_peek_presentation(
  smart_band_notification_service_t *service, uint32_t *notification_id,
  smart_band_notification_presentation_t *presentation)
{
  if (service == NULL || !service->initialized ||
      !service->presentation_pending || notification_id == NULL ||
      presentation == NULL)
    {
      return false;
    }

  *notification_id = service->pending_notification_id;
  *presentation = service->pending_presentation;
  return true;
}

bool smart_band_notification_service_ack_presentation(
  smart_band_notification_service_t *service, uint32_t notification_id)
{
  if (service == NULL || !service->initialized ||
      !service->presentation_pending || notification_id == 0u ||
      service->pending_notification_id != notification_id)
    {
      return false;
    }

  clear_presentation(service);
  return true;
}

bool smart_band_notification_event_received(
  const smart_band_notification_input_t *input, uint32_t monotonic_ms,
  smart_band_event_t *event)
{
  if (input == NULL || event == NULL || input->id == 0u ||
      !valid_type(input->type) || !valid_priority(input->priority) ||
      input->source == NULL || input->title == NULL || input->body == NULL)
    {
      return false;
    }

  memset(event, 0, sizeof(*event));
  event->type = SMART_BAND_EVENT_NOTIFICATION_RECEIVED;
  event->monotonic_ms = monotonic_ms;
  event->payload.notification_received.id = input->id;
  event->payload.notification_received.type = input->type;
  event->payload.notification_received.priority = input->priority;
  copy_text(event->payload.notification_received.source,
            sizeof(event->payload.notification_received.source),
            input->source);
  copy_text(event->payload.notification_received.title,
            sizeof(event->payload.notification_received.title), input->title);
  copy_text(event->payload.notification_received.body,
            sizeof(event->payload.notification_received.body), input->body);
  event->payload.notification_received.wall_timestamp = input->wall_timestamp;
  return true;
}

bool smart_band_notification_event_action(
  uint32_t id, smart_band_notification_command_t command,
  uint32_t monotonic_ms, smart_band_event_t *event)
{
  if (id == 0u || !valid_command(command) || event == NULL)
    {
      return false;
    }

  memset(event, 0, sizeof(*event));
  event->type = SMART_BAND_EVENT_NOTIFICATION_ACTION;
  event->monotonic_ms = monotonic_ms;
  event->payload.notification_action.id = id;
  event->payload.notification_action.command = command;
  return true;
}
