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

static smart_band_notification_priority_t normalize_priority(
  smart_band_notification_type_t type,
  smart_band_notification_priority_t priority)
{
  return type == SMART_BAND_NOTIFICATION_TYPE_CALL &&
             priority < SMART_BAND_NOTIFICATION_PRIORITY_HIGH ?
           SMART_BAND_NOTIFICATION_PRIORITY_HIGH : priority;
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
  service->presentation_acknowledged = false;
  service->presentation_has_deadline = false;
  service->pending_notification_id = 0u;
  service->pending_generation = 0u;
  service->pending_started_ms = 0u;
  service->pending_deadline_ms = 0u;
  service->pending_notification_type = SMART_BAND_NOTIFICATION_TYPE_APP;
  service->pending_notification_priority =
    SMART_BAND_NOTIFICATION_PRIORITY_LOW;
  memset(&service->pending_presentation, 0,
         sizeof(service->pending_presentation));
}

static bool generation_in_use(
  const smart_band_notification_service_t *service, uint32_t generation)
{
  size_t index;

  if (generation == 0u)
    {
      return true;
    }

  if (service->presentation_pending &&
      service->pending_generation == generation)
    {
      return true;
    }

  for (index = 0u; index < service->effect_count; index++)
    {
      if (service->effects[index].generation == generation)
        {
          return true;
        }
    }

  for (index = 0u; index < service->call_backlog_count; index++)
    {
      if (service->call_backlog[index].generation == generation)
        {
          return true;
        }
    }

  return false;
}

static uint32_t next_generation(smart_band_notification_service_t *service)
{
  do
    {
      service->next_generation++;
    }
  while (generation_in_use(service, service->next_generation));

  return service->next_generation;
}

static void remove_effect_at(smart_band_notification_service_t *service,
                             size_t index)
{
  if (index + 1u < service->effect_count)
    {
      memmove(&service->effects[index], &service->effects[index + 1u],
              (service->effect_count - index - 1u) *
                sizeof(service->effects[0]));
    }

  service->effect_count--;
  memset(&service->effects[service->effect_count], 0,
         sizeof(service->effects[0]));
}

static void remove_effects_for_notification(
  smart_band_notification_service_t *service, uint32_t notification_id);

static unsigned int effect_rank(
  const smart_band_notification_effect_t *effect)
{
  return (unsigned int)effect->priority * 2u +
         (effect->is_call ? 1u : 0u);
}

static void queue_effect(smart_band_notification_service_t *service,
                         const smart_band_notification_t *notification,
                         uint32_t generation,
                         const smart_band_notification_presentation_t
                           *presentation)
{
  smart_band_notification_effect_t *effect;
  size_t index;
  size_t lowest_index = 0u;
  smart_band_notification_effect_t incoming;

  if ((presentation->haptic == SMART_BAND_NOTIFICATION_HAPTIC_NONE &&
       !presentation->wake_request) || notification == NULL)
    {
      return;
    }

  remove_effects_for_notification(service, notification->id);
  memset(&incoming, 0, sizeof(incoming));
  incoming.notification_id = notification->id;
  incoming.generation = generation;
  incoming.priority = notification->priority;
  incoming.haptic = presentation->haptic;
  incoming.is_call =
    notification->type == SMART_BAND_NOTIFICATION_TYPE_CALL;
  incoming.haptic_pending =
    presentation->haptic != SMART_BAND_NOTIFICATION_HAPTIC_NONE;
  incoming.wake_pending = presentation->wake_request;
  if (service->effect_count >= SMART_BAND_NOTIFICATION_EFFECT_CAPACITY)
    {
      for (index = 1u; index < service->effect_count; index++)
        {
          if (effect_rank(&service->effects[index]) <
              effect_rank(&service->effects[lowest_index]))
            {
              lowest_index = index;
            }
        }

      if (effect_rank(&incoming) <=
          effect_rank(&service->effects[lowest_index]))
        {
          return;
        }
      remove_effect_at(service, lowest_index);
    }

  effect = &service->effects[service->effect_count++];
  *effect = incoming;
}

static void remove_effects_for_notification(
  smart_band_notification_service_t *service, uint32_t notification_id)
{
  size_t index = 0u;

  while (index < service->effect_count)
    {
      if (service->effects[index].notification_id == notification_id)
        {
          remove_effect_at(service, index);
        }
      else
        {
          index++;
        }
    }
}

static void remove_call_backlog_at(
  smart_band_notification_service_t *service, size_t index)
{
  if (index + 1u < service->call_backlog_count)
    {
      memmove(&service->call_backlog[index],
              &service->call_backlog[index + 1u],
              (service->call_backlog_count - index - 1u) *
                sizeof(service->call_backlog[0]));
    }

  service->call_backlog_count--;
  memset(&service->call_backlog[service->call_backlog_count], 0,
         sizeof(service->call_backlog[0]));
}

static void remove_call_from_backlog(
  smart_band_notification_service_t *service, uint32_t notification_id)
{
  size_t index = 0u;

  while (index < service->call_backlog_count)
    {
      if (service->call_backlog[index].notification_id == notification_id)
        {
          remove_call_backlog_at(service, index);
        }
      else
        {
          index++;
        }
    }
}

static bool queue_call(smart_band_notification_service_t *service,
                       uint32_t notification_id, uint32_t generation,
                       uint32_t received_ms)
{
  size_t index;

  for (index = 0u; index < service->call_backlog_count; index++)
    {
      if (service->call_backlog[index].notification_id == notification_id)
        {
          service->call_backlog[index].generation = generation;
          service->call_backlog[index].received_ms = received_ms;
          return true;
        }
    }

  if (service->call_backlog_count >=
      SMART_BAND_NOTIFICATION_CALL_BACKLOG_CAPACITY)
    {
      return false;
    }

  service->call_backlog[service->call_backlog_count].notification_id =
    notification_id;
  service->call_backlog[service->call_backlog_count].generation = generation;
  service->call_backlog[service->call_backlog_count].received_ms = received_ms;
  service->call_backlog_count++;
  return true;
}

static void activate_presentation(
  smart_band_notification_service_t *service,
  const smart_band_notification_t *notification,
  const smart_band_notification_presentation_t *presentation,
  uint32_t generation, uint32_t monotonic_ms)
{
  service->pending_presentation = *presentation;
  service->pending_notification_id = notification->id;
  service->pending_notification_type = notification->type;
  service->pending_notification_priority = notification->priority;
  service->pending_generation = generation;
  service->pending_started_ms = monotonic_ms;
  service->pending_deadline_ms =
    notification->type != SMART_BAND_NOTIFICATION_TYPE_CALL &&
        presentation->overlay ?
      monotonic_ms + SMART_BAND_NOTIFICATION_OVERLAY_TIMEOUT_MS :
      0u;
  service->presentation_has_deadline =
    notification->type != SMART_BAND_NOTIFICATION_TYPE_CALL &&
    presentation->overlay;
  service->presentation_acknowledged = false;
  service->presentation_pending = true;
  service->stats.presentations++;
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

static bool promote_next_call(smart_band_notification_service_t *service,
                              uint32_t monotonic_ms);

static void consider_presentation(
  smart_band_notification_service_t *service,
  const smart_band_notification_t *notification,
  const smart_band_notification_presentation_t *presentation,
  uint32_t monotonic_ms)
{
  uint32_t generation;

  if (presentation->center_only ||
      (!presentation->overlay && !presentation->full_screen))
    {
      if (service->presentation_pending &&
          service->pending_notification_id == notification->id)
        {
          clear_presentation(service);
          (void)promote_next_call(service, monotonic_ms);
        }
      return;
    }

  generation = next_generation(service);
  queue_effect(service, notification, generation, presentation);
  if (service->presentation_pending &&
      service->pending_notification_type == SMART_BAND_NOTIFICATION_TYPE_CALL &&
      notification->type == SMART_BAND_NOTIFICATION_TYPE_CALL &&
      service->pending_notification_id != notification->id)
    {
      (void)queue_call(service, notification->id, generation, monotonic_ms);
      return;
    }

  if (!should_replace_presentation(service, notification))
    {
      return;
    }

  activate_presentation(service, notification, presentation, generation,
                        monotonic_ms);
}

static bool promote_next_call(smart_band_notification_service_t *service,
                              uint32_t monotonic_ms)
{
  while (!service->presentation_pending &&
         service->call_backlog_count > 0u)
    {
      smart_band_notification_call_backlog_item_t item =
        service->call_backlog[0];
      const smart_band_notification_t *stored;
      smart_band_notification_presentation_t presentation;

      remove_call_backlog_at(service, 0u);
      stored = smart_band_notification_find(&service->model,
                                             item.notification_id);
      if (smart_band_notification_decide_presentation(
            stored, &service->policy, &presentation) &&
          !presentation.center_only &&
          (presentation.overlay || presentation.full_screen))
        {
          activate_presentation(service, stored, &presentation,
                                item.generation,
                                monotonic_ms == 0u ? item.received_ms :
                                                     monotonic_ms);
          return true;
        }
    }

  return false;
}

static void reconcile_removed_notifications(
  smart_band_notification_service_t *service,
  const uint32_t *ids_before, size_t count_before, uint32_t monotonic_ms)
{
  size_t index;
  bool cleared_active = false;

  for (index = 0u; index < count_before; index++)
    {
      uint32_t id = ids_before[index];

      if (smart_band_notification_find(&service->model, id) == NULL)
        {
          remove_effects_for_notification(service, id);
          remove_call_from_backlog(service, id);
          if (service->presentation_pending &&
              service->pending_notification_id == id)
            {
              clear_presentation(service);
              cleared_active = true;
            }
        }
    }

  if (cleared_active)
    {
      (void)promote_next_call(service, monotonic_ms);
    }
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
  const smart_band_notification_policy_t *policy, uint32_t monotonic_ms)
{
  if (service == NULL || !service->initialized || policy == NULL)
    {
      return;
    }

  service->policy = *policy;
  if (policy->dnd_enabled)
    {
      clear_presentation(service);
      memset(service->call_backlog, 0, sizeof(service->call_backlog));
      service->call_backlog_count = 0u;
      memset(service->effects, 0, sizeof(service->effects));
      service->effect_count = 0u;
    }
  else if (service->presentation_pending)
    {
      const smart_band_notification_t *stored =
        smart_band_notification_find(&service->model,
                                     service->pending_notification_id);
      smart_band_notification_presentation_t presentation;

      if (smart_band_notification_decide_presentation(
            stored, policy, &presentation))
        {
          if (presentation.center_only)
            {
              clear_presentation(service);
              (void)promote_next_call(service, monotonic_ms);
            }
          else if (memcmp(&service->pending_presentation, &presentation,
                          sizeof(presentation)) != 0)
            {
              activate_presentation(service, stored, &presentation,
                                    next_generation(service), monotonic_ms);
            }
        }
    }
}

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
  return now_ms - deadline_ms < UINT32_C(0x80000000);
}

bool smart_band_notification_service_tick(
  smart_band_notification_service_t *service, uint32_t monotonic_ms)
{
  if (service == NULL || !service->initialized ||
      !service->presentation_pending ||
      !service->presentation_has_deadline ||
      !deadline_reached(monotonic_ms, service->pending_deadline_ms))
    {
      return false;
    }

  clear_presentation(service);
  (void)promote_next_call(service, monotonic_ms);
  return true;
}

static smart_band_notification_service_result_t process_received(
  smart_band_notification_service_t *service,
  const smart_band_event_t *event)
{
  const smart_band_notification_t *stored;
  smart_band_notification_input_t input;
  smart_band_notification_presentation_t presentation;
  uint32_t ids_before[SMART_BAND_NOTIFICATION_CAPACITY];
  size_t count_before;
  size_t index;

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
  input.priority = normalize_priority(
    input.type, event->payload.notification_received.priority);
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

  count_before = smart_band_notification_count(&service->model);
  for (index = 0u; index < count_before; index++)
    {
      ids_before[index] = smart_band_notification_at(&service->model,
                                                      index)->id;
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

  reconcile_removed_notifications(service, ids_before, count_before,
                                  event->monotonic_ms);

  stored = smart_band_notification_find(&service->model, input.id);
  remove_effects_for_notification(service, input.id);
  if (stored->type != SMART_BAND_NOTIFICATION_TYPE_CALL)
    {
      remove_call_from_backlog(service, input.id);
    }
  if (!smart_band_notification_decide_presentation(
        stored, &service->policy, &presentation))
    {
      service->stats.invalid++;
      return SMART_BAND_NOTIFICATION_SERVICE_INVALID;
    }

  consider_presentation(service, stored, &presentation,
                        event->monotonic_ms);
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
      remove_effects_for_notification(service, id);
      remove_call_from_backlog(service, id);
      if (service->presentation_pending &&
          service->pending_notification_id == id)
        {
          clear_presentation(service);
          (void)promote_next_call(service, event->monotonic_ms);
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
  uint32_t *generation,
  smart_band_notification_presentation_t *presentation)
{
  if (service == NULL || !service->initialized ||
      !service->presentation_pending ||
      service->presentation_acknowledged || notification_id == NULL ||
      generation == NULL || presentation == NULL)
    {
      return false;
    }

  *notification_id = service->pending_notification_id;
  *generation = service->pending_generation;
  *presentation = service->pending_presentation;
  return true;
}

bool smart_band_notification_service_get_active_presentation(
  const smart_band_notification_service_t *service,
  uint32_t *notification_id, uint32_t *generation,
  smart_band_notification_presentation_t *presentation)
{
  if (service == NULL || !service->initialized ||
      !service->presentation_pending || notification_id == NULL ||
      generation == NULL || presentation == NULL)
    {
      return false;
    }

  *notification_id = service->pending_notification_id;
  *generation = service->pending_generation;
  *presentation = service->pending_presentation;
  return true;
}

bool smart_band_notification_service_ack_presentation(
  smart_band_notification_service_t *service, uint32_t notification_id,
  uint32_t generation)
{
  if (service == NULL || !service->initialized ||
      !service->presentation_pending || notification_id == 0u ||
      generation == 0u ||
      service->pending_notification_id != notification_id ||
      service->pending_generation != generation)
    {
      return false;
    }

  service->presentation_acknowledged = true;
  return true;
}

bool smart_band_notification_service_peek_haptic(
  smart_band_notification_service_t *service, uint32_t *notification_id,
  uint32_t *generation, smart_band_notification_haptic_t *haptic)
{
  size_t index;

  if (service == NULL || !service->initialized || notification_id == NULL ||
      generation == NULL || haptic == NULL)
    {
      return false;
    }

  for (index = 0u; index < service->effect_count; index++)
    {
      if (service->effects[index].haptic_pending)
        {
          *notification_id = service->effects[index].notification_id;
          *generation = service->effects[index].generation;
          *haptic = service->effects[index].haptic;
          return true;
        }
    }

  return false;
}

bool smart_band_notification_service_ack_haptic(
  smart_band_notification_service_t *service, uint32_t notification_id,
  uint32_t generation)
{
  size_t index;

  if (service == NULL || !service->initialized || notification_id == 0u ||
      generation == 0u)
    {
      return false;
    }

  for (index = 0u; index < service->effect_count; index++)
    {
      smart_band_notification_effect_t *effect = &service->effects[index];

      if (effect->notification_id == notification_id &&
          effect->generation == generation && effect->haptic_pending)
        {
          effect->haptic_pending = false;
          if (!effect->wake_pending)
            {
              remove_effect_at(service, index);
            }
          return true;
        }
    }

  return false;
}

bool smart_band_notification_service_peek_wake(
  smart_band_notification_service_t *service, uint32_t *notification_id,
  uint32_t *generation)
{
  size_t index;

  if (service == NULL || !service->initialized || notification_id == NULL ||
      generation == NULL)
    {
      return false;
    }

  for (index = 0u; index < service->effect_count; index++)
    {
      if (service->effects[index].wake_pending)
        {
          *notification_id = service->effects[index].notification_id;
          *generation = service->effects[index].generation;
          return true;
        }
    }

  return false;
}

bool smart_band_notification_service_ack_wake(
  smart_band_notification_service_t *service, uint32_t notification_id,
  uint32_t generation)
{
  size_t index;

  if (service == NULL || !service->initialized || notification_id == 0u ||
      generation == 0u)
    {
      return false;
    }

  for (index = 0u; index < service->effect_count; index++)
    {
      smart_band_notification_effect_t *effect = &service->effects[index];

      if (effect->notification_id == notification_id &&
          effect->generation == generation && effect->wake_pending)
        {
          effect->wake_pending = false;
          if (!effect->haptic_pending)
            {
              remove_effect_at(service, index);
            }
          return true;
        }
    }

  return false;
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
  event->payload.notification_received.priority =
    normalize_priority(input->type, input->priority);
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
