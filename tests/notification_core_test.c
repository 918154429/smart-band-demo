#include "notification_demo.h"
#include "smart_band_notification_model.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
  do                                                                         \
    {                                                                        \
      if (!(condition))                                                      \
        {                                                                    \
          fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
                  #condition);                                               \
          return 1;                                                          \
        }                                                                    \
    }                                                                        \
  while (0)

static smart_band_notification_input_t make_input(
  uint32_t id, smart_band_notification_type_t type,
  smart_band_notification_priority_t priority, uint64_t timestamp)
{
  smart_band_notification_input_t input;

  input.id = id;
  input.type = type;
  input.priority = priority;
  input.source = "test";
  input.title = "title";
  input.body = "body";
  input.wall_timestamp = timestamp;
  return input;
}

static int put_range(smart_band_notification_model_t *model, uint32_t first,
                     smart_band_notification_type_t type,
                     smart_band_notification_priority_t priority,
                     uint64_t timestamp)
{
  uint32_t offset;

  for (offset = 0u; offset < SMART_BAND_NOTIFICATION_CAPACITY; offset++)
    {
      smart_band_notification_input_t input =
        make_input(first + offset, type, priority, timestamp + offset);
      CHECK(smart_band_notification_put(model, &input) ==
            SMART_BAND_NOTIFICATION_PUT_ADDED);
    }

  return 0;
}

static int test_validation_and_strings(void)
{
  smart_band_notification_model_t model;
  smart_band_notification_model_t snapshot;
  smart_band_notification_input_t input =
    make_input(1u, SMART_BAND_NOTIFICATION_TYPE_APP,
               SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, 10u);
  const smart_band_notification_t *stored;
  char long_text[256];

  memset(&model, 0xa5, sizeof(model));
  smart_band_notification_model_init(&model);
  smart_band_notification_model_init(NULL);
  CHECK(smart_band_notification_count(&model) == 0u);
  CHECK(smart_band_notification_count(NULL) == 0u);
  CHECK(smart_band_notification_at(&model, 0u) == NULL);
  CHECK(smart_band_notification_find(&model, 1u) == NULL);
  CHECK(smart_band_notification_find(NULL, 1u) == NULL);
  CHECK(smart_band_notification_find(&model, 0u) == NULL);

  snapshot = model;
  CHECK(smart_band_notification_put(NULL, &input) ==
        SMART_BAND_NOTIFICATION_PUT_INVALID);
  CHECK(smart_band_notification_put(&model, NULL) ==
        SMART_BAND_NOTIFICATION_PUT_INVALID);
  input.id = 0u;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_INVALID);
  input.id = 1u;
  input.type = (smart_band_notification_type_t)-1;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_INVALID);
  input.type = (smart_band_notification_type_t)99;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_INVALID);
  input.type = SMART_BAND_NOTIFICATION_TYPE_APP;
  input.priority = (smart_band_notification_priority_t)-1;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_INVALID);
  input.priority = (smart_band_notification_priority_t)99;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_INVALID);
  input.priority = SMART_BAND_NOTIFICATION_PRIORITY_NORMAL;
  input.source = NULL;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_INVALID);
  input.source = "";
  input.title = NULL;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_INVALID);
  input.title = "";
  input.body = NULL;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_INVALID);
  CHECK(memcmp(&model, &snapshot, sizeof(model)) == 0);

  memset(long_text, 'x', sizeof(long_text));
  long_text[sizeof(long_text) - 1u] = '\0';
  input.id = UINT32_MAX;
  input.source = long_text;
  input.title = long_text;
  input.body = long_text;
  input.wall_timestamp = UINT64_MAX;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  stored = smart_band_notification_find(&model, UINT32_MAX);
  CHECK(stored != NULL);
  CHECK(strlen(stored->source) ==
        SMART_BAND_NOTIFICATION_SOURCE_CAPACITY - 1u);
  CHECK(strlen(stored->title) == SMART_BAND_NOTIFICATION_TITLE_CAPACITY - 1u);
  CHECK(strlen(stored->body) == SMART_BAND_NOTIFICATION_BODY_CAPACITY - 1u);
  CHECK(stored->source[SMART_BAND_NOTIFICATION_SOURCE_CAPACITY - 1u] == '\0');
  CHECK(stored->title[SMART_BAND_NOTIFICATION_TITLE_CAPACITY - 1u] == '\0');
  CHECK(stored->body[SMART_BAND_NOTIFICATION_BODY_CAPACITY - 1u] == '\0');
  CHECK(stored->wall_timestamp == UINT64_MAX);

  input.id = 1u;
  input.source = "";
  input.title = "";
  input.body = "";
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  stored = smart_band_notification_find(&model, 1u);
  CHECK(stored != NULL && stored->source[0] == '\0' &&
        stored->title[0] == '\0' && stored->body[0] == '\0');
  CHECK(smart_band_notification_at(&model, 2u) == NULL);

  model.count = SMART_BAND_NOTIFICATION_CAPACITY + 1u;
  snapshot = model;
  CHECK(smart_band_notification_count(&model) == 0u);
  CHECK(smart_band_notification_at(&model, 0u) == NULL);
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_INVALID);
  CHECK(!smart_band_notification_remove(&model, 1u));
  CHECK(memcmp(&model, &snapshot, sizeof(model)) == 0);
  return 0;
}

static int test_duplicates_and_actions(void)
{
  smart_band_notification_model_t model;
  smart_band_notification_model_t snapshot;
  smart_band_notification_input_t input =
    make_input(10u, SMART_BAND_NOTIFICATION_TYPE_APP,
               SMART_BAND_NOTIFICATION_PRIORITY_LOW, 1u);
  const smart_band_notification_t *stored;

  smart_band_notification_model_init(&model);
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  CHECK(smart_band_notification_apply(&model, 10u,
                                      SMART_BAND_NOTIFICATION_COMMAND_READ) ==
        SMART_BAND_NOTIFICATION_ACTION_APPLIED);
  CHECK(smart_band_notification_apply(&model, 10u,
                                      SMART_BAND_NOTIFICATION_COMMAND_READ) ==
        SMART_BAND_NOTIFICATION_ACTION_NO_CHANGE);

  input.priority = SMART_BAND_NOTIFICATION_PRIORITY_HIGH;
  input.source = "updated-source";
  input.title = "updated-title";
  input.body = "updated-body";
  input.wall_timestamp = 99u;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_UPDATED);
  CHECK(smart_band_notification_count(&model) == 1u);
  stored = smart_band_notification_find(&model, 10u);
  CHECK(stored != NULL && stored->read && !stored->dismissed);
  CHECK(stored->priority == SMART_BAND_NOTIFICATION_PRIORITY_HIGH);
  CHECK(strcmp(stored->source, "updated-source") == 0);
  CHECK(strcmp(stored->title, "updated-title") == 0);
  CHECK(strcmp(stored->body, "updated-body") == 0);
  CHECK(stored->wall_timestamp == 99u);

  snapshot = model;
  input.type = SMART_BAND_NOTIFICATION_TYPE_SMS;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_INVALID);
  CHECK(memcmp(&model, &snapshot, sizeof(model)) == 0);
  CHECK(smart_band_notification_apply(&model, 10u,
                                      SMART_BAND_NOTIFICATION_COMMAND_ACCEPT) ==
        SMART_BAND_NOTIFICATION_ACTION_INVALID);
  CHECK(smart_band_notification_apply(&model, 10u,
                                      SMART_BAND_NOTIFICATION_COMMAND_REJECT) ==
        SMART_BAND_NOTIFICATION_ACTION_INVALID);
  CHECK(smart_band_notification_apply(&model, 10u,
                                      SMART_BAND_NOTIFICATION_COMMAND_DISMISS) ==
        SMART_BAND_NOTIFICATION_ACTION_APPLIED);
  CHECK(smart_band_notification_apply(&model, 10u,
                                      SMART_BAND_NOTIFICATION_COMMAND_DISMISS) ==
        SMART_BAND_NOTIFICATION_ACTION_NO_CHANGE);

  input = make_input(20u, SMART_BAND_NOTIFICATION_TYPE_CALL,
                     SMART_BAND_NOTIFICATION_PRIORITY_HIGH, 2u);
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  CHECK(smart_band_notification_apply(&model, 20u,
                                      SMART_BAND_NOTIFICATION_COMMAND_ACCEPT) ==
        SMART_BAND_NOTIFICATION_ACTION_APPLIED);
  CHECK(smart_band_notification_apply(&model, 20u,
                                      SMART_BAND_NOTIFICATION_COMMAND_ACCEPT) ==
        SMART_BAND_NOTIFICATION_ACTION_NO_CHANGE);
  snapshot = model;
  CHECK(smart_band_notification_apply(&model, 20u,
                                      SMART_BAND_NOTIFICATION_COMMAND_REJECT) ==
        SMART_BAND_NOTIFICATION_ACTION_INVALID);
  CHECK(smart_band_notification_apply(&model, 20u,
                                      SMART_BAND_NOTIFICATION_COMMAND_DISMISS) ==
        SMART_BAND_NOTIFICATION_ACTION_INVALID);
  CHECK(memcmp(&model, &snapshot, sizeof(model)) == 0);

  input.id = 21u;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  CHECK(smart_band_notification_apply(&model, 21u,
                                      SMART_BAND_NOTIFICATION_COMMAND_REJECT) ==
        SMART_BAND_NOTIFICATION_ACTION_APPLIED);
  CHECK(smart_band_notification_apply(&model, 21u,
                                      SMART_BAND_NOTIFICATION_COMMAND_REJECT) ==
        SMART_BAND_NOTIFICATION_ACTION_NO_CHANGE);
  CHECK(smart_band_notification_apply(&model, 21u,
                                      SMART_BAND_NOTIFICATION_COMMAND_ACCEPT) ==
        SMART_BAND_NOTIFICATION_ACTION_INVALID);

  input.id = 22u;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  CHECK(smart_band_notification_apply(&model, 22u,
                                      SMART_BAND_NOTIFICATION_COMMAND_DISMISS) ==
        SMART_BAND_NOTIFICATION_ACTION_APPLIED);
  CHECK(smart_band_notification_apply(&model, 22u,
                                      SMART_BAND_NOTIFICATION_COMMAND_ACCEPT) ==
        SMART_BAND_NOTIFICATION_ACTION_INVALID);
  CHECK(smart_band_notification_apply(&model, 999u,
                                      SMART_BAND_NOTIFICATION_COMMAND_READ) ==
        SMART_BAND_NOTIFICATION_ACTION_NOT_FOUND);
  CHECK(smart_band_notification_apply(NULL, 1u,
                                      SMART_BAND_NOTIFICATION_COMMAND_READ) ==
        SMART_BAND_NOTIFICATION_ACTION_INVALID);
  CHECK(smart_band_notification_apply(&model, 0u,
                                      SMART_BAND_NOTIFICATION_COMMAND_READ) ==
        SMART_BAND_NOTIFICATION_ACTION_INVALID);
  snapshot = model;
  CHECK(smart_band_notification_apply(&model, 10u,
             (smart_band_notification_command_t)99) ==
        SMART_BAND_NOTIFICATION_ACTION_INVALID);
  CHECK(memcmp(&model, &snapshot, sizeof(model)) == 0);
  return 0;
}

static int test_capacity_and_eviction(void)
{
  smart_band_notification_model_t model;
  smart_band_notification_model_t snapshot;
  smart_band_notification_input_t input;
  uint32_t id;

  smart_band_notification_model_init(&model);
  CHECK(put_range(&model, 1u, SMART_BAND_NOTIFICATION_TYPE_APP,
                  SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, 100u) == 0);
  CHECK(smart_band_notification_count(&model) ==
        SMART_BAND_NOTIFICATION_CAPACITY);
  input = make_input(100u, SMART_BAND_NOTIFICATION_TYPE_APP,
                     SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, 1000u);
  snapshot = model;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_FULL);
  CHECK(memcmp(&model, &snapshot, sizeof(model)) == 0);

  input.priority = SMART_BAND_NOTIFICATION_PRIORITY_HIGH;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  CHECK(smart_band_notification_find(&model, 1u) == NULL);
  CHECK(smart_band_notification_find(&model, 100u) != NULL);

  smart_band_notification_model_init(&model);
  CHECK(put_range(&model, 1u, SMART_BAND_NOTIFICATION_TYPE_APP,
                  SMART_BAND_NOTIFICATION_PRIORITY_HIGH, 1u) == 0);
  input = make_input(100u, SMART_BAND_NOTIFICATION_TYPE_APP,
                     SMART_BAND_NOTIFICATION_PRIORITY_HIGH, 1000u);
  snapshot = model;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_FULL);
  CHECK(memcmp(&model, &snapshot, sizeof(model)) == 0);

  smart_band_notification_model_init(&model);
  CHECK(put_range(&model, 200u, SMART_BAND_NOTIFICATION_TYPE_CALL,
                  SMART_BAND_NOTIFICATION_PRIORITY_HIGH, 1u) == 0);
  CHECK(smart_band_notification_apply(&model, 200u,
                                      SMART_BAND_NOTIFICATION_COMMAND_READ) ==
        SMART_BAND_NOTIFICATION_ACTION_APPLIED);
  input = make_input(500u, SMART_BAND_NOTIFICATION_TYPE_APP,
                     SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL, 1u);
  snapshot = model;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_PROTECTED);
  CHECK(memcmp(&model, &snapshot, sizeof(model)) == 0);

  smart_band_notification_model_init(&model);
  for (id = 1u; id <= SMART_BAND_NOTIFICATION_CAPACITY; id++)
    {
      input = make_input(id, SMART_BAND_NOTIFICATION_TYPE_APP,
                         id == 1u ? SMART_BAND_NOTIFICATION_PRIORITY_LOW :
                                    SMART_BAND_NOTIFICATION_PRIORITY_NORMAL,
                         id == 1u ? 500u : 100u + id);
      CHECK(smart_band_notification_put(&model, &input) ==
            SMART_BAND_NOTIFICATION_PUT_ADDED);
    }
  CHECK(smart_band_notification_apply(&model, 2u,
                                      SMART_BAND_NOTIFICATION_COMMAND_READ) ==
        SMART_BAND_NOTIFICATION_ACTION_APPLIED);
  input = make_input(100u, SMART_BAND_NOTIFICATION_TYPE_SYSTEM,
                     SMART_BAND_NOTIFICATION_PRIORITY_HIGH, 1000u);
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  CHECK(smart_band_notification_find(&model, 1u) == NULL);
  CHECK(smart_band_notification_find(&model, 2u) != NULL);

  smart_band_notification_model_init(&model);
  CHECK(put_range(&model, 1u, SMART_BAND_NOTIFICATION_TYPE_APP,
                  SMART_BAND_NOTIFICATION_PRIORITY_LOW, 50u) == 0);
  CHECK(smart_band_notification_apply(&model, 1u,
                                      SMART_BAND_NOTIFICATION_COMMAND_READ) ==
        SMART_BAND_NOTIFICATION_ACTION_APPLIED);
  CHECK(smart_band_notification_apply(&model, 2u,
                                      SMART_BAND_NOTIFICATION_COMMAND_READ) ==
        SMART_BAND_NOTIFICATION_ACTION_APPLIED);
  model.items[0].wall_timestamp = 10u;
  model.items[1].wall_timestamp = 10u;
  input = make_input(100u, SMART_BAND_NOTIFICATION_TYPE_SMS,
                     SMART_BAND_NOTIFICATION_PRIORITY_LOW, 100u);
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  CHECK(smart_band_notification_find(&model, 1u) == NULL);
  CHECK(smart_band_notification_find(&model, 2u) != NULL);

  smart_band_notification_model_init(&model);
  input = make_input(1000u, SMART_BAND_NOTIFICATION_TYPE_CALL,
                     SMART_BAND_NOTIFICATION_PRIORITY_HIGH, 1u);
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  for (id = 1u; id < SMART_BAND_NOTIFICATION_CAPACITY; id++)
    {
      input = make_input(id, SMART_BAND_NOTIFICATION_TYPE_APP,
                         SMART_BAND_NOTIFICATION_PRIORITY_LOW, id);
      CHECK(smart_band_notification_put(&model, &input) ==
            SMART_BAND_NOTIFICATION_PUT_ADDED);
    }
  input = make_input(2000u, SMART_BAND_NOTIFICATION_TYPE_APP,
                     SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, 500u);
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  CHECK(smart_band_notification_find(&model, 1000u) != NULL);
  CHECK(smart_band_notification_find(&model, 1u) == NULL);
  return 0;
}

static int test_order_and_remove(void)
{
  smart_band_notification_model_t model;
  smart_band_notification_input_t input;
  const uint32_t expected[] = {2u, 3u, 5u};
  size_t index;

  smart_band_notification_model_init(&model);
  for (uint32_t id = 1u; id <= 5u; id++)
    {
      input = make_input(id, SMART_BAND_NOTIFICATION_TYPE_SMS,
                         SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, 100u);
      CHECK(smart_band_notification_put(&model, &input) ==
            SMART_BAND_NOTIFICATION_PUT_ADDED);
    }

  CHECK(smart_band_notification_remove(&model, 1u));
  CHECK(smart_band_notification_remove(&model, 4u));
  CHECK(smart_band_notification_remove(&model, 5u));
  CHECK(!smart_band_notification_remove(&model, 0u));
  CHECK(!smart_band_notification_remove(&model, 999u));
  input.id = 5u;
  CHECK(smart_band_notification_put(&model, &input) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  CHECK(smart_band_notification_count(&model) == 3u);
  for (index = 0u; index < 3u; index++)
    {
      const smart_band_notification_t *item =
        smart_band_notification_at(&model, index);
      CHECK(item != NULL && item->id == expected[index]);
    }
  return 0;
}

static int test_presentation_policy(void)
{
  smart_band_notification_t notification;
  smart_band_notification_policy_t policy = {false, false};
  smart_band_notification_presentation_t presentation;

  memset(&notification, 0, sizeof(notification));
  notification.type = SMART_BAND_NOTIFICATION_TYPE_APP;
  notification.priority = SMART_BAND_NOTIFICATION_PRIORITY_LOW;
  CHECK(smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  CHECK(presentation.center_only && !presentation.overlay &&
        !presentation.full_screen &&
        presentation.haptic == SMART_BAND_NOTIFICATION_HAPTIC_NONE &&
        !presentation.wake_request);

  notification.priority = SMART_BAND_NOTIFICATION_PRIORITY_NORMAL;
  CHECK(smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  CHECK(!presentation.center_only && presentation.overlay &&
        presentation.haptic == SMART_BAND_NOTIFICATION_HAPTIC_SUBTLE &&
        !presentation.wake_request);
  notification.priority = SMART_BAND_NOTIFICATION_PRIORITY_HIGH;
  CHECK(smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  CHECK(presentation.overlay &&
        presentation.haptic == SMART_BAND_NOTIFICATION_HAPTIC_NORMAL &&
        presentation.wake_request);
  notification.type = SMART_BAND_NOTIFICATION_TYPE_SYSTEM;
  notification.priority = SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL;
  CHECK(smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  CHECK(presentation.overlay &&
        presentation.haptic == SMART_BAND_NOTIFICATION_HAPTIC_URGENT);
  notification.type = SMART_BAND_NOTIFICATION_TYPE_CALL;
  notification.priority = SMART_BAND_NOTIFICATION_PRIORITY_HIGH;
  CHECK(smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  CHECK(presentation.full_screen && !presentation.overlay &&
        presentation.haptic == SMART_BAND_NOTIFICATION_HAPTIC_URGENT &&
        presentation.wake_request);

  policy.workout_active = true;
  CHECK(smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  CHECK(presentation.overlay && !presentation.full_screen &&
        presentation.haptic == SMART_BAND_NOTIFICATION_HAPTIC_URGENT);
  notification.type = SMART_BAND_NOTIFICATION_TYPE_APP;
  notification.priority = SMART_BAND_NOTIFICATION_PRIORITY_NORMAL;
  CHECK(smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  CHECK(presentation.center_only);
  notification.priority = SMART_BAND_NOTIFICATION_PRIORITY_HIGH;
  CHECK(smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  CHECK(presentation.overlay && !presentation.full_screen &&
        presentation.haptic == SMART_BAND_NOTIFICATION_HAPTIC_NORMAL);

  policy.dnd_enabled = true;
  notification.type = SMART_BAND_NOTIFICATION_TYPE_CALL;
  CHECK(smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  CHECK(presentation.center_only &&
        presentation.haptic == SMART_BAND_NOTIFICATION_HAPTIC_NONE &&
        !presentation.wake_request);
  policy.dnd_enabled = false;
  notification.dismissed = true;
  CHECK(smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  CHECK(presentation.center_only);
  notification.dismissed = false;
  notification.action_state = SMART_BAND_NOTIFICATION_ACTION_ACCEPTED;
  CHECK(smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  CHECK(presentation.center_only);

  CHECK(!smart_band_notification_decide_presentation(
          NULL, &policy, &presentation));
  CHECK(presentation.center_only);
  CHECK(!smart_band_notification_decide_presentation(
          &notification, NULL, &presentation));
  CHECK(!smart_band_notification_decide_presentation(
          &notification, &policy, NULL));
  notification.action_state = SMART_BAND_NOTIFICATION_ACTION_NONE;
  notification.type = (smart_band_notification_type_t)99;
  CHECK(!smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  notification.type = SMART_BAND_NOTIFICATION_TYPE_APP;
  notification.priority = (smart_band_notification_priority_t)99;
  CHECK(!smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  notification.priority = SMART_BAND_NOTIFICATION_PRIORITY_NORMAL;
  notification.action_state = (smart_band_notification_action_state_t)99;
  CHECK(!smart_band_notification_decide_presentation(
          &notification, &policy, &presentation));
  return 0;
}

static int test_deterministic_stress(void)
{
  smart_band_notification_model_t first;
  smart_band_notification_model_t second;
  const smart_band_notification_t *item;
  size_t left;
  size_t right;
  uint32_t sequence;

  smart_band_notification_model_init(&first);
  smart_band_notification_model_init(&second);
  for (sequence = 0u; sequence < 1000u; sequence++)
    {
      smart_band_notification_put_result_t first_result;
      smart_band_notification_put_result_t second_result;

      if (sequence % 5u == 0u && smart_band_notification_count(&first) > 0u)
        {
          uint32_t id = smart_band_notification_at(&first, 0u)->id;
          CHECK(smart_band_notification_remove(&first, id));
          CHECK(smart_band_notification_remove(&second, id));
        }

      first_result = smart_band_notification_demo_inject(
        &first, UINT32_C(0x12345678), sequence);
      second_result = smart_band_notification_demo_inject(
        &second, UINT32_C(0x12345678), sequence);
      CHECK(first_result == second_result);
      CHECK(memcmp(&first, &second, sizeof(first)) == 0);
      CHECK(smart_band_notification_count(&first) <=
            SMART_BAND_NOTIFICATION_CAPACITY);
      for (left = 0u; left < smart_band_notification_count(&first); left++)
        {
          item = smart_band_notification_at(&first, left);
          CHECK(item != NULL && item->id != 0u);
          CHECK(item->source[SMART_BAND_NOTIFICATION_SOURCE_CAPACITY - 1u] ==
                '\0');
          CHECK(item->title[SMART_BAND_NOTIFICATION_TITLE_CAPACITY - 1u] ==
                '\0');
          CHECK(item->body[SMART_BAND_NOTIFICATION_BODY_CAPACITY - 1u] ==
                '\0');
          for (right = left + 1u;
               right < smart_band_notification_count(&first); right++)
            {
              CHECK(item->id != smart_band_notification_at(&first, right)->id);
            }
        }
    }

  smart_band_notification_model_init(&first);
  CHECK(smart_band_notification_demo_inject(&first, 0u, 0u) ==
        SMART_BAND_NOTIFICATION_PUT_ADDED);
  CHECK(smart_band_notification_find(&first, UINT32_MAX) != NULL);
  CHECK(smart_band_notification_demo_inject(&first, 0u, 0u) ==
        SMART_BAND_NOTIFICATION_PUT_UPDATED);
  CHECK(smart_band_notification_count(&first) == 1u);
  return 0;
}

int main(void)
{
  CHECK(test_validation_and_strings() == 0);
  CHECK(test_duplicates_and_actions() == 0);
  CHECK(test_capacity_and_eviction() == 0);
  CHECK(test_order_and_remove() == 0);
  CHECK(test_presentation_policy() == 0);
  CHECK(test_deterministic_stress() == 0);
  puts("smart band notification core tests passed");
  return 0;
}
