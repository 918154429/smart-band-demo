#include "smart_band_runtime.h"
#include "smart_band_storage_backend.h"
#include "smart_band_sync_loopback.h"

#include "icon_assets.h"

#include <limits.h>
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

#define FAKE_WALL_BASE ((time_t)1800000000)

struct lv_obj_s
{
  int valid;
};

struct lv_font_s
{
  int unused;
};

typedef struct
{
  unsigned int ticks;
} fake_app_context_t;

typedef struct
{
  time_t wall_time;
  uint32_t monotonic_ms;
} fake_clock_t;

static smart_band_runtime_t *g_observed_runtime;
static unsigned int g_app_tick_count;
static unsigned int g_model_ticks_seen;
static smart_band_data_source_t g_heart_source_seen;
static smart_band_data_freshness_t g_heart_freshness_seen;
static uint32_t g_monotonic_seen;
static bool g_fail_app_init;
static bool g_app_tick_changed = true;

typedef struct
{
  unsigned int lock_calls;
  unsigned int unlock_calls;
  bool allow;
  bool held;
} fake_lock_t;

typedef struct
{
  unsigned int calls;
  bool accept;
  smart_band_event_t last_event;
} fake_sink_t;

static bool fake_lock_enter(void *context)
{
  fake_lock_t *lock = context;

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
  fake_lock_t *lock = context;

  lock->unlock_calls++;
  lock->held = false;
}

static bool fake_event_sink(void *context, const smart_band_event_t *event)
{
  fake_sink_t *sink = context;

  sink->calls++;
  sink->last_event = *event;
  return sink->accept;
}

lv_obj_t *lv_obj_create(lv_obj_t *parent)
{
  return parent;
}

int lv_obj_get_width(lv_obj_t *object)
{
  (void)object;
  return 1;
}

int lv_obj_get_height(lv_obj_t *object)
{
  (void)object;
  return 1;
}

void lv_obj_remove_style_all(lv_obj_t *object)
{
  (void)object;
}

void lv_obj_set_pos(lv_obj_t *object, int x, int y)
{
  (void)object;
  (void)x;
  (void)y;
}

void lv_obj_set_size(lv_obj_t *object, int width, int height)
{
  (void)object;
  (void)width;
  (void)height;
}

int lv_obj_is_valid(lv_obj_t *object)
{
  return object != NULL && object->valid;
}

void lv_obj_del(lv_obj_t *object)
{
  if (object != NULL)
    {
      object->valid = 0;
    }
}

const lv_image_dsc_t smart_band_icon_weather = {0};
const lv_image_dsc_t smart_band_icon_calculator = {0};
const lv_image_dsc_t smart_band_icon_timer = {0};
const lv_image_dsc_t smart_band_icon_game2048 = {0};
const lv_image_dsc_t smart_band_icon_stopwatch = {0};
const lv_image_dsc_t smart_band_icon_mines = {0};
const lv_image_dsc_t smart_band_icon_tetris = {0};
const lv_image_dsc_t smart_band_icon_wooden_fish = {0};

static int fake_app_init(void *context)
{
  fake_app_context_t *app = context;

  memset(app, 0, sizeof(*app));
  if (g_fail_app_init)
    {
      g_fail_app_init = false;
      return -1;
    }

  return 0;
}

static bool observed_tick(void *context, uint32_t now_ms)
{
  fake_app_context_t *app = context;
  const smart_band_metric_info_t *heart;

  app->ticks++;
  g_app_tick_count++;
  g_monotonic_seen = now_ms;
  if (g_observed_runtime != NULL)
    {
      g_model_ticks_seen = g_observed_runtime->model.ticks;
      heart = smart_band_state_metric_info(
        &g_observed_runtime->model, SMART_BAND_METRIC_HEART_RATE);
      if (heart != NULL)
        {
          g_heart_source_seen = heart->source;
          g_heart_freshness_seen = heart->freshness;
        }
    }

  return g_app_tick_changed;
}

#define FAKE_OPS(name, tick_fn)                 \
  const smart_band_app_ops_t name =             \
  {                                              \
    .context_size = sizeof(fake_app_context_t),  \
    .init = fake_app_init,                       \
    .tick = tick_fn                              \
  }

FAKE_OPS(smart_band_weather_app_ops, NULL);
FAKE_OPS(smart_band_calculator_app_ops, NULL);
FAKE_OPS(smart_band_timer_app_ops, observed_tick);
FAKE_OPS(smart_band_2048_app_ops, NULL);
FAKE_OPS(smart_band_stopwatch_app_ops, NULL);
FAKE_OPS(smart_band_mines_app_ops, NULL);
FAKE_OPS(smart_band_tetris_app_ops, NULL);
FAKE_OPS(smart_band_wooden_fish_app_ops, NULL);

static time_t fake_wall_now(void *context)
{
  return ((fake_clock_t *)context)->wall_time;
}

static uint32_t fake_monotonic_now(void *context)
{
  return ((fake_clock_t *)context)->monotonic_ms;
}

static smart_band_event_t make_event(smart_band_event_type_t type,
                                     uint32_t code)
{
  smart_band_event_t event;

  memset(&event, 0, sizeof(event));
  event.type = type;
  event.payload.generic.code = code;
  event.payload.generic.value = (int32_t)code;
  return event;
}

static smart_band_notification_input_t make_notification_input(
  uint32_t id, smart_band_notification_type_t type,
  smart_band_notification_priority_t priority, const char *body)
{
  smart_band_notification_input_t input;

  input.id = id;
  input.type = type;
  input.priority = priority;
  input.source = "runtime-test";
  input.title = "notification";
  input.body = body;
  input.wall_timestamp = UINT64_C(1800000000) + id;
  return input;
}

static int test_event_queue(void)
{
  smart_band_event_queue_t queue;
  smart_band_event_t event;
  smart_band_event_t popped;
  size_t index;
  int type;

  smart_band_event_queue_init(NULL);
  smart_band_event_queue_init(&queue);
  CHECK(smart_band_event_queue_count(NULL) == 0);
  CHECK(!smart_band_event_queue_pop(&queue, &popped));
  CHECK(!smart_band_event_queue_pop(NULL, &popped));
  CHECK(!smart_band_event_queue_pop(&queue, NULL));
  CHECK(!smart_band_event_queue_push(NULL, &event));

  for (type = SMART_BAND_EVENT_METRICS_UPDATED;
       type <= SMART_BAND_EVENT_STORAGE_FLUSH_REQUEST; type++)
    {
      event = make_event((smart_band_event_type_t)type, (uint32_t)type);
      CHECK(smart_band_event_priority(&event) >=
            SMART_BAND_EVENT_PRIORITY_LOW);
    }

  event = make_event(SMART_BAND_EVENT_METRICS_UPDATED, 0);
  event.payload.metrics.mask = 1u;
  event.monotonic_ms = 10;
  CHECK(smart_band_event_queue_push(&queue, &event));
  event.payload.metrics.mask = 4u;
  event.monotonic_ms = 20;
  CHECK(smart_band_event_queue_push(&queue, &event));
  CHECK(queue.count == 1);
  CHECK(queue.coalesced == 1);
  CHECK(smart_band_event_queue_pop(&queue, &popped));
  CHECK(popped.monotonic_ms == 20);
  CHECK(popped.payload.metrics.mask == 5u);

  for (index = 0; index < SMART_BAND_EVENT_QUEUE_CAPACITY; index++)
    {
      event = make_event(SMART_BAND_EVENT_TOUCH_ACTIVITY, (uint32_t)index);
      CHECK(smart_band_event_queue_push(&queue, &event));
    }

  event = make_event(SMART_BAND_EVENT_TOUCH_ACTIVITY, 100);
  CHECK(!smart_band_event_queue_push(&queue, &event));
  CHECK(queue.dropped == 1);
  event = make_event(SMART_BAND_EVENT_NOTIFICATION_RECEIVED, 0);
  event.payload.notification_received.type = SMART_BAND_NOTIFICATION_TYPE_CALL;
  event.payload.notification_received.priority =
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH;
  CHECK(smart_band_event_queue_push(&queue, &event));
  CHECK(queue.evicted == 1);
  CHECK(queue.count == SMART_BAND_EVENT_QUEUE_CAPACITY);

  CHECK(smart_band_event_queue_pop(&queue, &popped));
  CHECK(popped.type == SMART_BAND_EVENT_NOTIFICATION_RECEIVED);
  CHECK(popped.payload.notification_received.type ==
        SMART_BAND_NOTIFICATION_TYPE_CALL);
  CHECK(smart_band_event_queue_pop(&queue, &popped));
  CHECK(popped.payload.generic.code == 1);

  while (smart_band_event_queue_pop(&queue, &popped))
    {
    }

  event = make_event(SMART_BAND_EVENT_NONE, 0);
  CHECK(!smart_band_event_queue_push(&queue, &event));
  event.type =
    (smart_band_event_type_t)(SMART_BAND_EVENT_STORAGE_FLUSH_REQUEST + 1);
  CHECK(!smart_band_event_queue_push(&queue, &event));
  CHECK(!smart_band_event_queue_take(&queue,
                                     SMART_BAND_EVENT_TOUCH_ACTIVITY,
                                     &popped));
  CHECK(smart_band_event_priority(NULL) == SMART_BAND_EVENT_PRIORITY_LOW);
  event = make_event(SMART_BAND_EVENT_STORAGE_FLUSH_REQUEST, 0);
  CHECK(smart_band_event_priority(&event) ==
        SMART_BAND_EVENT_PRIORITY_CRITICAL);
  CHECK(!smart_band_event_queue_take(NULL,
                                     SMART_BAND_EVENT_TOUCH_ACTIVITY,
                                     &popped));
  CHECK(!smart_band_event_queue_take(&queue,
                                     SMART_BAND_EVENT_TOUCH_ACTIVITY, NULL));
  CHECK(!smart_band_event_queue_take(&queue, SMART_BAND_EVENT_NONE,
                                     &popped));
  event = make_event(SMART_BAND_EVENT_NOTIFICATION_RECEIVED, 0);
  event.payload.notification_received.type = SMART_BAND_NOTIFICATION_TYPE_APP;
  event.payload.notification_received.priority =
    SMART_BAND_NOTIFICATION_PRIORITY_NORMAL;
  CHECK(smart_band_event_priority(&event) ==
        SMART_BAND_EVENT_PRIORITY_NORMAL);
  event.payload.notification_received.type = SMART_BAND_NOTIFICATION_TYPE_CALL;
  CHECK(smart_band_event_priority(&event) ==
        SMART_BAND_EVENT_PRIORITY_HIGH);
  return 0;
}

static int test_clock_wrap_and_rollback(void)
{
  fake_clock_t fake = {FAKE_WALL_BASE, UINT32_MAX - 5u};
  smart_band_clock_source_t source =
    {fake_wall_now, fake_monotonic_now, &fake};
  smart_band_clock_t clock;
  smart_band_clock_sample_t sample;

  CHECK(smart_band_clock_init(NULL, &source) != 0);
  CHECK(smart_band_clock_init(&clock, NULL) != 0);
  CHECK(smart_band_clock_init(&clock, &source) == 0);
  CHECK(!smart_band_clock_sample(&clock, NULL));
  CHECK(smart_band_clock_sample(&clock, &sample));
  CHECK(sample.elapsed_ms == 0);
  CHECK(!sample.wall_rollback);

  fake.wall_time = FAKE_WALL_BASE + 1;
  fake.monotonic_ms = 3;
  CHECK(smart_band_clock_sample(&clock, &sample));
  CHECK(sample.elapsed_ms == 9);
  CHECK(!sample.wall_rollback);

  fake.wall_time = FAKE_WALL_BASE - 100;
  fake.monotonic_ms = 13;
  CHECK(smart_band_clock_sample(&clock, &sample));
  CHECK(sample.elapsed_ms == 19);
  CHECK(sample.wall_rollback);
  CHECK(clock.wall_rollback_count == 1);

  fake.wall_time = (time_t)-1;
  fake.monotonic_ms = 14;
  CHECK(smart_band_clock_sample(&clock, &sample));
  CHECK(!sample.wall_valid);
  CHECK(!sample.wall_rollback);
  fake.wall_time = FAKE_WALL_BASE - 200;
  fake.monotonic_ms = 15;
  CHECK(smart_band_clock_sample(&clock, &sample));
  CHECK(sample.wall_rollback);
  CHECK(clock.wall_rollback_count == 2);
  source.wall_now = NULL;
  CHECK(smart_band_clock_init(&clock, &source) == 0);
  CHECK(smart_band_clock_sample(&clock, &sample));
  CHECK(!sample.wall_valid);
  CHECK(!smart_band_clock_wall_is_valid(0));
  CHECK(smart_band_clock_wall_is_valid(FAKE_WALL_BASE));
  return 0;
}

static int test_capability_base(void)
{
  smart_band_capabilities_t capabilities;

  smart_band_capabilities_init_base(NULL);
  smart_band_capabilities_init_base(&capabilities);
  CHECK(capabilities.display);
  CHECK(capabilities.backlight);
  CHECK(capabilities.touch);
  CHECK(capabilities.monotonic_clock);
  CHECK(!capabilities.rtc);
  CHECK(!capabilities.storage);
  CHECK(!capabilities.ble);
  CHECK(!capabilities.sleep);
  CHECK(!capabilities.wake);
  return 0;
}

static int test_runtime_tick_order_and_clock_correction(void)
{
  fake_clock_t fake = {FAKE_WALL_BASE, UINT32_MAX - 5u};
  smart_band_clock_source_t source =
    {fake_wall_now, fake_monotonic_now, &fake};
  smart_band_capabilities_t capabilities;
  smart_band_runtime_t runtime;
  smart_band_event_t input;
  const smart_band_metric_info_t *heart;
  size_t index;

  memset(&capabilities, 0, sizeof(capabilities));
  capabilities.display = true;
  capabilities.heart_rate = true;
  capabilities.rtc = true;
  CHECK(smart_band_runtime_init(NULL, &source, &capabilities) != 0);
  CHECK(smart_band_runtime_init(&runtime, &source, &capabilities) == 0);
  CHECK(runtime.capabilities.display);
  CHECK(runtime.capabilities.heart_rate);
  CHECK(!runtime.capabilities.storage);
  capabilities.display = false;
  capabilities.storage = true;
  CHECK(runtime.capabilities.display);
  CHECK(!runtime.capabilities.storage);
  CHECK(!smart_band_runtime_post(NULL, &input));

  smart_band_state_set_data_mode_at(&runtime.model,
                                    SMART_BAND_DATA_MODE_AUTO,
                                    FAKE_WALL_BASE);
  CHECK(smart_band_state_publish_metric_at(
    &runtime.model, SMART_BAND_METRIC_HEART_RATE, 104,
    SMART_BAND_DATA_SOURCE_SENSOR, FAKE_WALL_BASE, 0));
  g_observed_runtime = &runtime;
  g_app_tick_count = 0;

  fake.wall_time = FAKE_WALL_BASE + 1;
  fake.monotonic_ms = 3;
  input = make_event(SMART_BAND_EVENT_TOUCH_ACTIVITY, 1);
  CHECK(smart_band_runtime_post(&runtime, &input));
  CHECK(smart_band_runtime_tick(&runtime, true));
  CHECK(smart_band_event_queue_count(&runtime.events) == 1);
  CHECK(runtime.model.ticks == 1);
  CHECK(g_app_tick_count == 1);
  CHECK(g_model_ticks_seen == 1);
  CHECK(g_monotonic_seen == 3);
  CHECK(g_heart_source_seen == SMART_BAND_DATA_SOURCE_SENSOR);
  CHECK(g_heart_freshness_seen == SMART_BAND_DATA_FRESHNESS_STALE);
  CHECK(runtime.clock.elapsed_ms == 9);

  for (index = 1; index < SMART_BAND_EVENT_QUEUE_CAPACITY; index++)
    {
      input = make_event(SMART_BAND_EVENT_TOUCH_ACTIVITY, (uint32_t)index);
      CHECK(smart_band_runtime_post(&runtime, &input));
    }
  CHECK(smart_band_event_queue_count(&runtime.events) ==
        SMART_BAND_EVENT_QUEUE_CAPACITY);

  fake.wall_time = FAKE_WALL_BASE - 100;
  fake.monotonic_ms = 13;
  CHECK(smart_band_runtime_tick(&runtime, false));
  heart = smart_band_state_metric_info(&runtime.model,
                                       SMART_BAND_METRIC_HEART_RATE);
  CHECK(heart != NULL);
  CHECK(heart->source == SMART_BAND_DATA_SOURCE_SIMULATED);
  CHECK(runtime.clock.wall_rollback_count == 1);
  CHECK(runtime.clock.elapsed_ms == 19);

  CHECK(smart_band_runtime_refresh_sensors(&runtime));
  while (smart_band_event_queue_pop(&runtime.events, &input))
    {
      CHECK(input.type == SMART_BAND_EVENT_TOUCH_ACTIVITY);
    }
  CHECK(!smart_band_runtime_tick(NULL, false));
  smart_band_runtime_deinit(&runtime);
  CHECK(!runtime.initialized);
  smart_band_runtime_deinit(&runtime);
  smart_band_runtime_deinit(NULL);
  g_observed_runtime = NULL;
  return 0;
}

static int test_runtime_defaults_and_init_rollback(void)
{
  fake_clock_t fake = {FAKE_WALL_BASE, 1};
  smart_band_clock_source_t source =
    {fake_wall_now, fake_monotonic_now, &fake};
  smart_band_runtime_t runtime;

  CHECK(smart_band_runtime_init(&runtime, &source, NULL) == 0);
  CHECK(runtime.capabilities.display);
  CHECK(runtime.storage_initialized);
  CHECK(runtime.storage.last_result == SMART_BAND_STORE_UNAVAILABLE);
  smart_band_runtime_deinit(&runtime);

  g_fail_app_init = true;
  CHECK(smart_band_runtime_init(&runtime, &source, NULL) != 0);
  CHECK(!runtime.initialized);
  CHECK(!runtime.sensors_initialized);
  return 0;
}

static int test_runtime_storage_failure_does_not_block_startup(void)
{
  static smart_band_storage_memory_t memory;
  fake_clock_t fake = {FAKE_WALL_BASE, 1};
  smart_band_clock_source_t source =
    {fake_wall_now, fake_monotonic_now, &fake};
  smart_band_storage_t storage;
  smart_band_platform_t platform;
  smart_band_runtime_t runtime;
  const uint8_t corrupt[] = {0x53, 0x42, 0x00};

  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context,
                           SMART_BAND_RUNTIME_CHECKPOINT_SLOT_A,
                           corrupt, sizeof(corrupt)) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(storage.ops->write(storage.context,
                           SMART_BAND_RUNTIME_CHECKPOINT_SLOT_B,
                           corrupt, sizeof(corrupt)) ==
        SMART_BAND_PLATFORM_OK);
  smart_band_platform_init_noop(&platform);
  platform.storage = storage;
  CHECK(smart_band_runtime_init_with_platform(
          &runtime, &source, NULL, &platform) == 0);
  CHECK(runtime.initialized);
  CHECK(runtime.storage_initialized);
  CHECK(runtime.storage.last_result == SMART_BAND_STORE_DEGRADED);
  CHECK(runtime.capabilities.storage);
  CHECK(smart_band_runtime_tick(&runtime, false));
  smart_band_runtime_deinit(&runtime);

  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  platform.storage = storage;
  CHECK(smart_band_runtime_init_with_platform(
          &runtime, &source, NULL, &platform) == 0);
  CHECK(runtime.storage.last_result == SMART_BAND_STORE_DEFAULTED);
  CHECK(runtime.capabilities.storage);
  smart_band_runtime_deinit(&runtime);
  return 0;
}

static int test_runtime_without_rtc(void)
{
  fake_clock_t fake = {(time_t)-1, 100};
  smart_band_clock_source_t source =
    {NULL, fake_monotonic_now, &fake};
  smart_band_runtime_t runtime;

  CHECK(smart_band_runtime_init(&runtime, &source, NULL) == 0);
  CHECK(!runtime.capabilities.rtc);
  CHECK(!runtime.model.time_valid);
  CHECK(strcmp(runtime.model.time_text, "--:--") == 0);
  fake.monotonic_ms = 200;
  CHECK(smart_band_runtime_tick(&runtime, false));
  CHECK(runtime.model.ticks == 1);
  CHECK(!runtime.model.time_valid);
  CHECK(runtime.clock.elapsed_ms == 100);

  runtime.clock.source.monotonic_now = NULL;
  CHECK(!smart_band_runtime_tick(&runtime, false));
  CHECK(runtime.model.ticks == 1);
  CHECK(!smart_band_runtime_refresh_sensors(&runtime));
  smart_band_runtime_deinit(&runtime);
  return 0;
}

static int test_runtime_recovers_when_rtc_becomes_valid(void)
{
  fake_clock_t fake = {(time_t)-1, 100};
  smart_band_clock_source_t source =
    {fake_wall_now, fake_monotonic_now, &fake};
  smart_band_runtime_t runtime;

  CHECK(smart_band_runtime_init(&runtime, &source, NULL) == 0);
  CHECK(runtime.capabilities.rtc);
  CHECK(!runtime.model.time_valid);
  CHECK(strcmp(runtime.model.time_text, "--:--") == 0);

  fake.wall_time = FAKE_WALL_BASE;
  fake.monotonic_ms = 200;
  CHECK(smart_band_runtime_tick(&runtime, false));
  CHECK(runtime.model.time_valid);
  CHECK(runtime.last_clock.wall_valid);
  CHECK(runtime.clock.elapsed_ms == 100);

  smart_band_runtime_deinit(&runtime);
  return 0;
}

static int test_event_inbox_contract(void)
{
  smart_band_event_inbox_t inbox;
  smart_band_event_lock_t lock_ops;
  smart_band_event_t event;
  smart_band_event_t popped;
  fake_lock_t lock = {0, 0, true, false};
  size_t index;

  memset(&lock_ops, 0, sizeof(lock_ops));
  CHECK(!smart_band_event_inbox_init(NULL, NULL));
  CHECK(smart_band_event_inbox_init(&inbox, NULL));
  event = make_event(SMART_BAND_EVENT_TOUCH_ACTIVITY, 1);
  CHECK(!smart_band_event_inbox_post(&inbox, &event));
  CHECK(!smart_band_event_inbox_close(&inbox));

  lock_ops.context = &lock;
  lock_ops.lock = fake_lock_enter;
  CHECK(!smart_band_event_inbox_init(&inbox, &lock_ops));
  lock_ops.unlock = fake_lock_leave;
  CHECK(smart_band_event_inbox_init(&inbox, &lock_ops));
  CHECK(!smart_band_event_inbox_post(NULL, &event));
  CHECK(!smart_band_event_inbox_post(&inbox, NULL));
  CHECK(!smart_band_event_inbox_pop(NULL, &popped));
  CHECK(!smart_band_event_inbox_pop(&inbox, NULL));

  for (index = 0; index < SMART_BAND_EVENT_QUEUE_CAPACITY; index++)
    {
      event = make_event(SMART_BAND_EVENT_TOUCH_ACTIVITY, (uint32_t)index);
      CHECK(smart_band_event_inbox_post(&inbox, &event));
    }

  CHECK(!smart_band_event_inbox_post(&inbox, &event));
  CHECK(inbox.dropped == 1);
  for (index = 0; index < SMART_BAND_EVENT_QUEUE_CAPACITY; index++)
    {
      CHECK(smart_band_event_inbox_pop(&inbox, &popped));
      CHECK(popped.payload.generic.code == (uint32_t)index);
    }

  CHECK(!smart_band_event_inbox_pop(&inbox, &popped));
  CHECK(smart_band_event_inbox_close(&inbox));
  CHECK(!smart_band_event_inbox_post(&inbox, &event));
  CHECK(lock.lock_calls == lock.unlock_calls);
  CHECK(!lock.held);

  lock.allow = false;
  CHECK(!smart_band_event_inbox_post(&inbox, &event));
  CHECK(!smart_band_event_inbox_pop(&inbox, &popped));
  CHECK(!smart_band_event_inbox_close(&inbox));
  return 0;
}

static int test_platform_noop_contract(void)
{
  smart_band_platform_t platform;
  smart_band_haptic_pulse_t pulse = {10, 20, 50};
  unsigned char buffer[4] = {0};
  size_t actual = 99;

  smart_band_platform_init_noop(NULL);
  smart_band_platform_init_noop(&platform);
  CHECK(platform.storage.ops->read(platform.storage.context, 1, buffer,
                                   sizeof(buffer), &actual) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(actual == 0);
  CHECK(platform.storage.ops->read(platform.storage.context, 1, buffer,
                                   sizeof(buffer), NULL) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(platform.storage.ops->write(platform.storage.context, 1, buffer,
                                    sizeof(buffer)) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(platform.storage.ops->flush(platform.storage.context) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(platform.power.ops->set_display_enabled(platform.power.context,
                                                 true) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(platform.power.ops->set_backlight(platform.power.context, 50) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(platform.power.ops->request_sleep(platform.power.context) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(platform.haptic.ops->play(platform.haptic.context, &pulse, 1) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(platform.haptic.ops->stop(platform.haptic.context) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(platform.sync.ops->start(platform.sync.context, fake_event_sink,
                                 NULL) == SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(platform.sync.ops->send(platform.sync.context, buffer,
                                sizeof(buffer)) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  actual = 99;
  CHECK(platform.sync.ops->poll(platform.sync.context, buffer,
                                sizeof(buffer), &actual) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(actual == 0);
  CHECK(platform.sync.ops->poll(platform.sync.context, buffer,
                                sizeof(buffer), NULL) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(platform.sync.ops->stop(platform.sync.context) ==
        SMART_BAND_PLATFORM_UNAVAILABLE);
  CHECK(platform.sync.ops->status(platform.sync.context) ==
        SMART_BAND_SYNC_STOPPED);
  CHECK(platform.sync.ops->mtu(platform.sync.context) == 0);
  return 0;
}

static int test_sync_loopback_contract(void)
{
  smart_band_sync_loopback_t loopback;
  smart_band_sync_transport_t transport;
  smart_band_sync_transport_t null_transport;
  fake_sink_t sink;
  unsigned char input[SMART_BAND_SYNC_LOOPBACK_MTU + 1];
  unsigned char output[SMART_BAND_SYNC_LOOPBACK_MTU];
  size_t actual;
  size_t index;

  memset(&sink, 0, sizeof(sink));
  memset(input, 0x5a, sizeof(input));
  smart_band_sync_loopback_init(NULL);
  smart_band_sync_loopback_init(&loopback);
  transport = smart_band_sync_loopback_transport(&loopback);
  null_transport = smart_band_sync_loopback_transport(NULL);
  CHECK(null_transport.ops->status(null_transport.context) ==
        SMART_BAND_SYNC_STOPPED);
  CHECK(null_transport.ops->mtu(null_transport.context) == 0);
  CHECK(null_transport.ops->start(null_transport.context, fake_event_sink,
                                  &sink) == SMART_BAND_PLATFORM_INVALID);
  CHECK(null_transport.ops->stop(null_transport.context) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(transport.ops->mtu(transport.context) ==
        SMART_BAND_SYNC_LOOPBACK_MTU);
  CHECK(transport.ops->status(transport.context) == SMART_BAND_SYNC_STOPPED);
  CHECK(transport.ops->send(transport.context, input, 1) ==
        SMART_BAND_PLATFORM_BUSY);
  CHECK(transport.ops->start(transport.context, NULL, NULL) ==
        SMART_BAND_PLATFORM_INVALID);
  sink.accept = false;
  CHECK(transport.ops->start(transport.context, fake_event_sink, &sink) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(transport.ops->start(transport.context, fake_event_sink, &sink) ==
        SMART_BAND_PLATFORM_BUSY);
  CHECK(transport.ops->status(transport.context) == SMART_BAND_SYNC_STARTED);
  CHECK(transport.ops->send(NULL, input, 1) == SMART_BAND_PLATFORM_INVALID);
  CHECK(transport.ops->send(transport.context, NULL, 1) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(transport.ops->send(transport.context, input, 0) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(transport.ops->send(transport.context, input, sizeof(input)) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(transport.ops->send(transport.context, input, 1) ==
        SMART_BAND_PLATFORM_BUSY);
  CHECK(loopback.count == 0);

  sink.accept = true;
  for (index = 0; index < SMART_BAND_SYNC_LOOPBACK_CAPACITY; index++)
    {
      input[0] = (unsigned char)index;
      CHECK(transport.ops->send(transport.context, input, 2) ==
            SMART_BAND_PLATFORM_OK);
    }

  CHECK(sink.calls == SMART_BAND_SYNC_LOOPBACK_CAPACITY + 1);
  CHECK(sink.last_event.type == SMART_BAND_EVENT_SYNC_REQUEST);
  CHECK(sink.last_event.payload.generic.value == 2);
  CHECK(transport.ops->send(transport.context, input, 1) ==
        SMART_BAND_PLATFORM_BUSY);
  actual = 99;
  CHECK(transport.ops->poll(NULL, output, sizeof(output), &actual) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(actual == 0);
  CHECK(transport.ops->poll(transport.context, NULL, sizeof(output),
                            &actual) == SMART_BAND_PLATFORM_INVALID);
  CHECK(transport.ops->poll(transport.context, output, sizeof(output),
                            NULL) == SMART_BAND_PLATFORM_INVALID);
  CHECK(transport.ops->poll(transport.context, output, 1, &actual) ==
        SMART_BAND_PLATFORM_INVALID);
  CHECK(transport.ops->poll(transport.context, output, sizeof(output),
                            &actual) == SMART_BAND_PLATFORM_OK);
  CHECK(actual == 2);
  CHECK(output[0] == 0);
  CHECK(transport.ops->stop(transport.context) == SMART_BAND_PLATFORM_OK);
  CHECK(transport.ops->poll(transport.context, output, sizeof(output),
                            &actual) == SMART_BAND_PLATFORM_BUSY);
  CHECK(transport.ops->start(transport.context, fake_event_sink, &sink) ==
        SMART_BAND_PLATFORM_OK);
  while (loopback.count > 0)
    {
      CHECK(transport.ops->poll(transport.context, output, sizeof(output),
                                &actual) == SMART_BAND_PLATFORM_OK);
    }
  CHECK(transport.ops->poll(transport.context, output, sizeof(output),
                            &actual) == SMART_BAND_PLATFORM_BUSY);
  CHECK(transport.ops->stop(transport.context) == SMART_BAND_PLATFORM_OK);
  return 0;
}

static int test_runtime_platform_ingress_and_dirty(void)
{
  fake_clock_t fake = {FAKE_WALL_BASE, 10};
  smart_band_clock_source_t source =
    {fake_wall_now, fake_monotonic_now, &fake};
  smart_band_capabilities_t capabilities;
  smart_band_platform_t platform;
  smart_band_sync_loopback_t loopback;
  smart_band_runtime_t runtime;
  smart_band_event_t event;
  smart_band_event_t popped;
  fake_lock_t lock = {0, 0, true, false};
  unsigned char input[] = {1, 2, 3};
  unsigned char output[4] = {0};
  size_t actual = 0;
  int injected_context = 42;

  memset(&capabilities, 0, sizeof(capabilities));
  capabilities.rtc = true;
  smart_band_platform_init_noop(&platform);
  smart_band_sync_loopback_init(&loopback);
  platform.sync = smart_band_sync_loopback_transport(&loopback);
  platform.event_lock.context = &lock;
  platform.event_lock.lock = fake_lock_enter;
  platform.event_lock.unlock = fake_lock_leave;
  platform.storage.context = &injected_context;
  platform.power.context = &injected_context;
  platform.haptic.context = &injected_context;

  CHECK(smart_band_runtime_init_with_platform(
          &runtime, &source, &capabilities, &platform) == 0);
  CHECK(runtime.platform.storage.context == &injected_context);
  CHECK(runtime.platform.power.context == &injected_context);
  CHECK(runtime.platform.haptic.context == &injected_context);
  CHECK(smart_band_runtime_peek_dirty(&runtime) == SMART_BAND_DIRTY_ALL);
  CHECK(smart_band_runtime_take_dirty(&runtime) == SMART_BAND_DIRTY_ALL);
  CHECK(smart_band_runtime_take_dirty(NULL) == SMART_BAND_DIRTY_NONE);
  CHECK(smart_band_runtime_peek_dirty(NULL) == SMART_BAND_DIRTY_NONE);

  smart_band_state_set_data_mode_at(&runtime.model,
                                    SMART_BAND_DATA_MODE_SENSORS_ONLY,
                                    FAKE_WALL_BASE);
  g_app_tick_changed = false;
  fake.monotonic_ms++;
  CHECK(smart_band_runtime_tick(&runtime, false));
  CHECK(smart_band_runtime_take_dirty(&runtime) == SMART_BAND_DIRTY_NONE);
  g_app_tick_changed = true;
  fake.monotonic_ms++;
  CHECK(smart_band_runtime_tick(&runtime, false));
  CHECK(smart_band_runtime_take_dirty(&runtime) == SMART_BAND_DIRTY_APP);
  g_app_tick_changed = false;
  fake.wall_time += 60;
  fake.monotonic_ms++;
  CHECK(smart_band_runtime_tick(&runtime, false));
  CHECK(smart_band_runtime_take_dirty(&runtime) == SMART_BAND_DIRTY_TIME);

  smart_band_runtime_mark_dirty(&runtime,
                                SMART_BAND_DIRTY_PAGE |
                                SMART_BAND_DIRTY_STEPS | (1u << 31));
  CHECK(smart_band_runtime_take_dirty(&runtime) ==
        (SMART_BAND_DIRTY_PAGE | SMART_BAND_DIRTY_STEPS));
  smart_band_runtime_mark_dirty(NULL, SMART_BAND_DIRTY_ALL);

  event = make_event(SMART_BAND_EVENT_TOUCH_ACTIVITY, 11);
  CHECK(smart_band_runtime_post_external(&runtime, &event));
  event = make_event(SMART_BAND_EVENT_TOUCH_ACTIVITY, 12);
  CHECK(smart_band_runtime_post_external(&runtime, &event));
  CHECK(smart_band_event_queue_count(&runtime.events) == 0);
  CHECK(smart_band_runtime_drain_external(&runtime, 1) == 1);
  CHECK(smart_band_event_queue_count(&runtime.events) == 1);
  CHECK(smart_band_event_queue_pop(&runtime.events, &popped));
  CHECK(popped.payload.generic.code == 11);
  fake.monotonic_ms++;
  CHECK(smart_band_runtime_tick(&runtime, false));
  CHECK(smart_band_event_queue_pop(&runtime.events, &popped));
  CHECK(popped.payload.generic.code == 12);
  CHECK(smart_band_runtime_drain_external(NULL, 1) == 0);

  CHECK(runtime.platform.sync.ops->start(
          runtime.platform.sync.context, smart_band_runtime_post_external,
          &runtime) == SMART_BAND_PLATFORM_OK);
  CHECK(runtime.platform.sync.ops->send(runtime.platform.sync.context, input,
                                        sizeof(input)) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(smart_band_event_queue_count(&runtime.events) == 0);
  fake.monotonic_ms++;
  CHECK(smart_band_runtime_tick(&runtime, false));
  CHECK(smart_band_event_queue_pop(&runtime.events, &popped));
  CHECK(popped.type == SMART_BAND_EVENT_SYNC_REQUEST);
  CHECK(runtime.platform.sync.ops->poll(runtime.platform.sync.context, output,
                                        sizeof(output), &actual) ==
        SMART_BAND_PLATFORM_OK);
  CHECK(actual == sizeof(input));
  CHECK(memcmp(input, output, sizeof(input)) == 0);
  smart_band_runtime_deinit(&runtime);
  CHECK(loopback.started == false);
  CHECK(loopback.event_sink == NULL);
  CHECK(!smart_band_runtime_post_external(&runtime, &event));
  CHECK(lock.lock_calls == lock.unlock_calls);
  g_app_tick_changed = true;
  return 0;
}

static int test_runtime_rejects_partial_event_lock(void)
{
  fake_clock_t fake = {FAKE_WALL_BASE, 1};
  smart_band_clock_source_t source =
    {fake_wall_now, fake_monotonic_now, &fake};
  smart_band_platform_t platform;
  smart_band_runtime_t runtime;

  smart_band_platform_init_noop(&platform);
  platform.event_lock.context = &fake;
  platform.event_lock.lock = fake_lock_enter;
  CHECK(smart_band_runtime_init_with_platform(
          &runtime, &source, NULL, &platform) != 0);
  CHECK(!runtime.initialized);
  CHECK(smart_band_runtime_init_with_platform(
          &runtime, &source, NULL, NULL) == 0);
  smart_band_runtime_deinit(&runtime);
  return 0;
}

static int test_runtime_dispatches_workout_commands(void)
{
  fake_clock_t fake = {FAKE_WALL_BASE, 100};
  smart_band_clock_source_t source =
    {fake_wall_now, fake_monotonic_now, &fake};
  smart_band_runtime_t runtime;
  smart_band_event_t event;
  smart_band_event_t retained;

  CHECK(smart_band_runtime_init(&runtime, &source, NULL) == 0);
  (void)smart_band_runtime_take_dirty(&runtime);
  event = make_event(SMART_BAND_EVENT_TOUCH_ACTIVITY, 77);
  CHECK(smart_band_runtime_post(&runtime, &event));
  memset(&event, 0, sizeof(event));
  event.type = SMART_BAND_EVENT_WORKOUT_COMMAND;
  event.payload.workout.command = SMART_BAND_WORKOUT_COMMAND_START;
  event.payload.workout.mode = SMART_BAND_WORKOUT_MODE_WALK;
  CHECK(smart_band_runtime_post(&runtime, &event));

  fake.wall_time++;
  fake.monotonic_ms += 1000u;
  CHECK(smart_band_runtime_tick(&runtime, false));
  CHECK(runtime.workout.model.data.state ==
        SMART_BAND_WORKOUT_STATE_COUNTDOWN);
  CHECK(smart_band_event_queue_count(&runtime.events) == 1u);
  CHECK(smart_band_event_queue_pop(&runtime.events, &retained));
  CHECK(retained.type == SMART_BAND_EVENT_TOUCH_ACTIVITY);

  for (int index = 0; index < 3; index++)
    {
      fake.wall_time++;
      fake.monotonic_ms += 1000u;
      CHECK(smart_band_runtime_tick(&runtime, false));
    }
  CHECK(runtime.workout.model.data.state == SMART_BAND_WORKOUT_STATE_ACTIVE);

  memset(&event, 0, sizeof(event));
  event.type = SMART_BAND_EVENT_WORKOUT_COMMAND;
  event.payload.workout.command = SMART_BAND_WORKOUT_COMMAND_PAUSE;
  CHECK(smart_band_runtime_post(&runtime, &event));
  fake.wall_time++;
  fake.monotonic_ms += 1000u;
  CHECK(smart_band_runtime_tick(&runtime, false));
  CHECK(runtime.workout.model.data.state == SMART_BAND_WORKOUT_STATE_PAUSED);
  CHECK(runtime.workout.pause_count == 1u);

  event.payload.workout.command = SMART_BAND_WORKOUT_COMMAND_RESUME;
  CHECK(smart_band_runtime_post(&runtime, &event));
  fake.wall_time++;
  fake.monotonic_ms += 1000u;
  CHECK(smart_band_runtime_tick(&runtime, false));
  CHECK(runtime.workout.model.data.state == SMART_BAND_WORKOUT_STATE_ACTIVE);

  event.payload.workout.command = SMART_BAND_WORKOUT_COMMAND_FINISH;
  CHECK(smart_band_runtime_post(&runtime, &event));
  fake.wall_time++;
  fake.monotonic_ms += 1000u;
  CHECK(smart_band_runtime_tick(&runtime, false));
  CHECK(runtime.workout.model.data.state == SMART_BAND_WORKOUT_STATE_FINISHED);
  CHECK(runtime.workout.phase == SMART_BAND_WORKOUT_SERVICE_PHASE_READY);
  CHECK(runtime.history.session_count == 1u);
  CHECK(runtime.history.sessions[0].pause_count == 1u);
  CHECK((smart_band_runtime_take_dirty(&runtime) &
         (SMART_BAND_DIRTY_WORKOUT | SMART_BAND_DIRTY_HISTORY)) != 0u);
  smart_band_runtime_deinit(&runtime);
  return 0;
}

static int test_immediate_dispatch_samples_current_clock(void)
{
  fake_clock_t fake = {FAKE_WALL_BASE, 100};
  smart_band_clock_source_t source =
    {fake_wall_now, fake_monotonic_now, &fake};
  smart_band_runtime_t runtime;
  smart_band_event_t event;

  CHECK(smart_band_runtime_init(&runtime, &source, NULL) == 0);
  fake.wall_time += 2;
  fake.monotonic_ms += 750u;
  memset(&event, 0, sizeof(event));
  event.type = SMART_BAND_EVENT_WORKOUT_COMMAND;
  event.payload.workout.command = SMART_BAND_WORKOUT_COMMAND_START;
  event.payload.workout.mode = SMART_BAND_WORKOUT_MODE_RUN;
  CHECK(smart_band_runtime_post(&runtime, &event));
  smart_band_runtime_dispatch_pending(&runtime);
  CHECK(runtime.workout.model.last_monotonic_ms == 750u);
  CHECK(runtime.last_clock.elapsed_ms == 750u);
  smart_band_runtime_deinit(&runtime);
  return 0;
}

static int test_runtime_deinit_checkpoints_live_workout(void)
{
  fake_clock_t fake = {FAKE_WALL_BASE, 100};
  smart_band_clock_source_t source =
    {fake_wall_now, fake_monotonic_now, &fake};
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_platform_t platform;
  smart_band_runtime_t runtime1;
  smart_band_runtime_t runtime2;
  smart_band_event_t event;
  smart_band_workout_snapshot_t before;
  smart_band_workout_snapshot_t recovered;
  int index;

  smart_band_platform_init_noop(&platform);
  CHECK(smart_band_storage_memory_init(&memory, &storage) ==
        SMART_BAND_PLATFORM_OK);
  platform.storage = storage;
  CHECK(smart_band_runtime_init_with_platform(
          &runtime1, &source, NULL, &platform) == 0);
  memset(&event, 0, sizeof(event));
  event.type = SMART_BAND_EVENT_WORKOUT_COMMAND;
  event.payload.workout.command = SMART_BAND_WORKOUT_COMMAND_START;
  event.payload.workout.mode = SMART_BAND_WORKOUT_MODE_WALK;
  CHECK(smart_band_runtime_post(&runtime1, &event));
  smart_band_runtime_dispatch_pending(&runtime1);
  for (index = 0; index < 8; index++)
    {
      fake.wall_time++;
      fake.monotonic_ms += 1000u;
      CHECK(smart_band_runtime_tick(&runtime1, false));
    }
  CHECK(smart_band_workout_service_snapshot(&runtime1.workout, &before));
  CHECK(before.state == SMART_BAND_WORKOUT_STATE_ACTIVE);
  CHECK(before.active_duration_ms == 5000u);
  smart_band_runtime_deinit(&runtime1);

  fake.wall_time++;
  fake.monotonic_ms += 1000u;
  CHECK(smart_band_runtime_init_with_platform(
          &runtime2, &source, NULL, &platform) == 0);
  CHECK(runtime2.workout.recovery_pending);
  CHECK(smart_band_workout_service_snapshot(&runtime2.workout, &recovered));
  CHECK(recovered.state == SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION);
  CHECK(recovered.active_duration_ms == before.active_duration_ms);
  smart_band_runtime_deinit(&runtime2);
  return 0;
}

static int test_runtime_notification_ingress_and_pressure(void)
{
  fake_clock_t fake = {FAKE_WALL_BASE, 100u};
  smart_band_clock_source_t source =
    {fake_wall_now, fake_monotonic_now, &fake};
  smart_band_runtime_t runtime;
  smart_band_notification_input_t input;
  smart_band_notification_policy_t policy = {false, false};
  smart_band_notification_presentation_t presentation;
  smart_band_event_t event;
  smart_band_event_t popped;
  const smart_band_notification_t *stored;
  uint32_t presented_id;
  size_t index;

  CHECK(smart_band_runtime_init(&runtime, &source, NULL) == 0);
  CHECK(smart_band_runtime_set_notification_policy(&runtime, &policy));
  input = make_notification_input(
    1u, SMART_BAND_NOTIFICATION_TYPE_APP,
    SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, "first");
  CHECK(smart_band_runtime_post_notification(&runtime, &input, 101u));
  CHECK(smart_band_runtime_post_notification_action(
          &runtime, 1u, SMART_BAND_NOTIFICATION_COMMAND_READ, 102u));
  smart_band_runtime_dispatch_pending(&runtime);
  stored = smart_band_notification_find(&runtime.notifications.model, 1u);
  CHECK(stored != NULL && stored->read);
  CHECK(!smart_band_notification_service_peek_presentation(
          &runtime.notifications, &presented_id, &presentation));
  input.body = "updated";
  CHECK(smart_band_runtime_post_notification(&runtime, &input, 103u));
  smart_band_runtime_dispatch_pending(&runtime);
  CHECK(smart_band_notification_service_peek_presentation(
          &runtime.notifications, &presented_id, &presentation));
  CHECK(presented_id == 1u && presentation.overlay);
  CHECK(smart_band_notification_service_ack_presentation(
          &runtime.notifications, 1u));

  CHECK(smart_band_runtime_post_notification(&runtime, &input, 104u));
  smart_band_runtime_dispatch_pending(&runtime);
  CHECK(runtime.last_notification_result ==
        SMART_BAND_NOTIFICATION_SERVICE_NO_CHANGE);
  CHECK(runtime.notifications.stats.duplicates == 1u);
  CHECK(!smart_band_notification_service_peek_presentation(
          &runtime.notifications, &presented_id, &presentation));

  input = make_notification_input(
    2u, SMART_BAND_NOTIFICATION_TYPE_CALL,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "incoming call");
  CHECK(smart_band_runtime_post_notification(&runtime, &input, 105u));
  input = make_notification_input(
    3u, SMART_BAND_NOTIFICATION_TYPE_APP,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "important app");
  CHECK(smart_band_runtime_post_notification(&runtime, &input, 106u));
  smart_band_runtime_dispatch_pending(&runtime);
  CHECK(smart_band_notification_service_peek_presentation(
          &runtime.notifications, &presented_id, &presentation));
  CHECK(presented_id == 2u && presentation.full_screen);

  policy.workout_active = true;
  CHECK(smart_band_runtime_set_notification_policy(&runtime, &policy));
  CHECK(smart_band_notification_service_peek_presentation(
          &runtime.notifications, &presented_id, &presentation));
  CHECK(presented_id == 2u && presentation.overlay &&
        !presentation.full_screen);
  CHECK(smart_band_runtime_post_notification_action(
          &runtime, 2u, SMART_BAND_NOTIFICATION_COMMAND_ACCEPT, 106u));
  smart_band_runtime_dispatch_pending(&runtime);
  CHECK(!smart_band_notification_service_peek_presentation(
          &runtime.notifications, &presented_id, &presentation));

  policy.dnd_enabled = true;
  CHECK(smart_band_runtime_set_notification_policy(&runtime, &policy));
  input = make_notification_input(
    4u, SMART_BAND_NOTIFICATION_TYPE_SMS,
    SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, "dnd message");
  CHECK(smart_band_runtime_post_notification(&runtime, &input, 107u));
  smart_band_runtime_dispatch_pending(&runtime);
  CHECK(smart_band_notification_find(&runtime.notifications.model, 4u) !=
        NULL);
  CHECK(!smart_band_notification_service_peek_presentation(
          &runtime.notifications, &presented_id, &presentation));
  CHECK(smart_band_runtime_post_notification_action(
          &runtime, 4u, SMART_BAND_NOTIFICATION_COMMAND_DELETE, 108u));
  smart_band_runtime_dispatch_pending(&runtime);
  CHECK(smart_band_notification_find(&runtime.notifications.model, 4u) ==
        NULL);

  while (smart_band_event_queue_pop(&runtime.events, &popped))
    {
    }
  for (index = 0u; index < SMART_BAND_EVENT_QUEUE_CAPACITY; index++)
    {
      event = make_event(SMART_BAND_EVENT_TOUCH_ACTIVITY, (uint32_t)index);
      CHECK(smart_band_runtime_post(&runtime, &event));
    }
  policy.dnd_enabled = false;
  policy.workout_active = false;
  CHECK(smart_band_runtime_set_notification_policy(&runtime, &policy));
  input = make_notification_input(
    5u, SMART_BAND_NOTIFICATION_TYPE_CALL,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "priority call");
  CHECK(smart_band_runtime_post_notification(&runtime, &input, 109u));
  CHECK(runtime.events.evicted == 1u);
  smart_band_runtime_dispatch_pending(&runtime);
  CHECK(smart_band_notification_find(&runtime.notifications.model, 5u) !=
        NULL);

  while (smart_band_event_queue_pop(&runtime.events, &popped))
    {
    }
  for (index = 0u; index < SMART_BAND_EVENT_QUEUE_CAPACITY; index++)
    {
      event = make_event(SMART_BAND_EVENT_STORAGE_FLUSH_REQUEST,
                         (uint32_t)index);
      CHECK(smart_band_runtime_post(&runtime, &event));
    }
  input = make_notification_input(
    6u, SMART_BAND_NOTIFICATION_TYPE_APP,
    SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, "retry");
  CHECK(!smart_band_runtime_post_notification(&runtime, &input, 110u));
  CHECK(smart_band_event_queue_pop(&runtime.events, &popped));
  CHECK(smart_band_runtime_post_notification(&runtime, &input, 111u));
  smart_band_runtime_dispatch_pending(&runtime);
  CHECK(smart_band_notification_find(&runtime.notifications.model, 6u) !=
        NULL);
  CHECK((smart_band_runtime_take_dirty(&runtime) &
         SMART_BAND_DIRTY_NOTIFICATION) != 0u);

  CHECK(!smart_band_runtime_inject_notification_demo(NULL, 1u, 0u, 0u));
  CHECK(smart_band_runtime_inject_notification_demo(
          &runtime, 7u, 1u, 112u));
  smart_band_runtime_dispatch_pending(&runtime);
  CHECK(runtime.notifications.stats.received >= 7u);
  smart_band_runtime_deinit(&runtime);
  return 0;
}

static int test_runtime_external_notification_ingress(void)
{
  fake_clock_t fake = {FAKE_WALL_BASE, 200u};
  smart_band_clock_source_t source =
    {fake_wall_now, fake_monotonic_now, &fake};
  smart_band_platform_t platform;
  smart_band_runtime_t runtime;
  fake_lock_t lock = {0u, 0u, true, false};
  smart_band_notification_input_t input = make_notification_input(
    700u, SMART_BAND_NOTIFICATION_TYPE_SMS,
    SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, "external");

  smart_band_platform_init_noop(&platform);
  platform.event_lock.context = &lock;
  platform.event_lock.lock = fake_lock_enter;
  platform.event_lock.unlock = fake_lock_leave;
  CHECK(smart_band_runtime_init_with_platform(
          &runtime, &source, NULL, &platform) == 0);
  CHECK(smart_band_runtime_post_notification_external(
          &runtime, &input, 201u));
  CHECK(smart_band_notification_find(&runtime.notifications.model, 700u) ==
        NULL);
  smart_band_runtime_dispatch_pending(&runtime);
  CHECK(smart_band_notification_find(&runtime.notifications.model, 700u) !=
        NULL);
  smart_band_runtime_deinit(&runtime);
  CHECK(!smart_band_runtime_post_notification_external(
          &runtime, &input, 202u));
  CHECK(lock.lock_calls == lock.unlock_calls);
  return 0;
}

int main(void)
{
  CHECK(test_event_queue() == 0);
  CHECK(test_clock_wrap_and_rollback() == 0);
  CHECK(test_capability_base() == 0);
  CHECK(test_runtime_tick_order_and_clock_correction() == 0);
  CHECK(test_runtime_defaults_and_init_rollback() == 0);
  CHECK(test_runtime_storage_failure_does_not_block_startup() == 0);
  CHECK(test_runtime_without_rtc() == 0);
  CHECK(test_event_inbox_contract() == 0);
  CHECK(test_platform_noop_contract() == 0);
  CHECK(test_sync_loopback_contract() == 0);
  CHECK(test_runtime_platform_ingress_and_dirty() == 0);
  CHECK(test_runtime_rejects_partial_event_lock() == 0);
  CHECK(test_runtime_recovers_when_rtc_becomes_valid() == 0);
  CHECK(test_runtime_dispatches_workout_commands() == 0);
  CHECK(test_immediate_dispatch_samples_current_clock() == 0);
  CHECK(test_runtime_deinit_checkpoints_live_workout() == 0);
  CHECK(test_runtime_notification_ingress_and_pressure() == 0);
  CHECK(test_runtime_external_notification_ingress() == 0);
  puts("smart band central runtime production tests passed");
  return 0;
}
