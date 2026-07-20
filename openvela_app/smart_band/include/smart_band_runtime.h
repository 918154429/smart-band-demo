#ifndef SMART_BAND_RUNTIME_H
#define SMART_BAND_RUNTIME_H

#include "sensor_bridge.h"
#include "smart_band_apps.h"
#include "smart_band_capabilities.h"
#include "smart_band_clock.h"
#include "smart_band_event.h"
#include "smart_band_platform.h"
#include "watch_model.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  smart_band_state_t model;
  smart_band_sensor_bridge_t sensors;
  smart_band_apps_runtime_t apps;
  smart_band_event_queue_t events;
  smart_band_event_inbox_t external_events;
  smart_band_clock_t clock;
  smart_band_clock_sample_t last_clock;
  smart_band_capabilities_t capabilities;
  smart_band_platform_t platform;
  uint32_t dirty;
  bool initialized;
  bool sensors_initialized;
} smart_band_runtime_t;

typedef uint32_t smart_band_dirty_flags_t;

#define SMART_BAND_DIRTY_NONE        0u
#define SMART_BAND_DIRTY_TIME        (1u << 0)
#define SMART_BAND_DIRTY_HEART       (1u << 1)
#define SMART_BAND_DIRTY_STEPS       (1u << 2)
#define SMART_BAND_DIRTY_BATTERY     (1u << 3)
#define SMART_BAND_DIRTY_ENVIRONMENT (1u << 4)
#define SMART_BAND_DIRTY_STATUS      (1u << 5)
#define SMART_BAND_DIRTY_APP         (1u << 6)
#define SMART_BAND_DIRTY_PAGE        (1u << 7)
#define SMART_BAND_DIRTY_ALL         ((1u << 8) - 1u)

int smart_band_runtime_init(
  smart_band_runtime_t *runtime,
  const smart_band_clock_source_t *clock_source,
  const smart_band_capabilities_t *capabilities);
int smart_band_runtime_init_with_platform(
  smart_band_runtime_t *runtime,
  const smart_band_clock_source_t *clock_source,
  const smart_band_capabilities_t *capabilities,
  const smart_band_platform_t *platform);
void smart_band_runtime_deinit(smart_band_runtime_t *runtime);
bool smart_band_runtime_post(smart_band_runtime_t *runtime,
                             const smart_band_event_t *event);
bool smart_band_runtime_post_external(void *context,
                                      const smart_band_event_t *event);
size_t smart_band_runtime_drain_external(smart_band_runtime_t *runtime,
                                         size_t limit);
bool smart_band_runtime_tick(smart_band_runtime_t *runtime,
                             bool active_app_visible);
bool smart_band_runtime_refresh_sensors(smart_band_runtime_t *runtime);
void smart_band_runtime_mark_dirty(smart_band_runtime_t *runtime,
                                   smart_band_dirty_flags_t flags);
smart_band_dirty_flags_t
smart_band_runtime_peek_dirty(const smart_band_runtime_t *runtime);
smart_band_dirty_flags_t smart_band_runtime_take_dirty(
  smart_band_runtime_t *runtime);

#ifdef __cplusplus
}
#endif

#endif
