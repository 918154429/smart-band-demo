#include "smart_band_notification_model.h"

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

static bool valid_action_state(
  smart_band_notification_action_state_t action_state)
{
  return action_state >= SMART_BAND_NOTIFICATION_ACTION_NONE &&
         action_state <= SMART_BAND_NOTIFICATION_ACTION_REJECTED;
}

static bool valid_model(const smart_band_notification_model_t *model)
{
  return model != NULL && model->count <= SMART_BAND_NOTIFICATION_CAPACITY;
}

static bool valid_input(const smart_band_notification_input_t *input)
{
  return input != NULL && input->id != 0u && valid_type(input->type) &&
         valid_priority(input->priority) && input->source != NULL &&
         input->title != NULL && input->body != NULL;
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

static void update_content(smart_band_notification_t *notification,
                           const smart_band_notification_input_t *input)
{
  notification->priority = input->priority;
  notification->wall_timestamp = input->wall_timestamp;
  copy_text(notification->source, sizeof(notification->source), input->source);
  copy_text(notification->title, sizeof(notification->title), input->title);
  copy_text(notification->body, sizeof(notification->body), input->body);
}

static bool protected_call(const smart_band_notification_t *notification)
{
  return notification->type == SMART_BAND_NOTIFICATION_TYPE_CALL &&
         notification->priority >= SMART_BAND_NOTIFICATION_PRIORITY_HIGH &&
         !notification->dismissed &&
         notification->action_state !=
           SMART_BAND_NOTIFICATION_ACTION_ACCEPTED &&
         notification->action_state !=
           SMART_BAND_NOTIFICATION_ACTION_REJECTED;
}

static unsigned int handled_rank(
  const smart_band_notification_t *notification)
{
  if (notification->dismissed ||
      notification->action_state != SMART_BAND_NOTIFICATION_ACTION_NONE)
    {
      return 2u;
    }

  return notification->read ? 1u : 0u;
}

static bool is_better_eviction_candidate(
  const smart_band_notification_t *candidate,
  const smart_band_notification_t *selected)
{
  unsigned int candidate_handled;
  unsigned int selected_handled;

  if (candidate->priority != selected->priority)
    {
      return candidate->priority < selected->priority;
    }

  candidate_handled = handled_rank(candidate);
  selected_handled = handled_rank(selected);
  if (candidate_handled != selected_handled)
    {
      return candidate_handled > selected_handled;
    }

  return candidate->wall_timestamp < selected->wall_timestamp;
}

static void remove_at(smart_band_notification_model_t *model, size_t index)
{
  if (index + 1u < model->count)
    {
      memmove(&model->items[index], &model->items[index + 1u],
              (model->count - index - 1u) * sizeof(model->items[0]));
    }

  model->count--;
}

static smart_band_notification_t *find_mutable(
  smart_band_notification_model_t *model, uint32_t id)
{
  size_t index;

  if (!valid_model(model) || id == 0u)
    {
      return NULL;
    }

  for (index = 0; index < model->count; index++)
    {
      if (model->items[index].id == id)
        {
          return &model->items[index];
        }
    }

  return NULL;
}

void smart_band_notification_model_init(smart_band_notification_model_t *model)
{
  if (model != NULL)
    {
      memset(model, 0, sizeof(*model));
    }
}

size_t smart_band_notification_count(
  const smart_band_notification_model_t *model)
{
  return valid_model(model) ? model->count : 0u;
}

const smart_band_notification_t *smart_band_notification_at(
  const smart_band_notification_model_t *model, size_t index)
{
  if (!valid_model(model) || index >= model->count)
    {
      return NULL;
    }

  return &model->items[index];
}

const smart_band_notification_t *smart_band_notification_find(
  const smart_band_notification_model_t *model, uint32_t id)
{
  size_t index;

  if (!valid_model(model) || id == 0u)
    {
      return NULL;
    }

  for (index = 0; index < model->count; index++)
    {
      if (model->items[index].id == id)
        {
          return &model->items[index];
        }
    }

  return NULL;
}

smart_band_notification_put_result_t smart_band_notification_put(
  smart_band_notification_model_t *model,
  const smart_band_notification_input_t *input)
{
  smart_band_notification_t *existing;
  smart_band_notification_t notification;
  size_t candidate = 0u;
  size_t index;
  bool found_candidate = false;

  if (!valid_model(model) || !valid_input(input))
    {
      return SMART_BAND_NOTIFICATION_PUT_INVALID;
    }

  existing = find_mutable(model, input->id);
  if (existing != NULL)
    {
      if (existing->type != input->type)
        {
          return SMART_BAND_NOTIFICATION_PUT_INVALID;
        }

      update_content(existing, input);
      return SMART_BAND_NOTIFICATION_PUT_UPDATED;
    }

  if (model->count == SMART_BAND_NOTIFICATION_CAPACITY)
    {
      for (index = 0u; index < model->count; index++)
        {
          if (protected_call(&model->items[index]))
            {
              continue;
            }

          if (!found_candidate || is_better_eviction_candidate(
                                    &model->items[index],
                                    &model->items[candidate]))
            {
              candidate = index;
              found_candidate = true;
            }
        }

      if (!found_candidate)
        {
          return SMART_BAND_NOTIFICATION_PUT_PROTECTED;
        }

      if (model->items[candidate].priority > input->priority ||
          (model->items[candidate].priority == input->priority &&
           handled_rank(&model->items[candidate]) == 0u))
        {
          return SMART_BAND_NOTIFICATION_PUT_FULL;
        }

      remove_at(model, candidate);
    }

  memset(&notification, 0, sizeof(notification));
  notification.id = input->id;
  notification.type = input->type;
  notification.action_state = SMART_BAND_NOTIFICATION_ACTION_NONE;
  update_content(&notification, input);
  model->items[model->count++] = notification;
  return SMART_BAND_NOTIFICATION_PUT_ADDED;
}

smart_band_notification_action_result_t smart_band_notification_apply(
  smart_band_notification_model_t *model, uint32_t id,
  smart_band_notification_command_t command)
{
  smart_band_notification_t *notification;

  if (!valid_model(model) || id == 0u ||
      command < SMART_BAND_NOTIFICATION_COMMAND_READ ||
      command > SMART_BAND_NOTIFICATION_COMMAND_REJECT)
    {
      return SMART_BAND_NOTIFICATION_ACTION_INVALID;
    }

  notification = find_mutable(model, id);
  if (notification == NULL)
    {
      return SMART_BAND_NOTIFICATION_ACTION_NOT_FOUND;
    }

  if (command == SMART_BAND_NOTIFICATION_COMMAND_READ)
    {
      if (notification->read)
        {
          return SMART_BAND_NOTIFICATION_ACTION_NO_CHANGE;
        }

      notification->read = true;
      return SMART_BAND_NOTIFICATION_ACTION_APPLIED;
    }

  if (command == SMART_BAND_NOTIFICATION_COMMAND_DISMISS)
    {
      if (notification->action_state ==
            SMART_BAND_NOTIFICATION_ACTION_ACCEPTED ||
          notification->action_state == SMART_BAND_NOTIFICATION_ACTION_REJECTED)
        {
          return SMART_BAND_NOTIFICATION_ACTION_INVALID;
        }

      if (notification->dismissed)
        {
          return SMART_BAND_NOTIFICATION_ACTION_NO_CHANGE;
        }

      notification->read = true;
      notification->dismissed = true;
      notification->action_state = SMART_BAND_NOTIFICATION_ACTION_DISMISSED;
      return SMART_BAND_NOTIFICATION_ACTION_APPLIED;
    }

  if (notification->type != SMART_BAND_NOTIFICATION_TYPE_CALL)
    {
      return SMART_BAND_NOTIFICATION_ACTION_INVALID;
    }

  if (notification->action_state == SMART_BAND_NOTIFICATION_ACTION_DISMISSED)
    {
      return SMART_BAND_NOTIFICATION_ACTION_INVALID;
    }

  if ((command == SMART_BAND_NOTIFICATION_COMMAND_ACCEPT &&
       notification->action_state == SMART_BAND_NOTIFICATION_ACTION_ACCEPTED) ||
      (command == SMART_BAND_NOTIFICATION_COMMAND_REJECT &&
       notification->action_state == SMART_BAND_NOTIFICATION_ACTION_REJECTED))
    {
      return SMART_BAND_NOTIFICATION_ACTION_NO_CHANGE;
    }

  if (notification->action_state != SMART_BAND_NOTIFICATION_ACTION_NONE)
    {
      return SMART_BAND_NOTIFICATION_ACTION_INVALID;
    }

  notification->read = true;
  notification->dismissed = true;
  notification->action_state =
    command == SMART_BAND_NOTIFICATION_COMMAND_ACCEPT ?
      SMART_BAND_NOTIFICATION_ACTION_ACCEPTED :
      SMART_BAND_NOTIFICATION_ACTION_REJECTED;
  return SMART_BAND_NOTIFICATION_ACTION_APPLIED;
}

bool smart_band_notification_remove(smart_band_notification_model_t *model,
                                    uint32_t id)
{
  size_t index;

  if (!valid_model(model) || id == 0u)
    {
      return false;
    }

  for (index = 0u; index < model->count; index++)
    {
      if (model->items[index].id == id)
        {
          remove_at(model, index);
          return true;
        }
    }

  return false;
}

bool smart_band_notification_decide_presentation(
  const smart_band_notification_t *notification,
  const smart_band_notification_policy_t *policy,
  smart_band_notification_presentation_t *presentation)
{
  if (presentation == NULL)
    {
      return false;
    }

  memset(presentation, 0, sizeof(*presentation));
  presentation->center_only = true;
  if (notification == NULL || policy == NULL ||
      !valid_type(notification->type) ||
      !valid_priority(notification->priority) ||
      !valid_action_state(notification->action_state))
    {
      return false;
    }

  if (notification->dismissed ||
      notification->action_state != SMART_BAND_NOTIFICATION_ACTION_NONE ||
      policy->dnd_enabled)
    {
      return true;
    }

  if (policy->workout_active)
    {
      if (notification->type == SMART_BAND_NOTIFICATION_TYPE_CALL ||
          notification->priority >= SMART_BAND_NOTIFICATION_PRIORITY_HIGH)
        {
          presentation->center_only = false;
          presentation->overlay = true;
          presentation->haptic =
            notification->type == SMART_BAND_NOTIFICATION_TYPE_CALL ?
              SMART_BAND_NOTIFICATION_HAPTIC_URGENT :
              SMART_BAND_NOTIFICATION_HAPTIC_NORMAL;
          presentation->wake_request = true;
        }

      return true;
    }

  presentation->center_only = false;
  if (notification->type == SMART_BAND_NOTIFICATION_TYPE_CALL)
    {
      presentation->full_screen = true;
      presentation->haptic = SMART_BAND_NOTIFICATION_HAPTIC_URGENT;
      presentation->wake_request = true;
    }
  else if (notification->priority >= SMART_BAND_NOTIFICATION_PRIORITY_HIGH)
    {
      presentation->overlay = true;
      presentation->haptic =
        notification->priority == SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL ?
          SMART_BAND_NOTIFICATION_HAPTIC_URGENT :
          SMART_BAND_NOTIFICATION_HAPTIC_NORMAL;
      presentation->wake_request = true;
    }
  else if (notification->priority == SMART_BAND_NOTIFICATION_PRIORITY_NORMAL)
    {
      presentation->overlay = true;
      presentation->haptic = SMART_BAND_NOTIFICATION_HAPTIC_SUBTLE;
    }
  else
    {
      presentation->center_only = true;
    }

  return true;
}
