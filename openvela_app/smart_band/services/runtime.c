#include "smart_band_runtime.h"
#include "notification_demo.h"

#include <string.h>

typedef struct
{
  smart_band_page_t page;
  bool time_valid;
  char time_text[SMART_BAND_TIME_TEXT_LEN];
  char date_text[SMART_BAND_DATE_TEXT_LEN];
  char status_text[SMART_BAND_STATUS_TEXT_LEN];
  int values[SMART_BAND_METRIC_COUNT];
  smart_band_data_source_t sources[SMART_BAND_METRIC_COUNT];
  smart_band_data_freshness_t freshness[SMART_BAND_METRIC_COUNT];
  int step_goal;
  bool battery_charging;
} smart_band_view_snapshot_t;

static int metric_value(const smart_band_state_t *model,
                        smart_band_metric_t metric)
{
  switch (metric)
    {
      case SMART_BAND_METRIC_HEART_RATE:
        return model->heart_rate;
      case SMART_BAND_METRIC_STEPS:
        return model->steps;
      case SMART_BAND_METRIC_BATTERY:
        return model->battery_percent;
      case SMART_BAND_METRIC_TEMPERATURE:
        return model->temperature_c;
      case SMART_BAND_METRIC_HUMIDITY:
        return model->humidity_percent;
      case SMART_BAND_METRIC_COUNT:
      default:
        return 0;
    }
}

static void capture_view_snapshot(const smart_band_state_t *model,
                                  smart_band_view_snapshot_t *snapshot)
{
  smart_band_metric_t metric;

  memset(snapshot, 0, sizeof(*snapshot));
  snapshot->page = model->page;
  snapshot->time_valid = model->time_valid;
  memcpy(snapshot->time_text, model->time_text, sizeof(snapshot->time_text));
  memcpy(snapshot->date_text, model->date_text, sizeof(snapshot->date_text));
  memcpy(snapshot->status_text, model->status_text,
         sizeof(snapshot->status_text));
  snapshot->step_goal = model->step_goal;
  snapshot->battery_charging = model->battery_charging;
  for (metric = SMART_BAND_METRIC_HEART_RATE;
       metric < SMART_BAND_METRIC_COUNT; metric++)
    {
      snapshot->values[metric] = metric_value(model, metric);
      snapshot->sources[metric] = model->metrics[metric].source;
      snapshot->freshness[metric] = model->metrics[metric].freshness;
    }
}

static bool metric_snapshot_changed(const smart_band_view_snapshot_t *before,
                                    const smart_band_view_snapshot_t *after,
                                    smart_band_metric_t metric)
{
  return before->values[metric] != after->values[metric] ||
         before->sources[metric] != after->sources[metric] ||
         before->freshness[metric] != after->freshness[metric];
}

static smart_band_dirty_flags_t
view_changes(const smart_band_view_snapshot_t *before,
             const smart_band_view_snapshot_t *after)
{
  smart_band_dirty_flags_t dirty = SMART_BAND_DIRTY_NONE;

  if (before->page != after->page)
    {
      dirty |= SMART_BAND_DIRTY_PAGE;
    }

  if (before->time_valid != after->time_valid ||
      memcmp(before->time_text, after->time_text,
             sizeof(before->time_text)) != 0 ||
      memcmp(before->date_text, after->date_text,
             sizeof(before->date_text)) != 0)
    {
      dirty |= SMART_BAND_DIRTY_TIME;
    }

  if (metric_snapshot_changed(before, after,
                              SMART_BAND_METRIC_HEART_RATE))
    {
      dirty |= SMART_BAND_DIRTY_HEART;
    }

  if (metric_snapshot_changed(before, after, SMART_BAND_METRIC_STEPS) ||
      before->step_goal != after->step_goal)
    {
      dirty |= SMART_BAND_DIRTY_STEPS;
    }

  if (metric_snapshot_changed(before, after, SMART_BAND_METRIC_BATTERY) ||
      before->battery_charging != after->battery_charging)
    {
      dirty |= SMART_BAND_DIRTY_BATTERY;
    }

  if (metric_snapshot_changed(before, after,
                              SMART_BAND_METRIC_TEMPERATURE) ||
      metric_snapshot_changed(before, after, SMART_BAND_METRIC_HUMIDITY))
    {
      dirty |= SMART_BAND_DIRTY_ENVIRONMENT;
    }

  if (memcmp(before->status_text, after->status_text,
             sizeof(before->status_text)) != 0)
    {
      dirty |= SMART_BAND_DIRTY_STATUS;
    }

  return dirty;
}

static void detect_sensor_capabilities(smart_band_runtime_t *runtime)
{
#if defined(CONFIG_LVX_DEMO_SMART_BAND_USE_SENSORS)
  runtime->capabilities.heart_rate = runtime->sensors.hrate_fd >= 0;
  runtime->capabilities.step_counter = runtime->sensors.step_fd >= 0;
  runtime->capabilities.accelerometer = runtime->sensors.accel_fd >= 0;
  runtime->capabilities.temperature = runtime->sensors.temp_fd >= 0;
  runtime->capabilities.humidity = runtime->sensors.humi_fd >= 0;
  runtime->capabilities.battery = runtime->sensors.battery_fd >= 0;
  runtime->capabilities.charging = runtime->capabilities.battery;
#else
  (void)runtime;
#endif
}

static bool runtime_step_sample(const smart_band_runtime_t *runtime,
                                smart_band_step_sample_t *sample)
{
  const smart_band_metric_info_t *info = smart_band_state_metric_info(
    &runtime->model, SMART_BAND_METRIC_STEPS);
  bool fresh = info != NULL &&
               info->freshness == SMART_BAND_DATA_FRESHNESS_FRESH;

  if (sample == NULL)
    {
      return false;
    }

  if (info != NULL && info->source == SMART_BAND_DATA_SOURCE_SIMULATED)
    {
      memset(sample, 0, sizeof(*sample));
      sample->source = SMART_BAND_STEP_SOURCE_SIMULATION;
      sample->raw_counter = runtime->model.steps < 0 ? 0u :
                            (uint64_t)runtime->model.steps;
      sample->available = true;
      sample->fresh = fresh;
      sample->monotonic_ms = runtime->last_clock.elapsed_ms;
      return true;
    }

  return smart_band_sensor_bridge_step_sample(
    &runtime->sensors, runtime->last_clock.elapsed_ms, fresh, sample);
}

static bool runtime_heart_is_new(const smart_band_runtime_t *runtime,
                                 const smart_band_metric_info_t *info)
{
  return info != NULL &&
         info->freshness == SMART_BAND_DATA_FRESHNESS_FRESH &&
         (info->source == SMART_BAND_DATA_SOURCE_SIMULATED ||
          (info->monotonic_valid &&
           info->last_update_monotonic_ms ==
           runtime->last_clock.elapsed_ms));
}

static bool sample_runtime_clock(smart_band_runtime_t *runtime)
{
  if (!smart_band_clock_sample(&runtime->clock, &runtime->last_clock))
    {
      return false;
    }

  runtime->last_clock.wall_valid = runtime->last_clock.wall_valid &&
                                   runtime->capabilities.rtc;
  runtime->last_clock.wall_rollback = runtime->last_clock.wall_rollback &&
                                      runtime->capabilities.rtc;
  return true;
}

static void sync_notification_policy(smart_band_runtime_t *runtime)
{
  smart_band_notification_policy_t policy = runtime->notifications.policy;
  uint32_t generation_before = runtime->notifications.pending_generation;
  bool pending_before = runtime->notifications.presentation_pending;

  policy.workout_active =
    smart_band_workout_service_is_live(&runtime->workout);
  if (memcmp(&policy, &runtime->notifications.policy, sizeof(policy)) != 0)
    {
      smart_band_notification_service_set_policy(
        &runtime->notifications, &policy,
        runtime->last_clock.monotonic_ms);
      if (pending_before != runtime->notifications.presentation_pending ||
          generation_before != runtime->notifications.pending_generation)
        {
          runtime->dirty |= SMART_BAND_DIRTY_NOTIFICATION;
        }
    }
}

static void dispatch_workout_event(smart_band_runtime_t *runtime,
                                   const smart_band_event_t *event)
{
  smart_band_workout_command_t command =
    (smart_band_workout_command_t)event->payload.workout.command;

  if (command == SMART_BAND_WORKOUT_COMMAND_START)
    {
      runtime->last_workout_result = smart_band_workout_service_start(
        &runtime->workout,
        (smart_band_workout_mode_t)event->payload.workout.mode,
        runtime->last_clock.elapsed_ms, runtime->last_clock.wall_time,
        runtime->last_clock.wall_valid);
    }
  else
    {
      runtime->last_workout_result = smart_band_workout_service_command(
        &runtime->workout, command, runtime->last_clock.elapsed_ms,
        runtime->last_clock.wall_time, runtime->last_clock.wall_valid,
        runtime->last_clock.wall_rollback);
    }
  runtime->dirty |= SMART_BAND_DIRTY_WORKOUT | SMART_BAND_DIRTY_HISTORY;
}

static void dispatch_domain_events(smart_band_runtime_t *runtime)
{
  smart_band_event_t event;

  while (smart_band_event_queue_take_next_domain(&runtime->events, &event))
    {
      if (event.type == SMART_BAND_EVENT_WORKOUT_COMMAND)
        {
          dispatch_workout_event(runtime, &event);
        }
      else if (event.type == SMART_BAND_EVENT_WORKOUT_CHECKPOINT)
        {
          (void)smart_band_workout_service_checkpoint(
            &runtime->workout, runtime->last_clock.elapsed_ms);
        }
      else
        {
          sync_notification_policy(runtime);
          runtime->last_notification_result =
            smart_band_notification_service_process(&runtime->notifications,
                                                    &event);
          runtime->dirty |= SMART_BAND_DIRTY_NOTIFICATION;
        }
    }
  sync_notification_policy(runtime);
  if (smart_band_notification_service_tick(
        &runtime->notifications, runtime->last_clock.monotonic_ms))
    {
      runtime->dirty |= SMART_BAND_DIRTY_NOTIFICATION;
    }
}

void smart_band_runtime_dispatch_pending(smart_band_runtime_t *runtime)
{
  if (runtime != NULL && runtime->initialized)
    {
      (void)smart_band_runtime_drain_external(
        runtime, SMART_BAND_EVENT_QUEUE_CAPACITY);
      if (!sample_runtime_clock(runtime))
        {
          return;
        }

      dispatch_domain_events(runtime);
    }
}

static bool advance_runtime(smart_band_runtime_t *runtime,
                            bool active_app_visible)
{
  smart_band_view_snapshot_t before;
  smart_band_view_snapshot_t after;
  smart_band_step_sample_t step_sample;
  smart_band_workout_snapshot_t workout_before;
  smart_band_workout_snapshot_t workout_after;
  const smart_band_metric_info_t *heart_info;
  bool workout_changed;
  bool heart_valid;

  if (!sample_runtime_clock(runtime))
    {
      return false;
    }

  capture_view_snapshot(&runtime->model, &before);
  smart_band_state_tick(&runtime->model,
                        runtime->last_clock.wall_valid ?
                        runtime->last_clock.wall_time : 0);
  smart_band_state_set_wall_time(&runtime->model,
                                 runtime->last_clock.wall_time,
                                 runtime->last_clock.wall_valid);
  smart_band_sensor_bridge_update_clocked(
    &runtime->sensors, &runtime->model, runtime->last_clock.wall_time,
    runtime->last_clock.elapsed_ms, runtime->last_clock.wall_rollback);
  if (!runtime_step_sample(runtime, &step_sample))
    {
      return false;
    }
  heart_info = smart_band_state_metric_info(
    &runtime->model, SMART_BAND_METRIC_HEART_RATE);
  heart_valid = heart_info != NULL &&
                heart_info->freshness == SMART_BAND_DATA_FRESHNESS_FRESH &&
                runtime->model.heart_rate > 0 &&
                runtime->model.heart_rate <= UINT16_MAX;
  (void)smart_band_workout_service_snapshot(&runtime->workout,
                                            &workout_before);
  runtime->last_workout_result = smart_band_workout_service_tick(
    &runtime->workout, &step_sample, (uint16_t)runtime->model.heart_rate,
    heart_valid, runtime_heart_is_new(runtime, heart_info),
    runtime->last_clock.elapsed_ms, runtime->last_clock.wall_time,
    runtime->last_clock.wall_valid, runtime->last_clock.wall_rollback);
  (void)smart_band_workout_service_snapshot(&runtime->workout,
                                            &workout_after);
  workout_changed = memcmp(&workout_before, &workout_after,
                           sizeof(workout_before)) != 0;
  if (workout_changed)
    {
      runtime->dirty |= SMART_BAND_DIRTY_WORKOUT |
                        SMART_BAND_DIRTY_HISTORY;
    }
  dispatch_domain_events(runtime);
  capture_view_snapshot(&runtime->model, &after);
  runtime->dirty |= view_changes(&before, &after);

  if (smart_band_apps_tick_at(&runtime->apps, active_app_visible,
                              runtime->last_clock.monotonic_ms))
    {
      runtime->dirty |= SMART_BAND_DIRTY_APP;
    }

  return true;
}

int smart_band_runtime_init(
  smart_band_runtime_t *runtime,
  const smart_band_clock_source_t *clock_source,
  const smart_band_capabilities_t *capabilities)
{
  smart_band_platform_t platform;

  smart_band_platform_init_noop(&platform);
  return smart_band_runtime_init_with_platform(runtime, clock_source,
                                               capabilities, &platform);
}

int smart_band_runtime_init_with_platform(
  smart_band_runtime_t *runtime,
  const smart_band_clock_source_t *clock_source,
  const smart_band_capabilities_t *capabilities,
  const smart_band_platform_t *platform)
{
  bool detect_capabilities = capabilities == NULL;
  smart_band_platform_t default_platform;

  if (runtime == NULL)
    {
      return -1;
    }

  if (platform == NULL)
    {
      smart_band_platform_init_noop(&default_platform);
      platform = &default_platform;
    }

  memset(runtime, 0, sizeof(*runtime));
  runtime->platform = *platform;
  if (!smart_band_event_inbox_init(&runtime->external_events,
                                   &runtime->platform.event_lock) ||
      smart_band_clock_init(&runtime->clock, clock_source) != 0 ||
      !smart_band_clock_sample(&runtime->clock, &runtime->last_clock))
    {
      memset(runtime, 0, sizeof(*runtime));
      return -1;
    }

  if (smart_band_store_init(&runtime->storage,
                            &runtime->platform.storage) == 0)
    {
      runtime->storage_initialized = true;
    }

  if (capabilities == NULL)
    {
      smart_band_capabilities_init_base(&runtime->capabilities);
      runtime->capabilities.rtc = runtime->clock.source.wall_now != NULL;
    }
  else
    {
      runtime->capabilities = *capabilities;
    }

  smart_band_event_queue_init(&runtime->events);
  (void)smart_band_notification_service_init(&runtime->notifications);
  runtime->notifications_initialized = true;
  runtime->last_clock.wall_valid = runtime->last_clock.wall_valid &&
                                   runtime->capabilities.rtc;
  smart_band_state_init(&runtime->model,
                        runtime->last_clock.wall_valid ?
                        runtime->last_clock.wall_time : 0);
  smart_band_state_set_wall_time(&runtime->model,
                                 runtime->last_clock.wall_time,
                                 runtime->last_clock.wall_valid);
  smart_band_sensor_bridge_init(&runtime->sensors);
  runtime->sensors_initialized = true;
  if (detect_capabilities)
    {
      detect_sensor_capabilities(runtime);
    }

  if (smart_band_history_init(
        &runtime->history,
        runtime->storage_initialized ? &runtime->storage : NULL) != 0)
    {
      smart_band_sensor_bridge_deinit(&runtime->sensors);
      runtime->sensors_initialized = false;
      smart_band_notification_service_reset(&runtime->notifications);
      runtime->notifications_initialized = false;
      if (runtime->storage_initialized)
        {
          smart_band_store_deinit(&runtime->storage);
          runtime->storage_initialized = false;
        }
      (void)smart_band_event_inbox_close(&runtime->external_events);
      return -1;
    }
  runtime->history_initialized = true;
  if (smart_band_workout_service_init(
        &runtime->workout,
        runtime->storage_initialized ? &runtime->storage : NULL,
        &runtime->history, runtime->last_clock.elapsed_ms) != 0)
    {
      smart_band_history_reset(&runtime->history);
      runtime->history_initialized = false;
      smart_band_sensor_bridge_deinit(&runtime->sensors);
      runtime->sensors_initialized = false;
      smart_band_notification_service_reset(&runtime->notifications);
      runtime->notifications_initialized = false;
      if (runtime->storage_initialized)
        {
          smart_band_store_deinit(&runtime->storage);
          runtime->storage_initialized = false;
        }
      (void)smart_band_event_inbox_close(&runtime->external_events);
      return -1;
    }
  runtime->workout_initialized = true;

  if (detect_capabilities && runtime->storage_initialized &&
      runtime->workout.checkpoint_result != SMART_BAND_STORE_UNAVAILABLE)
    {
      runtime->capabilities.storage = true;
    }

  if (smart_band_apps_init(&runtime->apps) != 0)
    {
      smart_band_workout_service_reset(&runtime->workout);
      runtime->workout_initialized = false;
      smart_band_history_reset(&runtime->history);
      runtime->history_initialized = false;
      smart_band_sensor_bridge_deinit(&runtime->sensors);
      runtime->sensors_initialized = false;
      smart_band_notification_service_reset(&runtime->notifications);
      runtime->notifications_initialized = false;
      if (runtime->storage_initialized)
        {
          smart_band_store_deinit(&runtime->storage);
          runtime->storage_initialized = false;
        }
      (void)smart_band_event_inbox_close(&runtime->external_events);
      return -1;
    }

  runtime->dirty = SMART_BAND_DIRTY_ALL;
  runtime->initialized = true;
  return 0;
}

void smart_band_runtime_deinit(smart_band_runtime_t *runtime)
{
  if (runtime == NULL)
    {
      return;
    }

  if (runtime->initialized && runtime->platform.sync.ops != NULL &&
      runtime->platform.sync.ops->stop != NULL)
    {
      (void)runtime->platform.sync.ops->stop(runtime->platform.sync.context);
    }

  (void)smart_band_event_inbox_close(&runtime->external_events);
  if (runtime->initialized)
    {
      smart_band_apps_deinit(&runtime->apps);
    }

  if (runtime->sensors_initialized)
    {
      smart_band_sensor_bridge_deinit(&runtime->sensors);
    }

  if (runtime->workout_initialized)
    {
      if (runtime->initialized)
        {
          (void)sample_runtime_clock(runtime);
          (void)smart_band_workout_service_checkpoint(
            &runtime->workout, runtime->last_clock.elapsed_ms);
        }
      smart_band_workout_service_reset(&runtime->workout);
    }
  if (runtime->notifications_initialized)
    {
      smart_band_notification_service_reset(&runtime->notifications);
    }
  if (runtime->history_initialized)
    {
      smart_band_history_reset(&runtime->history);
    }

  if (runtime->storage_initialized)
    {
      smart_band_store_deinit(&runtime->storage);
    }

  memset(runtime, 0, sizeof(*runtime));
}

bool smart_band_runtime_post(smart_band_runtime_t *runtime,
                             const smart_band_event_t *event)
{
  return runtime != NULL && runtime->initialized &&
         smart_band_event_inbox_post_main(&runtime->external_events,
                                          &runtime->events, event);
}

bool smart_band_runtime_post_external(void *context,
                                      const smart_band_event_t *event)
{
  smart_band_runtime_t *runtime = context;

  return runtime != NULL && runtime->initialized &&
         smart_band_event_inbox_post(&runtime->external_events, event);
}

bool smart_band_runtime_post_notification(
  smart_band_runtime_t *runtime,
  const smart_band_notification_input_t *input, uint32_t monotonic_ms)
{
  smart_band_event_t event;

  return runtime != NULL && runtime->initialized &&
         smart_band_notification_event_received(input, monotonic_ms, &event) &&
         smart_band_runtime_post(runtime, &event);
}

bool smart_band_runtime_post_notification_external(
  smart_band_runtime_t *runtime,
  const smart_band_notification_utf8_input_t *input,
  uint32_t monotonic_ms)
{
  smart_band_event_t event;

  return runtime != NULL && runtime->initialized &&
         smart_band_notification_event_received_utf8(
           input, monotonic_ms, &event) &&
         smart_band_runtime_post_external(runtime, &event);
}

bool smart_band_runtime_inject_notification_demo(
  smart_band_runtime_t *runtime, uint32_t seed, uint32_t sequence,
  uint32_t monotonic_ms)
{
  smart_band_notification_t notification;
  smart_band_notification_input_t input;

  (void)smart_band_notification_demo_build(seed, sequence, &notification);

  input.id = notification.id;
  input.type = notification.type;
  input.priority = notification.priority;
  input.source = notification.source;
  input.title = notification.title;
  input.body = notification.body;
  input.wall_timestamp = notification.wall_timestamp;
  return smart_band_runtime_post_notification(runtime, &input, monotonic_ms);
}

bool smart_band_runtime_post_notification_action(
  smart_band_runtime_t *runtime, uint32_t notification_id,
  smart_band_notification_command_t command, uint32_t monotonic_ms)
{
  smart_band_event_t event;

  return runtime != NULL && runtime->initialized &&
         smart_band_notification_event_action(
           notification_id, command, monotonic_ms, &event) &&
         smart_band_runtime_post(runtime, &event);
}

bool smart_band_runtime_set_notification_policy(
  smart_band_runtime_t *runtime,
  const smart_band_notification_policy_t *policy)
{
  if (runtime == NULL || !runtime->initialized || policy == NULL)
    {
      return false;
    }

  {
    smart_band_notification_policy_t derived = *policy;

    derived.workout_active =
      smart_band_workout_service_is_live(&runtime->workout);
    smart_band_notification_service_set_policy(
      &runtime->notifications, &derived, runtime->last_clock.monotonic_ms);
  }
  runtime->dirty |= SMART_BAND_DIRTY_NOTIFICATION;
  return true;
}

size_t smart_band_runtime_drain_external(smart_band_runtime_t *runtime,
                                         size_t limit)
{
  smart_band_event_t event;
  size_t drained = 0;

  if (runtime == NULL || !runtime->initialized)
    {
      return 0;
    }

  while (drained < limit &&
         smart_band_event_inbox_pop(&runtime->external_events, &event))
    {
      (void)smart_band_event_queue_push(&runtime->events, &event);
      drained++;
    }

  return drained;
}

bool smart_band_runtime_tick(smart_band_runtime_t *runtime,
                             bool active_app_visible)
{
  if (runtime == NULL || !runtime->initialized)
    {
      return false;
    }

  (void)smart_band_runtime_drain_external(
    runtime, SMART_BAND_EVENT_QUEUE_CAPACITY);
  return advance_runtime(runtime, active_app_visible);
}

bool smart_band_runtime_refresh_sensors(smart_band_runtime_t *runtime)
{
  smart_band_view_snapshot_t before;
  smart_band_view_snapshot_t after;

  if (runtime == NULL || !runtime->initialized ||
      !sample_runtime_clock(runtime))
    {
      return false;
    }

  capture_view_snapshot(&runtime->model, &before);
  smart_band_sensor_bridge_update_clocked(
    &runtime->sensors, &runtime->model, runtime->last_clock.wall_time,
    runtime->last_clock.elapsed_ms, runtime->last_clock.wall_rollback);
  capture_view_snapshot(&runtime->model, &after);
  runtime->dirty |= view_changes(&before, &after);
  return true;
}

void smart_band_runtime_mark_dirty(smart_band_runtime_t *runtime,
                                   smart_band_dirty_flags_t flags)
{
  if (runtime != NULL)
    {
      runtime->dirty |= flags & SMART_BAND_DIRTY_ALL;
    }
}

smart_band_dirty_flags_t
smart_band_runtime_peek_dirty(const smart_band_runtime_t *runtime)
{
  return runtime == NULL ? SMART_BAND_DIRTY_NONE : runtime->dirty;
}

smart_band_dirty_flags_t smart_band_runtime_take_dirty(
  smart_band_runtime_t *runtime)
{
  smart_band_dirty_flags_t dirty;

  if (runtime == NULL)
    {
      return SMART_BAND_DIRTY_NONE;
    }

  dirty = runtime->dirty;
  runtime->dirty = SMART_BAND_DIRTY_NONE;
  return dirty;
}
