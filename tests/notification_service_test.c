#include "notification_demo.h"
#include "smart_band_event.h"
#include "smart_band_notification_service.h"

#include <stdbool.h>
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

typedef struct
{
  unsigned int lock_calls;
  unsigned int unlock_calls;
  bool allow;
  bool held;
} fake_lock_t;

static bool fake_lock_enter(void *context)
{
  fake_lock_t *lock = (fake_lock_t *)context;

  lock->lock_calls++;
  if (!lock->allow || lock->held)
    {
      return false;
    }

  lock->held = true;
  return true;
}

static void fake_lock_leave(void *context)
{
  fake_lock_t *lock = (fake_lock_t *)context;

  lock->unlock_calls++;
  lock->held = false;
}

static smart_band_notification_input_t make_input(
  uint32_t id, smart_band_notification_type_t type,
  smart_band_notification_priority_t priority, uint64_t timestamp)
{
  smart_band_notification_input_t input;

  input.id = id;
  input.type = type;
  input.priority = priority;
  input.source = "test-source";
  input.title = "test-title";
  input.body = "test-body";
  input.wall_timestamp = timestamp;
  return input;
}

static smart_band_event_t make_generic_event(smart_band_event_type_t type,
                                             uint32_t code)
{
  smart_band_event_t event;

  memset(&event, 0, sizeof(event));
  event.type = type;
  event.payload.generic.code = code;
  return event;
}

static int receive(smart_band_notification_service_t *service,
                   const smart_band_notification_input_t *input,
                   uint32_t monotonic_ms,
                   smart_band_notification_service_result_t expected)
{
  smart_band_event_t event;

  CHECK(smart_band_notification_event_received(input, monotonic_ms, &event));
  CHECK(event.type == SMART_BAND_EVENT_NOTIFICATION_RECEIVED);
  CHECK(event.monotonic_ms == monotonic_ms);
  CHECK(smart_band_notification_service_process(service, &event) == expected);
  return 0;
}

static int action(smart_band_notification_service_t *service, uint32_t id,
                  smart_band_notification_command_t command,
                  smart_band_notification_service_result_t expected)
{
  smart_band_event_t event;

  CHECK(smart_band_notification_event_action(id, command, id, &event));
  CHECK(event.type == SMART_BAND_EVENT_NOTIFICATION_ACTION);
  CHECK(event.monotonic_ms == id);
  CHECK(smart_band_notification_service_process(service, &event) == expected);
  return 0;
}

static int model_invariants(const smart_band_notification_model_t *model)
{
  size_t left;
  size_t right;

  CHECK(smart_band_notification_count(model) <=
        SMART_BAND_NOTIFICATION_CAPACITY);
  for (left = 0u; left < smart_band_notification_count(model); left++)
    {
      const smart_band_notification_t *item =
        smart_band_notification_at(model, left);

      CHECK(item != NULL && item->id != 0u);
      CHECK(item->source[SMART_BAND_NOTIFICATION_SOURCE_CAPACITY - 1u] ==
            '\0');
      CHECK(item->title[SMART_BAND_NOTIFICATION_TITLE_CAPACITY - 1u] ==
            '\0');
      CHECK(item->body[SMART_BAND_NOTIFICATION_BODY_CAPACITY - 1u] == '\0');
      for (right = left + 1u;
           right < smart_band_notification_count(model); right++)
        {
          CHECK(item->id != smart_band_notification_at(model, right)->id);
        }
    }

  return 0;
}

static int test_validation_sizes_and_recovery(void)
{
  smart_band_notification_service_t service;
  smart_band_event_queue_t queue;
  smart_band_notification_model_t model_before;
  smart_band_notification_input_t input =
    make_input(1u, SMART_BAND_NOTIFICATION_TYPE_APP,
               SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, 1u);
  smart_band_event_t event;
  smart_band_notification_presentation_t presentation;
  uint32_t id;
  volatile size_t stats_size =
    sizeof(smart_band_notification_service_stats_t);
  volatile size_t event_size = sizeof(smart_band_event_t);
  volatile size_t queue_size = sizeof(smart_band_event_queue_t);
  volatile size_t inbox_size = sizeof(smart_band_event_inbox_t);
  volatile size_t service_size = sizeof(smart_band_notification_service_t);

  CHECK(stats_size == 8u * sizeof(uint32_t));
  CHECK(event_size <= 256u);
  CHECK(queue_size <=
        event_size * SMART_BAND_EVENT_QUEUE_CAPACITY + 128u);
  CHECK(inbox_size <=
        event_size * SMART_BAND_EVENT_QUEUE_CAPACITY + 128u);
  CHECK(service_size <=
        sizeof(smart_band_notification_t) * SMART_BAND_NOTIFICATION_CAPACITY +
          512u);
  printf("notification sizes: event=%zu queue=%zu inbox=%zu service=%zu "
         "stats=%zu\n",
         sizeof(smart_band_event_t), sizeof(smart_band_event_queue_t),
         sizeof(smart_band_event_inbox_t),
         sizeof(smart_band_notification_service_t),
         sizeof(smart_band_notification_service_stats_t));

  CHECK(smart_band_notification_service_init(NULL) != 0);
  CHECK(smart_band_notification_service_init(&service) == 0);
  CHECK(!smart_band_notification_service_peek_presentation(
          &service, &id, &presentation));
  CHECK(!smart_band_notification_service_ack_presentation(&service, 1u));
  CHECK(!smart_band_notification_service_process(NULL, &event));
  CHECK(smart_band_notification_service_process(&service, NULL) ==
        SMART_BAND_NOTIFICATION_SERVICE_INVALID);
  CHECK(!smart_band_notification_event_received(NULL, 0u, &event));
  CHECK(!smart_band_notification_event_received(&input, 0u, NULL));
  CHECK(!smart_band_notification_event_action(
          0u, SMART_BAND_NOTIFICATION_COMMAND_READ, 0u, &event));
  CHECK(!smart_band_notification_event_action(
          1u, (smart_band_notification_command_t)99, 0u, &event));

  smart_band_event_queue_init(&queue);
  for (size_t index = 0u; index < SMART_BAND_EVENT_QUEUE_CAPACITY; index++)
    {
      event = make_generic_event(SMART_BAND_EVENT_TOUCH_ACTIVITY,
                                 (uint32_t)index);
      CHECK(smart_band_event_queue_push(&queue, &event));
    }
  memset(&event, 0, sizeof(event));
  event.type = SMART_BAND_EVENT_NOTIFICATION_RECEIVED;
  event.payload.notification_received.type = SMART_BAND_NOTIFICATION_TYPE_CALL;
  event.payload.notification_received.priority =
    (smart_band_notification_priority_t)-1;
  CHECK(smart_band_event_priority(&event) == SMART_BAND_EVENT_PRIORITY_LOW);
  CHECK(!smart_band_event_queue_push(&queue, &event));
  event.payload.notification_received.type =
    (smart_band_notification_type_t)99;
  event.payload.notification_received.priority =
    SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL;
  CHECK(smart_band_event_priority(&event) == SMART_BAND_EVENT_PRIORITY_LOW);
  CHECK(!smart_band_event_queue_push(&queue, &event));
  CHECK(queue.evicted == 0u && queue.dropped == 2u);

  CHECK(smart_band_notification_event_received(&input, 1u, &event));
  event.payload.notification_received
    .body[SMART_BAND_NOTIFICATION_BODY_CAPACITY - 1u] = 'x';
  model_before = service.model;
  CHECK(smart_band_notification_service_process(&service, &event) ==
        SMART_BAND_NOTIFICATION_SERVICE_INVALID);
  CHECK(memcmp(&model_before, &service.model, sizeof(model_before)) == 0);
  CHECK(service.stats.invalid == 1u);

  event = make_generic_event(SMART_BAND_EVENT_TOUCH_ACTIVITY, 1u);
  CHECK(smart_band_notification_service_process(&service, &event) ==
        SMART_BAND_NOTIFICATION_SERVICE_INVALID);
  CHECK(memcmp(&model_before, &service.model, sizeof(model_before)) == 0);
  CHECK(service.stats.invalid == 2u);

  CHECK(receive(&service, &input, 2u,
                SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  CHECK(smart_band_notification_count(&service.model) == 1u);
  smart_band_notification_service_reset(&service);
  CHECK(!service.initialized);
  CHECK(smart_band_notification_service_process(&service, &event) ==
        SMART_BAND_NOTIFICATION_SERVICE_INVALID);
  smart_band_notification_service_reset(NULL);
  return 0;
}

static int test_overlong_copy_and_exact_duplicate(void)
{
  smart_band_notification_service_t service;
  smart_band_notification_input_t input =
    make_input(10u, SMART_BAND_NOTIFICATION_TYPE_APP,
               SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, UINT64_MAX);
  smart_band_notification_presentation_t presentation;
  const smart_band_notification_t *stored;
  smart_band_event_t event;
  char long_text[4096];
  uint32_t id;

  memset(long_text, 'x', sizeof(long_text));
  long_text[sizeof(long_text) - 1u] = '\0';
  input.source = long_text;
  input.title = long_text;
  input.body = long_text;
  CHECK(smart_band_notification_service_init(&service) == 0);
  CHECK(smart_band_notification_event_received(&input, UINT32_MAX, &event));
  CHECK(strlen(event.payload.notification_received.source) ==
        SMART_BAND_NOTIFICATION_SOURCE_CAPACITY - 1u);
  CHECK(strlen(event.payload.notification_received.title) ==
        SMART_BAND_NOTIFICATION_TITLE_CAPACITY - 1u);
  CHECK(strlen(event.payload.notification_received.body) ==
        SMART_BAND_NOTIFICATION_BODY_CAPACITY - 1u);
  CHECK(event.payload.notification_received.wall_timestamp == UINT64_MAX);
  CHECK(smart_band_notification_service_process(&service, &event) ==
        SMART_BAND_NOTIFICATION_SERVICE_APPLIED);
  stored = smart_band_notification_find(&service.model, 10u);
  CHECK(stored != NULL);
  CHECK(strcmp(stored->source, event.payload.notification_received.source) ==
        0);
  CHECK(strcmp(stored->title, event.payload.notification_received.title) == 0);
  CHECK(strcmp(stored->body, event.payload.notification_received.body) == 0);
  CHECK(smart_band_notification_service_peek_presentation(
          &service, &id, &presentation));
  CHECK(id == 10u && presentation.overlay);
  CHECK(smart_band_notification_service_ack_presentation(&service, id));

  CHECK(smart_band_notification_service_process(&service, &event) ==
        SMART_BAND_NOTIFICATION_SERVICE_NO_CHANGE);
  CHECK(service.stats.received == 2u);
  CHECK(service.stats.added == 1u);
  CHECK(service.stats.duplicates == 1u);
  CHECK(service.stats.presentations == 1u);
  CHECK(!smart_band_notification_service_peek_presentation(
          &service, &id, &presentation));
  return 0;
}

static int test_call_pending_peek_ack_and_policy(void)
{
  smart_band_notification_service_t service;
  smart_band_notification_input_t call =
    make_input(20u, SMART_BAND_NOTIFICATION_TYPE_CALL,
               SMART_BAND_NOTIFICATION_PRIORITY_HIGH, 20u);
  smart_band_notification_input_t app =
    make_input(21u, SMART_BAND_NOTIFICATION_TYPE_APP,
               SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL, 21u);
  smart_band_notification_policy_t policy = {false, false};
  smart_band_notification_presentation_t presentation;
  uint32_t id;

  CHECK(smart_band_notification_service_init(&service) == 0);
  CHECK(receive(&service, &call, 20u,
                SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  CHECK(smart_band_notification_service_peek_presentation(
          &service, &id, &presentation));
  CHECK(id == 20u && presentation.full_screen && !presentation.overlay);

  CHECK(receive(&service, &app, 21u,
                SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  CHECK(smart_band_notification_service_peek_presentation(
          &service, &id, &presentation));
  CHECK(id == 20u && presentation.full_screen);
  CHECK(!smart_band_notification_service_ack_presentation(&service, 21u));
  CHECK(smart_band_notification_service_peek_presentation(
          &service, &id, &presentation));
  CHECK(id == 20u);

  policy.workout_active = true;
  smart_band_notification_service_set_policy(&service, &policy);
  CHECK(smart_band_notification_service_peek_presentation(
          &service, &id, &presentation));
  CHECK(id == 20u && !presentation.full_screen && presentation.overlay);

  policy.dnd_enabled = true;
  smart_band_notification_service_set_policy(&service, &policy);
  CHECK(!smart_band_notification_service_peek_presentation(
          &service, &id, &presentation));
  app.id = 22u;
  CHECK(receive(&service, &app, 22u,
                SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  CHECK(smart_band_notification_find(&service.model, 22u) != NULL);
  CHECK(!smart_band_notification_service_peek_presentation(
          &service, &id, &presentation));

  policy.dnd_enabled = false;
  smart_band_notification_service_set_policy(&service, &policy);
  call.id = 23u;
  CHECK(receive(&service, &call, 23u,
                SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  CHECK(smart_band_notification_service_peek_presentation(
          &service, &id, &presentation));
  CHECK(id == 23u && presentation.overlay && !presentation.full_screen);
  policy.workout_active = false;
  smart_band_notification_service_set_policy(&service, &policy);
  CHECK(smart_band_notification_service_peek_presentation(
          &service, &id, &presentation));
  CHECK(id == 23u && presentation.overlay && !presentation.full_screen);
  CHECK(smart_band_notification_service_ack_presentation(&service, 23u));
  CHECK(!smart_band_notification_service_peek_presentation(
          &service, &id, &presentation));
  return 0;
}

static int test_full_protected_and_recovery(void)
{
  smart_band_notification_service_t service;
  smart_band_notification_input_t input;
  uint32_t index;

  CHECK(smart_band_notification_service_init(&service) == 0);
  for (index = 0u; index < SMART_BAND_NOTIFICATION_CAPACITY; index++)
    {
      input = make_input(
        UINT32_C(1000) + index, SMART_BAND_NOTIFICATION_TYPE_CALL,
        SMART_BAND_NOTIFICATION_PRIORITY_HIGH, index);
      CHECK(receive(&service, &input, index,
                    SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
    }

  input = make_input(2000u, SMART_BAND_NOTIFICATION_TYPE_APP,
                     SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL, 2000u);
  CHECK(receive(&service, &input, 2000u,
                SMART_BAND_NOTIFICATION_SERVICE_REJECTED) == 0);
  CHECK(service.last_put_result == SMART_BAND_NOTIFICATION_PUT_PROTECTED);
  CHECK(service.stats.rejected == 1u);
  CHECK(smart_band_notification_find(&service.model, 2000u) == NULL);

  CHECK(action(&service, 1000u, SMART_BAND_NOTIFICATION_COMMAND_ACCEPT,
               SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  CHECK(receive(&service, &input, 2001u,
                SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  CHECK(smart_band_notification_count(&service.model) ==
        SMART_BAND_NOTIFICATION_CAPACITY);
  CHECK(smart_band_notification_find(&service.model, 1000u) == NULL);
  CHECK(smart_band_notification_find(&service.model, 2000u) != NULL);
  CHECK(model_invariants(&service.model) == 0);
  return 0;
}

static int test_accept_reject_and_delete(void)
{
  smart_band_notification_service_t service;
  smart_band_notification_input_t input =
    make_input(30u, SMART_BAND_NOTIFICATION_TYPE_CALL,
               SMART_BAND_NOTIFICATION_PRIORITY_HIGH, 30u);
  const smart_band_notification_t *stored;

  CHECK(smart_band_notification_service_init(&service) == 0);
  CHECK(receive(&service, &input, 30u,
                SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  CHECK(action(&service, 30u, SMART_BAND_NOTIFICATION_COMMAND_ACCEPT,
               SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  stored = smart_band_notification_find(&service.model, 30u);
  CHECK(stored != NULL && stored->read && stored->dismissed &&
        stored->action_state == SMART_BAND_NOTIFICATION_ACTION_ACCEPTED);
  CHECK(action(&service, 30u, SMART_BAND_NOTIFICATION_COMMAND_ACCEPT,
               SMART_BAND_NOTIFICATION_SERVICE_NO_CHANGE) == 0);
  CHECK(action(&service, 30u, SMART_BAND_NOTIFICATION_COMMAND_REJECT,
               SMART_BAND_NOTIFICATION_SERVICE_INVALID) == 0);

  input.id = 31u;
  CHECK(receive(&service, &input, 31u,
                SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  CHECK(action(&service, 31u, SMART_BAND_NOTIFICATION_COMMAND_REJECT,
               SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  stored = smart_band_notification_find(&service.model, 31u);
  CHECK(stored != NULL &&
        stored->action_state == SMART_BAND_NOTIFICATION_ACTION_REJECTED);

  input.id = 32u;
  input.type = SMART_BAND_NOTIFICATION_TYPE_SMS;
  input.priority = SMART_BAND_NOTIFICATION_PRIORITY_NORMAL;
  CHECK(receive(&service, &input, 32u,
                SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  CHECK(action(&service, 32u, SMART_BAND_NOTIFICATION_COMMAND_ACCEPT,
               SMART_BAND_NOTIFICATION_SERVICE_INVALID) == 0);
  CHECK(action(&service, 32u, SMART_BAND_NOTIFICATION_COMMAND_DELETE,
               SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  CHECK(smart_band_notification_find(&service.model, 32u) == NULL);
  CHECK(action(&service, 32u, SMART_BAND_NOTIFICATION_COMMAND_DELETE,
               SMART_BAND_NOTIFICATION_SERVICE_REJECTED) == 0);
  CHECK(action(&service, 30u, SMART_BAND_NOTIFICATION_COMMAND_DELETE,
               SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
  CHECK(smart_band_notification_find(&service.model, 30u) == NULL);
  return 0;
}

static int test_thousand_mixed_receives(void)
{
  smart_band_notification_service_t service;
  smart_band_notification_t generated;
  smart_band_notification_input_t input;
  smart_band_event_t event;
  uint32_t iteration;

  CHECK(smart_band_notification_service_init(&service) == 0);
  for (iteration = 0u; iteration < 1000u; iteration++)
    {
      uint32_t sequence = iteration / 2u;

      CHECK(smart_band_notification_demo_build(
              UINT32_C(0x12345678), sequence, &generated));
      input.id = generated.id;
      input.type = generated.type;
      input.priority = generated.priority;
      input.source = generated.source;
      input.title = generated.title;
      input.body = generated.body;
      input.wall_timestamp = generated.wall_timestamp;

      if ((iteration & 1u) == 0u &&
          smart_band_notification_count(&service.model) ==
            SMART_BAND_NOTIFICATION_CAPACITY)
        {
          const smart_band_notification_t *oldest =
            smart_band_notification_at(&service.model, 0u);

          CHECK(oldest != NULL);
          CHECK(action(&service, oldest->id,
                       SMART_BAND_NOTIFICATION_COMMAND_DELETE,
                       SMART_BAND_NOTIFICATION_SERVICE_APPLIED) == 0);
        }

      CHECK(smart_band_notification_event_received(
              &input, iteration, &event));
      CHECK(smart_band_notification_service_process(&service, &event) ==
            ((iteration & 1u) == 0u ?
              SMART_BAND_NOTIFICATION_SERVICE_APPLIED :
              SMART_BAND_NOTIFICATION_SERVICE_NO_CHANGE));
      CHECK(model_invariants(&service.model) == 0);
    }

  CHECK(service.stats.received == 1000u);
  CHECK(service.stats.added == 500u);
  CHECK(service.stats.duplicates == 500u);
  CHECK(service.stats.updated == 0u);
  CHECK(service.stats.rejected == 0u);
  CHECK(service.stats.invalid == 0u);
  CHECK(service.stats.presentations > 0u &&
        service.stats.presentations <= service.stats.added);
  CHECK(smart_band_notification_count(&service.model) ==
        SMART_BAND_NOTIFICATION_CAPACITY);
  return 0;
}

static int test_inbox_wrap_priority_and_lock_recovery(void)
{
  smart_band_event_inbox_t inbox;
  smart_band_event_lock_t lock_ops;
  fake_lock_t lock = {0u, 0u, true, false};
  smart_band_notification_input_t call =
    make_input(5000u, SMART_BAND_NOTIFICATION_TYPE_CALL,
               SMART_BAND_NOTIFICATION_PRIORITY_HIGH, 5000u);
  smart_band_event_t event;
  smart_band_event_t popped;
  size_t index;
  unsigned int call_count = 0u;
  unsigned int action_count = 0u;
  unsigned int normal_count = 0u;

  lock_ops.context = &lock;
  lock_ops.lock = fake_lock_enter;
  lock_ops.unlock = fake_lock_leave;
  CHECK(smart_band_event_inbox_init(&inbox, &lock_ops));

  for (index = 0u; index < SMART_BAND_EVENT_QUEUE_CAPACITY; index++)
    {
      event = make_generic_event(SMART_BAND_EVENT_TOUCH_ACTIVITY,
                                 (uint32_t)index);
      CHECK(smart_band_event_inbox_post(&inbox, &event));
    }
  for (index = 0u; index < 5u; index++)
    {
      CHECK(smart_band_event_inbox_pop(&inbox, &popped));
      CHECK(popped.payload.generic.code == (uint32_t)index);
    }
  for (index = 0u; index < 5u; index++)
    {
      event = make_generic_event(SMART_BAND_EVENT_TOUCH_ACTIVITY,
                                 UINT32_C(100) + (uint32_t)index);
      CHECK(smart_band_event_inbox_post(&inbox, &event));
    }
  CHECK(inbox.count == SMART_BAND_EVENT_QUEUE_CAPACITY);
  CHECK(inbox.head != 0u);

  CHECK(smart_band_notification_event_received(&call, 5000u, &event));
  CHECK(smart_band_event_inbox_post(&inbox, &event));
  CHECK(inbox.evicted == 1u && inbox.dropped == 0u);
  CHECK(smart_band_notification_event_action(
          call.id, SMART_BAND_NOTIFICATION_COMMAND_ACCEPT, 5001u, &event));
  CHECK(smart_band_event_inbox_post(&inbox, &event));
  CHECK(inbox.evicted == 2u && inbox.dropped == 0u);
  CHECK(inbox.count == SMART_BAND_EVENT_QUEUE_CAPACITY);

  while (smart_band_event_inbox_pop(&inbox, &popped))
    {
      if (popped.type == SMART_BAND_EVENT_NOTIFICATION_RECEIVED)
        {
          CHECK(popped.payload.notification_received.id == call.id);
          call_count++;
        }
      else if (popped.type == SMART_BAND_EVENT_NOTIFICATION_ACTION)
        {
          CHECK(popped.payload.notification_action.id == call.id);
          action_count++;
        }
      else
        {
          CHECK(popped.type == SMART_BAND_EVENT_TOUCH_ACTIVITY);
          normal_count++;
        }
    }
  CHECK(call_count == 1u && action_count == 1u);
  CHECK(normal_count == SMART_BAND_EVENT_QUEUE_CAPACITY - 2u);

  lock.allow = false;
  event = make_generic_event(SMART_BAND_EVENT_TOUCH_ACTIVITY, 999u);
  CHECK(!smart_band_event_inbox_post(&inbox, &event));
  CHECK(inbox.count == 0u);
  lock.allow = true;
  CHECK(smart_band_event_inbox_post(&inbox, &event));
  CHECK(smart_band_event_inbox_pop(&inbox, &popped));
  CHECK(popped.payload.generic.code == 999u);
  CHECK(lock.lock_calls == lock.unlock_calls + 1u);
  CHECK(!lock.held);
  return 0;
}

int main(void)
{
  CHECK(test_validation_sizes_and_recovery() == 0);
  CHECK(test_overlong_copy_and_exact_duplicate() == 0);
  CHECK(test_call_pending_peek_ack_and_policy() == 0);
  CHECK(test_accept_reject_and_delete() == 0);
  CHECK(test_full_protected_and_recovery() == 0);
  CHECK(test_thousand_mixed_receives() == 0);
  CHECK(test_inbox_wrap_priority_and_lock_recovery() == 0);
  puts("smart band notification service tests passed");
  return 0;
}
