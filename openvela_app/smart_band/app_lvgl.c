#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#  define _POSIX_C_SOURCE 200809L
#endif

#include "app_lvgl.h"

#include "icon_assets.h"
#include "smart_band_event_mutex.h"
#include "smart_band_runtime.h"
#include "smart_band_storage_backend.h"
#include "smart_band_watch_face.h"
#include "smart_band_watch_face_settings.h"
#include "ui/lvgl/components.h"
#include "ui/lvgl/watch_face_picker.h"
#include "ui/lvgl/history_view.h"
#include "ui/lvgl/notification_view.h"
#include "ui/lvgl/workout_view.h"
#include "ui/lvgl/watch_pages.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SMART_BAND_DEFAULT_TZ "CST-8"
#define SMART_BAND_SWIPE_CLICK_GUARD_MS 300
#define SMART_BAND_FACE_LONG_PRESS_MS 600

typedef enum
{
  SMART_BAND_SYSTEM_VIEW_NONE = 0,
  SMART_BAND_SYSTEM_VIEW_WORKOUT,
  SMART_BAND_SYSTEM_VIEW_HISTORY,
  SMART_BAND_SYSTEM_VIEW_NOTIFICATIONS
} smart_band_system_view_t;

typedef struct
{
  lv_obj_t *root;
  lv_obj_t *watch;
  lv_obj_t *screen;
  lv_obj_t *face_host;
  smart_band_ui_components_t components;
  smart_band_watch_face_instance_t watch_face;
  smart_band_watch_face_picker_t face_picker;
  smart_band_watch_pages_t watch_pages;

  lv_obj_t *dots_row;
  lv_obj_t *dots[SMART_BAND_PAGE_COUNT];

  lv_obj_t *apps_page;
  lv_obj_t *apps_date;
  lv_obj_t *apps_launcher;
  lv_obj_t *app_detail;
  lv_obj_t *app_title;
  lv_obj_t *app_content;
  lv_obj_t *app_back;
  smart_band_workout_view_t workout_view;
  smart_band_history_view_t history_view;
  smart_band_notification_view_t notification_view;
  smart_band_system_view_t system_view;

  lv_timer_t *runtime_timer;
  lv_timer_t *event_pump_timer;
  smart_band_event_mutex_t event_mutex;
  smart_band_runtime_t runtime;
  smart_band_storage_file_t storage_file;
  lv_coord_t screen_w;
  lv_coord_t screen_h;
  lv_point_t press_point;
  uint32_t press_tick;
  bool press_valid;
  bool page_swipe_consumed;
  uint32_t page_swipe_at;
  smart_band_watch_face_id_t selected_face;
  smart_band_store_result_t face_settings_result;
  bool compact_band;
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
  uint64_t diagnostic_last_elapsed_ms;
  uint64_t diagnostic_tick_gap_max_ms;
  uint32_t diagnostic_runtime_ticks;
  uint32_t diagnostic_event_pumps;
  uint32_t diagnostic_haptic_events;
  uint32_t diagnostic_wake_requests;
  uint32_t diagnostic_haptic_retries;
  uint32_t diagnostic_haptic_log_dropped;
  uint32_t diagnostic_wake_log_dropped;
  uint32_t diagnostic_last_haptic_notification_id;
  uint32_t diagnostic_last_haptic_generation;
  uint32_t diagnostic_last_wake_notification_id;
  uint32_t diagnostic_last_wake_generation;
  smart_band_notification_haptic_t diagnostic_last_haptic;
  smart_band_platform_result_t diagnostic_last_haptic_platform_result;
  lv_timer_t *diagnostic_q4_timer;
  smart_band_lvgl_effect_log_for_test_t effect_log_for_test;
  void *effect_log_context;
#endif
} smart_band_ui_t;

static smart_band_ui_t g_ui;

static void page_drag_cb(lv_event_t *event);
static void dot_click_cb(lv_event_t *event);
static void enable_touch_navigation(lv_obj_t *obj);
static void enable_touch_navigation_tree(lv_obj_t *obj);
static void set_page_visible(lv_obj_t *page, bool visible);
static void app_icon_cb(lv_event_t *event);
static void system_icon_cb(lv_event_t *event);
static void app_back_cb(lv_event_t *event);
static void step_goal_cb(lv_event_t *event);
static void render_page(void);
static void render_pending(void);
static void consume_notification_effects(void);
static void workout_action_cb(void *context,
                              smart_band_workout_view_action_t action);
static void notification_action_cb(
  void *context, uint32_t notification_id,
  smart_band_notification_command_t command);
static lv_obj_t *create_action_button(lv_obj_t *parent, const char *text,
                                      lv_coord_t x, lv_coord_t y,
                                      lv_coord_t w, lv_coord_t h,
                                      lv_color_t color, lv_event_cb_t cb,
                                      uintptr_t data);

#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
static size_t diagnostic_object_count(lv_obj_t *object)
{
  size_t count = object == NULL ? 0u : 1u;
  int32_t index = 0;
  lv_obj_t *child;

  if (object == NULL)
    {
      return 0u;
    }
  while ((child = lv_obj_get_child(object, index)) != NULL)
    {
      count += diagnostic_object_count(child);
      index++;
    }
  return count;
}

static void emit_q3_diagnostics(void)
{
  smart_band_workout_snapshot_t workout;
  uint64_t elapsed_ms = g_ui.runtime.last_clock.elapsed_ms;
  uint64_t gap_ms = elapsed_ms - g_ui.diagnostic_last_elapsed_ms;

  if (!smart_band_workout_service_snapshot(&g_ui.runtime.workout, &workout))
    {
      return;
    }
  if (g_ui.diagnostic_last_elapsed_ms != 0u &&
      gap_ms > g_ui.diagnostic_tick_gap_max_ms)
    {
      g_ui.diagnostic_tick_gap_max_ms = gap_ms;
    }
  g_ui.diagnostic_last_elapsed_ms = elapsed_ms;
  printf("smart_band:q3:v1 elapsed_ms=%llu page=%u view=%u state=%u "
         "mode=%u active_ms=%llu steps=%llu recovery=%u phase=%u "
         "checkpoint=%d daily=%u sessions=%u daily_store=%d "
         "session_store=%d queue=%u dropped=%u evicted=%u "
         "coalesced=%u inbox_dropped=%u objects=%u tick_gap_max_ms=%llu\n",
         (unsigned long long)elapsed_ms,
         (unsigned int)g_ui.runtime.model.page,
         (unsigned int)g_ui.system_view, (unsigned int)workout.state,
         (unsigned int)workout.mode,
         (unsigned long long)workout.active_duration_ms,
         (unsigned long long)workout.steps,
         g_ui.runtime.workout.recovery_pending ? 1u : 0u,
         (unsigned int)g_ui.runtime.workout.phase,
         (int)g_ui.runtime.workout.checkpoint_result,
         (unsigned int)g_ui.runtime.history.daily_count,
         (unsigned int)g_ui.runtime.history.session_count,
         (int)g_ui.runtime.history.last_daily_result,
         (int)g_ui.runtime.history.last_session_result,
         (unsigned int)smart_band_event_queue_count(&g_ui.runtime.events),
         g_ui.runtime.events.dropped, g_ui.runtime.events.evicted,
         g_ui.runtime.events.coalesced,
         g_ui.runtime.external_events.dropped,
         (unsigned int)diagnostic_object_count(g_ui.root),
         (unsigned long long)g_ui.diagnostic_tick_gap_max_ms);
  fflush(stdout);
}

static void emit_q4_diagnostics(void)
{
  smart_band_notification_presentation_t presentation;
  uint32_t notification_id = 0u;
  uint32_t generation = 0u;
  unsigned int presentation_kind = 0u;

  if (smart_band_notification_service_get_active_presentation(
        &g_ui.runtime.notifications, &notification_id, &generation,
        &presentation))
    {
      presentation_kind = presentation.full_screen ? 2u :
                          presentation.overlay ? 1u : 0u;
    }

  printf("smart_band:q4:v1 elapsed_ms=%llu notifications=%u dnd=%u "
         "active_id=%lu active_generation=%lu presentation=%u "
         "haptic_events=%lu wake_requests=%lu haptic_retries=%lu "
         "haptic_log_dropped=%lu wake_log_dropped=%lu "
         "pending_effects=%u inbox_dropped=%u\n",
         (unsigned long long)g_ui.runtime.last_clock.elapsed_ms,
         (unsigned int)smart_band_notification_count(
           &g_ui.runtime.notifications.model),
         g_ui.runtime.notifications.policy.dnd_enabled ? 1u : 0u,
         (unsigned long)notification_id, (unsigned long)generation,
         presentation_kind,
         (unsigned long)g_ui.diagnostic_haptic_events,
         (unsigned long)g_ui.diagnostic_wake_requests,
         (unsigned long)g_ui.diagnostic_haptic_retries,
         (unsigned long)g_ui.diagnostic_haptic_log_dropped,
         (unsigned long)g_ui.diagnostic_wake_log_dropped,
         (unsigned int)g_ui.runtime.notifications.effect_count,
         g_ui.runtime.external_events.dropped);
  fflush(stdout);
}

static void emit_q4_inject_marker(const char *scenario, const char *phase,
                                  size_t accepted, size_t requested)
{
  printf("smart_band:q4:inject:v1 scenario=%s phase=%s "
         "accepted=%u requested=%u\n",
         scenario, phase, (unsigned int)accepted, (unsigned int)requested);
  fflush(stdout);
}

static bool diagnostic_post_notification(
  uint32_t id, smart_band_notification_type_t type,
  smart_band_notification_priority_t priority, const char *source,
  const char *title, const char *body)
{
  smart_band_notification_utf8_input_t input;

  if (source == NULL || title == NULL || body == NULL)
    {
      return false;
    }

  memset(&input, 0, sizeof(input));
  input.id = id;
  input.type = type;
  input.priority = priority;
  input.source.data = source;
  input.source.length = strlen(source);
  input.title.data = title;
  input.title.length = strlen(title);
  input.body.data = body;
  input.body.length = strlen(body);
  input.wall_timestamp = UINT64_C(1700000000) + id;
  return smart_band_lvgl_post_notification_external(&input, lv_tick_get());
}

static void diagnostic_q4_timer_done(lv_timer_t *timer)
{
  if (g_ui.diagnostic_q4_timer == timer)
    {
      g_ui.diagnostic_q4_timer = NULL;
    }
  lv_timer_del(timer);
}

static void diagnostic_q4_ordinary_update_cb(lv_timer_t *timer)
{
  static const char long_utf8_body[] =
    "\xe9\x95\xbf\xe6\x96\x87\xe9\x80\x9a\xe7\x9f\xa5\xe6\xad\xa3\xe5\x9c\xa8"
    "\xe9\xaa\x8c\xe8\xaf\x81 UTF-8 \xe5\xae\x89\xe5\x85\xa8\xe6\x88\xaa\xe6\x96\xad"
    "\xef\xbc\x8c\xe5\xa4\x9a\xe5\xad\x97\xe8\x8a\x82\xe5\xad\x97\xe7\xac\xa6\xe5\xbf\x85"
    "\xe9\xa1\xbb\xe4\xbf\x9d\xe6\x8c\x81\xe5\xae\x8c\xe6\x95\xb4\xe3\x80\x82"
    "\xe9\x95\xbf\xe6\x96\x87\xe9\x80\x9a\xe7\x9f\xa5\xe6\xad\xa3\xe5\x9c\xa8"
    "\xe9\xaa\x8c\xe8\xaf\x81 UTF-8 \xe5\xae\x89\xe5\x85\xa8\xe6\x88\xaa\xe6\x96\xad"
    "\xef\xbc\x8c\xe5\xa4\x9a\xe5\xad\x97\xe8\x8a\x82\xe5\xad\x97\xe7\xac\xa6\xe5\xbf\x85"
    "\xe9\xa1\xbb\xe4\xbf\x9d\xe6\x8c\x81\xe5\xae\x8c\xe6\x95\xb4\xe3\x80\x82";
  bool accepted = diagnostic_post_notification(
    701u, SMART_BAND_NOTIFICATION_TYPE_SMS,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Messages",
    "Native message updated", long_utf8_body);

  emit_q4_inject_marker("ordinary", "updated", accepted ? 1u : 0u, 1u);
  diagnostic_q4_timer_done(timer);
}

static void diagnostic_q4_workout_poll_cb(lv_timer_t *timer)
{
  smart_band_workout_snapshot_t workout;
  bool accepted;

  if (!smart_band_workout_service_snapshot(&g_ui.runtime.workout, &workout) ||
      workout.state != SMART_BAND_WORKOUT_STATE_ACTIVE)
    {
      return;
    }

  accepted = diagnostic_post_notification(
    731u, SMART_BAND_NOTIFICATION_TYPE_CALL,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Phone", "Coach",
    "Workout call remains non-blocking");
  emit_q4_inject_marker("workout", "active", accepted ? 1u : 0u, 1u);
  diagnostic_q4_timer_done(timer);
}
#endif

static time_t runtime_wall_now(void *context)
{
  (void)context;
  return time(NULL);
}

static uint32_t runtime_monotonic_now(void *context)
{
  (void)context;
  return lv_tick_get();
}

static int runtime_init(const smart_band_clock_source_t *clock_source)
{
  smart_band_platform_t platform;
  int result;

  if (smart_band_event_mutex_init(&g_ui.event_mutex) != 0)
    {
      return -1;
    }

  smart_band_platform_init_noop(&platform);
#if defined(CONFIG_LVX_DEMO_SMART_BAND_STORAGE_PATH)
  if (CONFIG_LVX_DEMO_SMART_BAND_STORAGE_PATH[0] != '\0')
    {
      smart_band_platform_result_t result = smart_band_storage_file_init(
        &g_ui.storage_file, CONFIG_LVX_DEMO_SMART_BAND_STORAGE_PATH,
        &platform.storage);

      if (result != SMART_BAND_PLATFORM_OK)
        {
          fprintf(stderr, "smart_band: storage unavailable (%d)\n", result);
          smart_band_platform_init_noop(&platform);
        }
    }
#endif

  if (!smart_band_event_mutex_get_lock(&g_ui.event_mutex,
                                       &platform.event_lock))
    {
      smart_band_event_mutex_deinit(&g_ui.event_mutex);
      return -1;
    }

  result = smart_band_runtime_init_with_platform(
    &g_ui.runtime, clock_source, NULL, &platform);
  if (result != 0)
    {
      smart_band_event_mutex_deinit(&g_ui.event_mutex);
    }
  return result;
}

static const char *notification_haptic_name(
  smart_band_notification_haptic_t haptic)
{
  switch (haptic)
    {
      case SMART_BAND_NOTIFICATION_HAPTIC_SUBTLE:
        return "subtle";
      case SMART_BAND_NOTIFICATION_HAPTIC_NORMAL:
        return "normal";
      case SMART_BAND_NOTIFICATION_HAPTIC_URGENT:
        return "urgent";
      case SMART_BAND_NOTIFICATION_HAPTIC_NONE:
      default:
        return "none";
    }
}

static bool emit_effect_log(const char *line)
{
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
  if (g_ui.effect_log_for_test != NULL)
    {
      return g_ui.effect_log_for_test(g_ui.effect_log_context, line);
    }
#endif
  if (printf("%s\n", line) <= 0)
    {
      return false;
    }

  return fflush(stdout) == 0;
}

static bool emit_simulated_haptic(uint32_t notification_id,
                                  uint32_t generation,
                                  smart_band_notification_haptic_t haptic)
{
  char line[160];

  (void)snprintf(
    line, sizeof(line),
    "smart_band:q4:haptic:v1 notification_id=%lu generation=%lu "
    "pattern=%s simulated=1",
    (unsigned long)notification_id, (unsigned long)generation,
    notification_haptic_name(haptic));
  return emit_effect_log(line);
}

static bool emit_synthetic_wake(uint32_t notification_id,
                                uint32_t generation)
{
  char line[176];

  (void)snprintf(
    line, sizeof(line),
    "smart_band:q4:wake:v1 notification_id=%lu generation=%lu "
    "reason=notification synthetic=1 power_transition=0",
    (unsigned long)notification_id, (unsigned long)generation);
  return emit_effect_log(line);
}

static size_t notification_haptic_pulses(
  smart_band_notification_haptic_t haptic,
  smart_band_haptic_pulse_t pulses[3])
{
  memset(pulses, 0, sizeof(*pulses) * 3u);
  if (haptic == SMART_BAND_NOTIFICATION_HAPTIC_SUBTLE)
    {
      pulses[0].on_ms = 40u;
      pulses[0].strength = 35u;
      return 1u;
    }
  if (haptic == SMART_BAND_NOTIFICATION_HAPTIC_NORMAL)
    {
      pulses[0].on_ms = 70u;
      pulses[0].off_ms = 50u;
      pulses[0].strength = 60u;
      pulses[1].on_ms = 70u;
      pulses[1].strength = 60u;
      return 2u;
    }
  if (haptic == SMART_BAND_NOTIFICATION_HAPTIC_URGENT)
    {
      size_t index;

      for (index = 0u; index < 3u; index++)
        {
          pulses[index].on_ms = 120u;
          pulses[index].off_ms = index + 1u < 3u ? 70u : 0u;
          pulses[index].strength = 100u;
        }
      return 3u;
    }

  return 0u;
}

/* These consumers deliberately acknowledge only their own service-owned
 * effect generation. Visual acknowledgement remains in notification_view.
 * The wake marker is synthetic evidence only; Q5 owns power-state changes. */
static void consume_notification_effects(void)
{
  uint32_t notification_id;
  uint32_t generation;
  smart_band_notification_haptic_t haptic;
  smart_band_haptic_pulse_t pulses[3];
  smart_band_platform_result_t platform_result;
  size_t pulse_count;

  while (smart_band_notification_service_peek_haptic(
           &g_ui.runtime.notifications, &notification_id, &generation,
           &haptic))
    {
      pulse_count = notification_haptic_pulses(haptic, pulses);
      if (pulse_count == 0u || g_ui.runtime.platform.haptic.ops == NULL ||
          g_ui.runtime.platform.haptic.ops->play == NULL)
        {
          platform_result = SMART_BAND_PLATFORM_INVALID;
        }
      else
        {
          platform_result = g_ui.runtime.platform.haptic.ops->play(
            g_ui.runtime.platform.haptic.context, pulses, pulse_count);
        }
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
      g_ui.diagnostic_last_haptic_platform_result = platform_result;
#endif
      if (platform_result != SMART_BAND_PLATFORM_OK &&
          platform_result != SMART_BAND_PLATFORM_UNAVAILABLE)
        {
          /* BUSY, IO, INVALID and future adapter failures retain the exact
           * service generation for a later pump retry. */
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
          g_ui.diagnostic_haptic_retries++;
#endif
          break;
        }
      if (platform_result == SMART_BAND_PLATFORM_UNAVAILABLE &&
          !emit_simulated_haptic(notification_id, generation, haptic))
        {
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
          g_ui.diagnostic_haptic_log_dropped++;
#endif
        }
      if (!smart_band_notification_service_ack_haptic(
            &g_ui.runtime.notifications, notification_id, generation))
        {
          break;
        }
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
      g_ui.diagnostic_haptic_events++;
      g_ui.diagnostic_last_haptic_notification_id = notification_id;
      g_ui.diagnostic_last_haptic_generation = generation;
      g_ui.diagnostic_last_haptic = haptic;
#endif
    }

  while (smart_band_notification_service_peek_wake(
           &g_ui.runtime.notifications, &notification_id, &generation))
    {
      if (!emit_synthetic_wake(notification_id, generation))
        {
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
          g_ui.diagnostic_wake_log_dropped++;
#endif
        }
      if (!smart_band_notification_service_ack_wake(
            &g_ui.runtime.notifications, notification_id, generation))
        {
          break;
        }
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
      g_ui.diagnostic_wake_requests++;
      g_ui.diagnostic_last_wake_notification_id = notification_id;
      g_ui.diagnostic_last_wake_generation = generation;
#endif
    }
}

static const lv_font_t *font_12(void) { return smart_band_ui_font_12(); }
static const lv_font_t *font_14(void) { return smart_band_ui_font_14(); }
static const lv_font_t *font_16(void) { return smart_band_ui_font_16(); }
static const lv_font_t *font_20(void) { return smart_band_ui_font_20(); }
static const lv_font_t *font_32(void) { return smart_band_ui_font_32(); }
static const lv_font_t *font_time(void) { return smart_band_ui_font_time(); }
static lv_coord_t sx(int value) { return smart_band_ui_sx(&g_ui.components, value); }
static lv_coord_t sy(int value) { return smart_band_ui_sy(&g_ui.components, value); }
static lv_coord_t min_coord(lv_coord_t a, lv_coord_t b) { return smart_band_ui_min(a, b); }
static lv_coord_t max_coord(lv_coord_t a, lv_coord_t b) { return smart_band_ui_max(a, b); }
static lv_coord_t abs_coord(lv_coord_t value) { return smart_band_ui_abs(value); }
static void strip_obj(lv_obj_t *obj) { smart_band_ui_strip_obj(obj); }
static lv_obj_t *create_box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                            lv_coord_t w, lv_coord_t h, lv_color_t color,
                            lv_coord_t radius)
{
  return smart_band_ui_create_box(parent, x, y, w, h, color, radius);
}
static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, lv_color_t color,
                              lv_text_align_t align)
{
  return smart_band_ui_create_label(parent, text, font, color, align);
}
static void place_label(lv_obj_t *label, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h)
{
  smart_band_ui_place_label(label, x, y, w, h);
}
static void set_label_text(lv_obj_t *label, const char *text)
{
  smart_band_ui_set_label_text(label, text);
}
static void set_label_text_fmt_int(lv_obj_t *label, const char *fmt, int value)
{
  smart_band_ui_set_label_text_fmt_int(label, fmt, value);
}
static lv_obj_t *create_icon_image(lv_obj_t *parent,
                                   const lv_image_dsc_t *src,
                                   lv_coord_t x, lv_coord_t y,
                                   lv_coord_t size)
{
  return smart_band_ui_create_icon_image(parent, src, x, y, size);
}
static void configure_local_time(void)
{
#if defined(__NuttX__)
  setenv("TZ", SMART_BAND_DEFAULT_TZ, 1);
#endif

  tzset();
}

static void format_temperature(char *buffer, size_t size)
{
  smart_band_ui_format_temperature(&g_ui.runtime.model, buffer, size);
}
static void format_duration(char *buffer, size_t size, int seconds)
{
  smart_band_ui_format_duration(buffer, size, seconds);
}
static void format_watch_date(char *buffer, size_t size)
{
  smart_band_ui_format_watch_date(&g_ui.runtime.model, buffer, size);
}
static lv_obj_t *create_page(lv_obj_t *parent)
{
  lv_obj_t *page = lv_obj_create(parent);
  if (page == NULL) return NULL;
  strip_obj(page);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_size(page, g_ui.screen_w, g_ui.screen_h);
  lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
  return page;
}

static int create_face_page(void)
{
  smart_band_watch_face_config_t config;
  const smart_band_watch_face_descriptor_t *descriptor;

  g_ui.face_host = create_page(g_ui.screen);
  if (g_ui.face_host == NULL)
    {
      return -1;
    }

  config.screen_width = g_ui.screen_w;
  config.screen_height = g_ui.screen_h;
  config.compact_band = g_ui.compact_band;
  descriptor = smart_band_watch_face_registry_find(g_ui.selected_face);
  if (descriptor == NULL)
    {
      descriptor = smart_band_watch_face_registry_default();
      g_ui.selected_face = SMART_BAND_WATCH_FACE_LOTUS;
    }

  return smart_band_watch_face_mount(
    &g_ui.watch_face, descriptor, g_ui.face_host,
    &config);
}

static void current_face_config(smart_band_watch_face_config_t *config)
{
  if (config != NULL)
    {
      config->screen_width = g_ui.screen_w;
      config->screen_height = g_ui.screen_h;
      config->compact_band = g_ui.compact_band;
    }
}

static int switch_watch_face(smart_band_watch_face_id_t selected_face)
{
  const smart_band_watch_face_descriptor_t *next_descriptor;
  const smart_band_watch_face_descriptor_t *previous_descriptor;
  smart_band_watch_face_config_t config;

  next_descriptor = smart_band_watch_face_registry_find(selected_face);
  if (next_descriptor == NULL || g_ui.face_host == NULL)
    {
      return -1;
    }

  if (g_ui.watch_face.mounted &&
      g_ui.watch_face.descriptor == next_descriptor)
    {
      return 0;
    }

  previous_descriptor = g_ui.watch_face.descriptor;
  current_face_config(&config);
  smart_band_watch_face_unmount(&g_ui.watch_face);
  if (smart_band_watch_face_mount(&g_ui.watch_face, next_descriptor,
                                  g_ui.face_host, &config) != 0)
    {
      if (previous_descriptor != NULL)
        {
          (void)smart_band_watch_face_mount(
            &g_ui.watch_face, previous_descriptor, g_ui.face_host, &config);
          smart_band_watch_face_render(&g_ui.watch_face,
                                       &g_ui.runtime.model);
          smart_band_watch_face_set_visible(
            &g_ui.watch_face,
            g_ui.runtime.model.page == SMART_BAND_PAGE_FACE);
          enable_touch_navigation_tree(
            smart_band_watch_face_root(&g_ui.watch_face));
        }

      return -1;
    }

  g_ui.selected_face = selected_face;
  smart_band_watch_face_render(&g_ui.watch_face, &g_ui.runtime.model);
  smart_band_watch_face_set_visible(
    &g_ui.watch_face, g_ui.runtime.model.page == SMART_BAND_PAGE_FACE);
  enable_touch_navigation_tree(smart_band_watch_face_root(&g_ui.watch_face));

  g_ui.face_settings_result = SMART_BAND_STORE_UNAVAILABLE;
  if (g_ui.runtime.storage_initialized)
    {
      g_ui.face_settings_result = smart_band_watch_face_settings_commit(
        &g_ui.runtime.storage, selected_face, NULL);
    }

  fprintf(stderr, "smart_band: watch face selected id=%d name=%s storage=%d\n",
          (int)selected_face, next_descriptor->name,
          (int)g_ui.face_settings_result);

  return 0;
}

static void face_picker_apply(void *context,
                              smart_band_watch_face_id_t selected_face)
{
  (void)context;

  if (switch_watch_face(selected_face) != 0)
    {
      smart_band_watch_face_picker_set_status_message(
        &g_ui.face_picker, "Face unavailable");
      return;
    }

  smart_band_watch_face_picker_hide(&g_ui.face_picker);
}

static int create_face_picker(void)
{
  int result = smart_band_watch_face_picker_mount(
    &g_ui.face_picker, g_ui.screen, &g_ui.components, g_ui.selected_face,
    face_picker_apply, NULL);

  if (result == 0)
    {
      enable_touch_navigation_tree(
        smart_band_watch_face_picker_root(&g_ui.face_picker));
    }

  return result;
}

static int create_heart_page(void)
{
  return smart_band_watch_pages_build_heart(&g_ui.watch_pages, g_ui.screen,
                                            &g_ui.components);
}

static int create_steps_page(void)
{
  return smart_band_watch_pages_build_steps(&g_ui.watch_pages, g_ui.screen,
                                            &g_ui.components, step_goal_cb);
}

static lv_obj_t *create_plain_layer(lv_obj_t *parent, lv_coord_t x,
                                    lv_coord_t y, lv_coord_t w,
                                    lv_coord_t h)
{
  lv_obj_t *layer = lv_obj_create(parent);
  if (layer == NULL)
    {
      return NULL;
    }

  strip_obj(layer);
  lv_obj_set_pos(layer, x, y);
  lv_obj_set_size(layer, w, h);
  lv_obj_set_style_bg_opa(layer, LV_OPA_TRANSP, 0);
  return layer;
}

static lv_obj_t *create_action_button(lv_obj_t *parent, const char *text,
                                      lv_coord_t x, lv_coord_t y,
                                      lv_coord_t w, lv_coord_t h,
                                      lv_color_t color, lv_event_cb_t cb,
                                      uintptr_t data)
{
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_t *label;

  if (button == NULL)
    {
      return NULL;
    }

  strip_obj(button);
  lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, w, h);
  lv_obj_set_style_bg_color(button, color, 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(button, sx(16), 0);
  lv_obj_set_style_shadow_width(button, sx(8), 0);
  lv_obj_set_style_shadow_color(button, lv_color_hex(0x314856), 0);
  lv_obj_set_style_shadow_opa(button, LV_OPA_20, 0);
  lv_obj_set_style_shadow_offset_y(button, sy(4), 0);
  lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, (void *)data);

  label = create_label(button, text, font_14(), lv_color_hex(0xffffff),
                       LV_TEXT_ALIGN_CENTER);
  if (label == NULL)
    {
      return NULL;
    }

  lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(label, cb, LV_EVENT_CLICKED, (void *)data);
  place_label(label, sx(4), (h - sy(20)) / 2, w - sx(8), sy(22));
  return button;
}

static int create_app_stat(lv_obj_t *parent, int col, int row,
                           const char *title, lv_obj_t **value_out)
{
  lv_coord_t margin = sx(22);
  lv_coord_t gap = sx(10);
  lv_coord_t card_w = (g_ui.screen_w - margin * 2 - gap) / 2;
  lv_coord_t card_h = sy(66);
  lv_coord_t x = margin + col * (card_w + gap);
  lv_coord_t y = sy(150) + row * (card_h + sy(10));
  lv_obj_t *card;
  lv_obj_t *title_label;

  card = create_box(parent, x, y, card_w, card_h, lv_color_hex(0xffffff),
                    sx(18));
  if (card == NULL)
    {
      return -1;
    }

  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0xe6eeee), 0);
  title_label = create_label(card, title, font_12(), lv_color_hex(0x81939a),
                             LV_TEXT_ALIGN_LEFT);
  *value_out = create_label(card, "--", font_16(), lv_color_hex(0x293b53),
                            LV_TEXT_ALIGN_LEFT);
  if (title_label == NULL || *value_out == NULL)
    {
      return -1;
    }

  place_label(title_label, sx(12), sy(10), card_w - sx(24), sy(18));
  place_label(*value_out, sx(12), sy(34), card_w - sx(24), sy(24));
  return 0;
}

static smart_band_app_host_t make_app_host(void)
{
  smart_band_app_host_t host;

  memset(&host, 0, sizeof(host));
  host.screen_w = g_ui.screen_w;
  host.screen_h = g_ui.screen_h;
  host.model = &g_ui.runtime.model;
  host.monotonic_now = runtime_monotonic_now;
  host.clock_context = g_ui.runtime.clock.source.context;
  host.sx = sx;
  host.sy = sy;
  host.font_12 = font_12;
  host.font_14 = font_14;
  host.font_16 = font_16;
  host.font_20 = font_20;
  host.font_32 = font_32;
  host.font_time = font_time;
  host.create_box = create_box;
  host.create_label = create_label;
  host.create_action_button = create_action_button;
  host.create_app_stat = create_app_stat;
  host.place_label = place_label;
  host.set_label_text = set_label_text;
  host.set_label_text_fmt_int = set_label_text_fmt_int;
  host.format_temperature = format_temperature;
  host.format_duration = format_duration;
  return host;
}

static void update_active_app(void)
{
  smart_band_app_host_t host = make_app_host();

  smart_band_app_render(&g_ui.runtime.apps, &host);
}

static void build_history_view_state(smart_band_history_view_state_t *state)
{
  static const char *const weekdays[] =
    {"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed"};
  int32_t latest_day = 0;
  size_t index;

  memset(state, 0, sizeof(*state));
  if (!(g_ui.runtime.last_clock.wall_valid &&
        smart_band_history_day_key(g_ui.runtime.last_clock.wall_time,
                                   &latest_day)) &&
      g_ui.runtime.history.daily_count != 0u)
    {
      latest_day = g_ui.runtime.history.daily[
        g_ui.runtime.history.daily_count - 1u].day_key;
    }
  if (g_ui.runtime.history.daily_count != 0u &&
      g_ui.runtime.history.daily[
        g_ui.runtime.history.daily_count - 1u].day_key > latest_day)
    {
      latest_day = g_ui.runtime.history.daily[
        g_ui.runtime.history.daily_count - 1u].day_key;
    }

  for (index = 0; index < SMART_BAND_HISTORY_VIEW_DAY_COUNT; index++)
    {
      smart_band_history_view_day_t *day = &state->days[index];
      int32_t key = latest_day -
                    (int32_t)(SMART_BAND_HISTORY_VIEW_DAY_COUNT - 1u - index);
      int32_t weekday = key % 7;
      size_t history_index;

      if (weekday < 0)
        {
          weekday += 7;
        }
      day->summary.day_key = key;
      (void)snprintf(day->label, sizeof(day->label), "%s",
                     weekdays[weekday]);
      for (history_index = 0;
           history_index < g_ui.runtime.history.daily_count;
           history_index++)
        {
          if (g_ui.runtime.history.daily[history_index].day_key == key)
            {
              day->summary = g_ui.runtime.history.daily[history_index];
              day->present = true;
              break;
            }
        }
    }

  if (g_ui.runtime.history.session_count != 0u)
    {
      state->latest_session = g_ui.runtime.history.sessions[
        g_ui.runtime.history.session_count - 1u];
      state->latest_session_present = true;
    }
}

static void render_system_view(void)
{
  if (g_ui.system_view == SMART_BAND_SYSTEM_VIEW_WORKOUT &&
      g_ui.workout_view.mounted)
    {
      smart_band_workout_view_state_t state;

      memset(&state, 0, sizeof(state));
      (void)smart_band_workout_service_snapshot(&g_ui.runtime.workout,
                                                &state.snapshot);
      state.countdown_duration_ms = 3000u;
      state.pause_count = g_ui.runtime.workout.pause_count;
      smart_band_workout_view_render(&g_ui.workout_view, &state);
    }
  else if (g_ui.system_view == SMART_BAND_SYSTEM_VIEW_HISTORY &&
           g_ui.history_view.mounted)
    {
      smart_band_history_view_state_t state;

      build_history_view_state(&state);
      smart_band_history_view_render(&g_ui.history_view, &state);
    }
  else if (g_ui.system_view == SMART_BAND_SYSTEM_VIEW_NOTIFICATIONS &&
           g_ui.notification_view.center_mounted)
    {
      smart_band_notification_view_render_center(
        &g_ui.notification_view, &g_ui.runtime.notifications.model);
    }
}

static void close_system_view(void)
{
  smart_band_workout_view_unmount(&g_ui.workout_view);
  smart_band_history_view_unmount(&g_ui.history_view);
  smart_band_notification_view_unmount_center(&g_ui.notification_view);
  g_ui.system_view = SMART_BAND_SYSTEM_VIEW_NONE;
  set_page_visible(g_ui.dots_row, true);
  set_label_text(g_ui.app_title, "Apps");
  set_page_visible(g_ui.app_detail, false);
  set_page_visible(g_ui.apps_launcher, true);
}

static void open_system_view(smart_band_system_view_t system_view)
{
  int result = -1;

  smart_band_app_unmount(&g_ui.runtime.apps);
  smart_band_workout_view_unmount(&g_ui.workout_view);
  smart_band_history_view_unmount(&g_ui.history_view);
  smart_band_notification_view_unmount_center(&g_ui.notification_view);
  lv_obj_clean(g_ui.app_content);
  g_ui.system_view = system_view;
  set_page_visible(g_ui.dots_row, false);
  if (system_view == SMART_BAND_SYSTEM_VIEW_WORKOUT)
    {
      set_label_text(g_ui.app_title, "Workout");
      result = smart_band_workout_view_mount(
        &g_ui.workout_view, g_ui.app_content, &g_ui.components,
        workout_action_cb, NULL);
    }
  else if (system_view == SMART_BAND_SYSTEM_VIEW_HISTORY)
    {
      set_label_text(g_ui.app_title, "History");
      result = smart_band_history_view_mount(
        &g_ui.history_view, g_ui.app_content, &g_ui.components);
    }
  else if (system_view == SMART_BAND_SYSTEM_VIEW_NOTIFICATIONS)
    {
      set_label_text(g_ui.app_title, "Notifications");
      result = smart_band_notification_view_mount_center(
        &g_ui.notification_view, g_ui.app_content);
    }

  if (result != 0)
    {
      smart_band_workout_view_unmount(&g_ui.workout_view);
      smart_band_history_view_unmount(&g_ui.history_view);
      smart_band_notification_view_unmount_center(&g_ui.notification_view);
      g_ui.system_view = SMART_BAND_SYSTEM_VIEW_NONE;
      set_page_visible(g_ui.dots_row, true);
      set_label_text(g_ui.app_title, "View failed");
    }

  set_page_visible(g_ui.apps_launcher, false);
  set_page_visible(g_ui.app_detail, true);
  render_system_view();
}

static void open_app(smart_band_app_id_t id)
{
  const smart_band_app_def_t *def = smart_band_app_find(id);
  smart_band_app_host_t host;

  if (def == NULL || g_ui.app_content == NULL)
    {
      return;
    }

  host = make_app_host();
  smart_band_workout_view_unmount(&g_ui.workout_view);
  smart_band_history_view_unmount(&g_ui.history_view);
  smart_band_notification_view_unmount_center(&g_ui.notification_view);
  g_ui.system_view = SMART_BAND_SYSTEM_VIEW_NONE;
  smart_band_app_unmount(&g_ui.runtime.apps);
  lv_obj_clean(g_ui.app_content);
  set_label_text(g_ui.app_title, def->title);

  if (smart_band_app_mount(&g_ui.runtime.apps, id, g_ui.app_content,
                           &host) != 0)
    {
      lv_obj_clean(g_ui.app_content);
      set_label_text(g_ui.app_title, "App failed");
    }

  set_page_visible(g_ui.apps_launcher, false);
  set_page_visible(g_ui.app_detail, true);
}

static int create_system_launcher_card(lv_obj_t *parent, const char *title_text,
                                       const lv_image_dsc_t *icon_source,
                                       smart_band_system_view_t system_view,
                                       lv_coord_t x, lv_coord_t y,
                                       lv_coord_t w, lv_coord_t h)
{
  lv_obj_t *card = lv_btn_create(parent);
  lv_obj_t *icon;
  lv_obj_t *title;
  lv_coord_t icon_size = min_coord(sx(42), h - sy(8));
  lv_coord_t title_x = sx(8) + icon_size + sx(4);

  if (card == NULL)
    {
      return -1;
    }

  strip_obj(card);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  lv_obj_set_style_bg_color(card, lv_color_hex(0xe9f7f4), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, sx(16), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0xbfe2dc), 0);
  lv_obj_add_event_cb(card, system_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)system_view);

  icon = create_icon_image(card, icon_source, sx(8), sy(4), icon_size);
  title = create_label(card, title_text, font_12(), lv_color_hex(0x234a50),
                       LV_TEXT_ALIGN_CENTER);
  if (icon == NULL || title == NULL)
    {
      return -1;
    }

  lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(icon, system_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)system_view);
  lv_obj_add_flag(title, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(title, system_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)system_view);
  place_label(title, title_x, (h - sy(24)) / 2,
              max_coord(w - title_x - sx(4), 1), sy(24));
  return 0;
}

static int create_launcher_card(lv_obj_t *parent,
                                const smart_band_app_def_t *def,
                                lv_coord_t x, lv_coord_t y,
                                lv_coord_t w, lv_coord_t h)
{
  lv_obj_t *card = lv_btn_create(parent);
  lv_obj_t *icon;
  lv_obj_t *title;
  lv_coord_t icon_size = min_coord(sx(42), h - sy(8));
  lv_coord_t title_x = sx(8) + icon_size + sx(4);

  if (card == NULL || def == NULL)
    {
      return -1;
    }

  strip_obj(card);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  lv_obj_set_style_bg_color(card, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, sx(16), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0xe6eeee), 0);
  lv_obj_set_style_shadow_width(card, sx(5), 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x314856), 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
  lv_obj_set_style_shadow_offset_y(card, sy(2), 0);
  lv_obj_add_event_cb(card, app_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)def->id);

  icon = create_icon_image(card, def->icon, sx(8), sy(4), icon_size);
  if (icon == NULL)
    {
      return -1;
    }

  lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(icon, app_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)def->id);

  title = create_label(card, def->title, font_12(), lv_color_hex(0x293b53),
                       LV_TEXT_ALIGN_CENTER);
  if (title == NULL)
    {
      return -1;
    }

  lv_obj_add_flag(title, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(title, app_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)def->id);
  place_label(title, title_x, (h - sy(24)) / 2,
              max_coord(w - title_x - sx(4), 1), sy(24));
  return 0;
}

static int create_apps_page(void)
{
  size_t app_count;
  const smart_band_app_def_t *apps = smart_band_apps_catalog(&app_count);
  lv_obj_t *title;
  lv_coord_t margin = sx(18);
  lv_coord_t gap_x = sx(10);
  lv_coord_t gap_y = sy(6);
  lv_coord_t card_w = (g_ui.screen_w - margin * 2 - gap_x) / 2;
  lv_coord_t card_h = sy(52);
  lv_coord_t grid_y = sy(126);
  lv_coord_t content_y = sy(94);

  g_ui.apps_page = create_page(g_ui.screen);
  if (g_ui.apps_page == NULL ||
      smart_band_watch_page_build_header(g_ui.apps_page, &g_ui.components,
                                         &g_ui.apps_date, sy(22), sy(64)) != 0)
    {
      return -1;
    }

  title = create_label(g_ui.apps_page, "Apps", font_20(),
                       lv_color_hex(0x293b53), LV_TEXT_ALIGN_CENTER);
  if (title == NULL)
    {
      return -1;
    }

  place_label(title, sx(22), sy(94), g_ui.screen_w - sx(44), sy(28));

  g_ui.apps_launcher = create_plain_layer(g_ui.apps_page, 0, 0,
                                          g_ui.screen_w, g_ui.screen_h);
  g_ui.app_detail = create_plain_layer(g_ui.apps_page, 0, 0,
                                       g_ui.screen_w, g_ui.screen_h);
  if (g_ui.apps_launcher == NULL || g_ui.app_detail == NULL)
    {
      return -1;
    }

  lv_obj_set_style_bg_color(g_ui.app_detail, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_grad_color(g_ui.app_detail, lv_color_hex(0xfffcf6), 0);
  lv_obj_set_style_bg_grad_dir(g_ui.app_detail, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(g_ui.app_detail, LV_OPA_COVER, 0);

  g_ui.app_back = create_action_button(g_ui.app_detail, "<", sx(20), sy(28),
                                       sx(44), sy(38),
                                       lv_color_hex(0x6f8790), app_back_cb,
                                       0);
  g_ui.app_title = create_label(g_ui.app_detail, "Apps", font_20(),
                                lv_color_hex(0x293b53),
                                LV_TEXT_ALIGN_CENTER);
  g_ui.app_content = create_plain_layer(
    g_ui.app_detail, 0, content_y, g_ui.screen_w,
    max_coord(g_ui.screen_h - content_y, 1));
  if (g_ui.app_back == NULL || g_ui.app_title == NULL ||
      g_ui.app_content == NULL)
    {
      return -1;
    }

  if (create_system_launcher_card(
        g_ui.apps_launcher, "Workout", &smart_band_icon_steps,
        SMART_BAND_SYSTEM_VIEW_WORKOUT, margin, grid_y, card_w, card_h) != 0 ||
      create_system_launcher_card(
        g_ui.apps_launcher, "History", &smart_band_icon_stopwatch,
        SMART_BAND_SYSTEM_VIEW_HISTORY, margin + card_w + gap_x,
        grid_y, card_w, card_h) != 0 ||
      create_system_launcher_card(
        g_ui.apps_launcher, "Notifications", &smart_band_icon_heart,
        SMART_BAND_SYSTEM_VIEW_NOTIFICATIONS, margin,
        grid_y + card_h + gap_y, card_w, card_h) != 0)
    {
      return -1;
    }

  for (size_t i = 0; i < app_count; i++)
    {
      size_t position = i + 3u;
      lv_coord_t row = (lv_coord_t)(position / 2u);
      lv_coord_t col = (lv_coord_t)(position % 2u);

      if (create_launcher_card(g_ui.apps_launcher, &apps[i],
                               margin + col * (card_w + gap_x),
                               grid_y + row * (card_h + gap_y),
                               card_w, card_h) != 0)
        {
          return -1;
        }
    }

  place_label(g_ui.app_title, sx(72), sy(34), g_ui.screen_w - sx(144),
              sy(30));
  set_page_visible(g_ui.app_detail, false);
  return 0;
}

static int create_dots(void)
{
  lv_coord_t dot = sx(12);
  lv_coord_t gap = sx(10);
  lv_coord_t row_w = dot * SMART_BAND_PAGE_COUNT +
                     gap * (SMART_BAND_PAGE_COUNT - 1);
  g_ui.dots_row = lv_obj_create(g_ui.screen);

  if (g_ui.dots_row == NULL)
    {
      return -1;
    }

  strip_obj(g_ui.dots_row);
  lv_obj_set_size(g_ui.dots_row, row_w, sy(20));
  lv_obj_align(g_ui.dots_row, LV_ALIGN_BOTTOM_MID, 0, -sy(22));
  lv_obj_set_style_bg_opa(g_ui.dots_row, LV_OPA_TRANSP, 0);

  for (int i = 0; i < SMART_BAND_PAGE_COUNT; i++)
    {
      g_ui.dots[i] = create_box(g_ui.dots_row, i * (dot + gap), sy(4), dot,
                                dot, lv_color_hex(0xc2d3d1),
                                LV_RADIUS_CIRCLE);
      if (g_ui.dots[i] == NULL)
        {
          return -1;
        }

      lv_obj_add_flag(g_ui.dots[i], LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(g_ui.dots[i], dot_click_cb, LV_EVENT_CLICKED,
                          (void *)(uintptr_t)i);
    }

  return 0;
}

static int create_background_waves(void)
{
  lv_obj_t *wave_one;
  lv_obj_t *wave_two;

  wave_one = create_box(g_ui.screen, -sx(40), g_ui.screen_h - sy(104),
                        g_ui.screen_w + sx(80), sy(156),
                        lv_color_hex(0xe5f7f4), LV_RADIUS_CIRCLE);
  wave_two = create_box(g_ui.screen, sx(84), g_ui.screen_h - sy(82),
                        g_ui.screen_w, sy(124), lv_color_hex(0xfff4dd),
                        LV_RADIUS_CIRCLE);
  if (wave_one == NULL || wave_two == NULL)
    {
      return -1;
    }

  lv_obj_set_style_bg_opa(wave_one, LV_OPA_50, 0);
  lv_obj_set_style_bg_opa(wave_two, LV_OPA_40, 0);
  return 0;
}

static int create_ui_tree(lv_obj_t *root)
{
  lv_coord_t root_w;
  lv_coord_t root_h;
  lv_coord_t watch_w;
  lv_coord_t watch_h;
  lv_coord_t frame_radius;
  bool compact_band;

  lv_obj_update_layout(root);
  root_w = lv_obj_get_width(root);
  root_h = lv_obj_get_height(root);

  if (root_w <= 0)
    {
      root_w = 320;
    }

  if (root_h <= 0)
    {
      root_h = 480;
    }

  compact_band = root_w <= 540 && root_h <= 540;
  g_ui.compact_band = compact_band;
  watch_h = min_coord(root_h - 48, 720);
  watch_h = max_coord(watch_h, 360);
  watch_w = (watch_h * 194) / 368;

  if (watch_w > root_w - 48)
    {
      watch_w = root_w - 48;
      watch_h = (watch_w * 368) / 194;
    }

  frame_radius = max_coord((watch_w * 54) / 330, 34);

  lv_obj_clean(root);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(root, lv_color_hex(0xeef4f3), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(root, 0, 0);

  if (compact_band)
    {
      g_ui.watch = root;
      g_ui.screen = root;
      g_ui.screen_w = root_w;
      g_ui.screen_h = root_h;
      smart_band_ui_components_init(&g_ui.components, g_ui.screen_w,
                                    g_ui.screen_h);
      lv_obj_set_style_bg_color(root, lv_color_hex(0xffffff), 0);
      lv_obj_set_style_bg_grad_color(root, lv_color_hex(0xfffcf6), 0);
      lv_obj_set_style_bg_grad_dir(root, LV_GRAD_DIR_VER, 0);

      if (create_background_waves() != 0 || create_face_page() != 0 ||
          create_heart_page() != 0 || create_steps_page() != 0 ||
          create_apps_page() != 0 || create_dots() != 0 ||
          create_face_picker() != 0)
        {
          return -1;
        }

      enable_touch_navigation(g_ui.screen);
      enable_touch_navigation_tree(
        smart_band_watch_face_root(&g_ui.watch_face));
      enable_touch_navigation_tree(g_ui.watch_pages.heart_page);
      enable_touch_navigation_tree(g_ui.watch_pages.steps_page);
      enable_touch_navigation_tree(g_ui.apps_launcher);
      return 0;
    }

  g_ui.watch = lv_obj_create(root);
  if (g_ui.watch == NULL)
    {
      return -1;
    }

  strip_obj(g_ui.watch);
  lv_obj_set_size(g_ui.watch, watch_w, watch_h);
  lv_obj_center(g_ui.watch);
  lv_obj_set_style_bg_color(g_ui.watch, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_grad_color(g_ui.watch, lv_color_hex(0xf9fbf8), 0);
  lv_obj_set_style_bg_grad_dir(g_ui.watch, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(g_ui.watch, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_ui.watch, frame_radius, 0);
  lv_obj_set_style_border_width(g_ui.watch, 1, 0);
  lv_obj_set_style_border_color(g_ui.watch, lv_color_hex(0xb7c2c7), 0);
  lv_obj_set_style_shadow_width(g_ui.watch, 32, 0);
  lv_obj_set_style_shadow_color(g_ui.watch, lv_color_hex(0x1c3040), 0);
  lv_obj_set_style_shadow_opa(g_ui.watch, LV_OPA_30, 0);
  lv_obj_set_style_shadow_offset_y(g_ui.watch, 18, 0);

  g_ui.screen = lv_obj_create(g_ui.watch);
  if (g_ui.screen == NULL)
    {
      return -1;
    }

  strip_obj(g_ui.screen);
  g_ui.screen_w = watch_w - 6;
  g_ui.screen_h = watch_h - 6;
  smart_band_ui_components_init(&g_ui.components, g_ui.screen_w,
                                g_ui.screen_h);
  lv_obj_set_size(g_ui.screen, g_ui.screen_w, g_ui.screen_h);
  lv_obj_center(g_ui.screen);
  lv_obj_set_style_bg_color(g_ui.screen, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_grad_color(g_ui.screen, lv_color_hex(0xfffcf6), 0);
  lv_obj_set_style_bg_grad_dir(g_ui.screen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(g_ui.screen, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_ui.screen, frame_radius - 2, 0);
  lv_obj_set_style_clip_corner(g_ui.screen, true, 0);
  lv_obj_add_flag(g_ui.screen, LV_OBJ_FLAG_CLICKABLE);

  if (create_background_waves() != 0 || create_face_page() != 0 ||
      create_heart_page() != 0 || create_steps_page() != 0 ||
      create_apps_page() != 0 || create_dots() != 0 ||
      create_face_picker() != 0)
    {
      return -1;
    }

  enable_touch_navigation(g_ui.screen);
  enable_touch_navigation_tree(smart_band_watch_face_root(&g_ui.watch_face));
  enable_touch_navigation_tree(g_ui.watch_pages.heart_page);
  enable_touch_navigation_tree(g_ui.watch_pages.steps_page);
  enable_touch_navigation_tree(g_ui.apps_launcher);
  return 0;
}

static void update_dots(void)
{
  for (int i = 0; i < SMART_BAND_PAGE_COUNT; i++)
    {
      lv_color_t color = i == (int)g_ui.runtime.model.page ?
                         lv_color_hex(0x79c5be) : lv_color_hex(0xc2d3d1);
      lv_obj_set_style_bg_color(g_ui.dots[i], color, 0);
      lv_obj_set_size(g_ui.dots[i], sx(12), sx(12));
    }
}

static void set_page_visible(lv_obj_t *page, bool visible)
{
  if (page == NULL)
    {
      return;
    }

  if (visible)
    {
      lv_obj_clear_flag(page, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_page_visibility(void)
{
  smart_band_watch_face_set_visible(
    &g_ui.watch_face, g_ui.runtime.model.page == SMART_BAND_PAGE_FACE);
  set_page_visible(g_ui.watch_pages.heart_page,
                   g_ui.runtime.model.page == SMART_BAND_PAGE_HEART);
  set_page_visible(g_ui.watch_pages.steps_page,
                   g_ui.runtime.model.page == SMART_BAND_PAGE_STEPS);
  set_page_visible(g_ui.apps_page,
                   g_ui.runtime.model.page == SMART_BAND_PAGE_APPS);

  update_dots();
}

static void switch_to_page(smart_band_page_t page)
{
  if (page >= SMART_BAND_PAGE_COUNT)
    {
      return;
    }

  g_ui.runtime.model.page = page;
  smart_band_runtime_mark_dirty(&g_ui.runtime, SMART_BAND_DIRTY_PAGE);
  render_pending();
}

static void update_face(void)
{
  smart_band_watch_face_render(&g_ui.watch_face, &g_ui.runtime.model);
}

static void update_heart_detail(void)
{
  smart_band_watch_pages_render_heart(&g_ui.watch_pages,
                                      &g_ui.runtime.model);
}

static void update_steps_detail(void)
{
  smart_band_watch_pages_render_steps(&g_ui.watch_pages,
                                      &g_ui.runtime.model);
}

static void update_apps_page(void)
{
  char date_text[20];

  format_watch_date(date_text, sizeof(date_text));
  set_label_text(g_ui.apps_date, date_text);
  if (g_ui.system_view == SMART_BAND_SYSTEM_VIEW_NONE)
    {
      update_active_app();
    }
  else
    {
      render_system_view();
    }
}

static void render_page(void)
{
  switch (g_ui.runtime.model.page)
    {
      case SMART_BAND_PAGE_FACE:
        update_face();
        break;
      case SMART_BAND_PAGE_HEART:
        update_heart_detail();
        break;
      case SMART_BAND_PAGE_STEPS:
        update_steps_detail();
        break;
      case SMART_BAND_PAGE_APPS:
        update_apps_page();
        break;
      default:
        break;
    }

  update_page_visibility();
}

static smart_band_dirty_flags_t current_page_dirty_mask(void)
{
  switch (g_ui.runtime.model.page)
    {
      case SMART_BAND_PAGE_FACE:
        return SMART_BAND_DIRTY_TIME | SMART_BAND_DIRTY_HEART |
               SMART_BAND_DIRTY_STEPS | SMART_BAND_DIRTY_BATTERY |
               SMART_BAND_DIRTY_ENVIRONMENT | SMART_BAND_DIRTY_STATUS |
               SMART_BAND_DIRTY_PAGE;
      case SMART_BAND_PAGE_HEART:
        return SMART_BAND_DIRTY_HEART | SMART_BAND_DIRTY_PAGE;
      case SMART_BAND_PAGE_STEPS:
        return SMART_BAND_DIRTY_STEPS | SMART_BAND_DIRTY_PAGE;
      case SMART_BAND_PAGE_APPS:
        return SMART_BAND_DIRTY_TIME | SMART_BAND_DIRTY_APP |
               SMART_BAND_DIRTY_WORKOUT | SMART_BAND_DIRTY_HISTORY |
               SMART_BAND_DIRTY_PAGE;
      default:
        return SMART_BAND_DIRTY_PAGE;
    }
}

static void render_pending(void)
{
  smart_band_dirty_flags_t dirty =
    smart_band_runtime_take_dirty(&g_ui.runtime);

  if ((dirty & current_page_dirty_mask()) != 0)
    {
      render_page();
    }

  if ((dirty & SMART_BAND_DIRTY_NOTIFICATION) != 0u)
    {
      smart_band_notification_view_render_center(
        &g_ui.notification_view, &g_ui.runtime.notifications.model);
      (void)smart_band_notification_view_render_presentation(
        &g_ui.notification_view, &g_ui.runtime.notifications);
      if (smart_band_notification_view_presentation_root(
            &g_ui.notification_view) != NULL)
        {
          lv_obj_move_foreground(
            smart_band_notification_view_presentation_root(
              &g_ui.notification_view));
        }
    }
}

static void app_icon_cb(lv_event_t *event)
{
  smart_band_app_id_t id =
    (smart_band_app_id_t)(uintptr_t)lv_event_get_user_data(event);

  if (smart_band_notification_view_captures_input(&g_ui.notification_view))
    {
      return;
    }

  if (g_ui.page_swipe_consumed &&
      lv_tick_elaps(g_ui.page_swipe_at) < SMART_BAND_SWIPE_CLICK_GUARD_MS)
    {
      return;
    }

  g_ui.page_swipe_consumed = false;
  open_app(id);
}

static void system_icon_cb(lv_event_t *event)
{
  smart_band_system_view_t system_view =
    (smart_band_system_view_t)(uintptr_t)lv_event_get_user_data(event);

  if (smart_band_notification_view_captures_input(&g_ui.notification_view))
    {
      return;
    }

  if (g_ui.page_swipe_consumed &&
      lv_tick_elaps(g_ui.page_swipe_at) < SMART_BAND_SWIPE_CLICK_GUARD_MS)
    {
      return;
    }

  g_ui.page_swipe_consumed = false;
  open_system_view(system_view);
}

static void app_back_cb(lv_event_t *event)
{
  (void)event;

  if (smart_band_notification_view_captures_input(&g_ui.notification_view))
    {
      return;
    }

  if (g_ui.system_view == SMART_BAND_SYSTEM_VIEW_WORKOUT &&
      smart_band_workout_view_captures_input(&g_ui.workout_view))
    {
      return;
    }

  if (g_ui.system_view != SMART_BAND_SYSTEM_VIEW_NONE)
    {
      close_system_view();
      return;
    }

  smart_band_app_unmount(&g_ui.runtime.apps);
  if (g_ui.app_content != NULL)
    {
      lv_obj_clean(g_ui.app_content);
    }

  set_label_text(g_ui.app_title, "Apps");
  set_page_visible(g_ui.app_detail, false);
  set_page_visible(g_ui.apps_launcher, true);
}

static void post_workout_command(smart_band_workout_command_t command,
                                 smart_band_workout_mode_t mode)
{
  smart_band_event_t event;

  memset(&event, 0, sizeof(event));
  event.type = SMART_BAND_EVENT_WORKOUT_COMMAND;
  event.payload.workout.command = (uint8_t)command;
  event.payload.workout.mode = (uint8_t)mode;
  if (smart_band_runtime_post(&g_ui.runtime, &event))
    {
      smart_band_runtime_dispatch_pending(&g_ui.runtime);
    }
}

static void workout_action_cb(void *context,
                              smart_band_workout_view_action_t action)
{
  (void)context;

  switch (action)
    {
      case SMART_BAND_WORKOUT_VIEW_ACTION_START_WALK:
        post_workout_command(SMART_BAND_WORKOUT_COMMAND_START,
                             SMART_BAND_WORKOUT_MODE_WALK);
        break;
      case SMART_BAND_WORKOUT_VIEW_ACTION_START_RUN:
        post_workout_command(SMART_BAND_WORKOUT_COMMAND_START,
                             SMART_BAND_WORKOUT_MODE_RUN);
        break;
      case SMART_BAND_WORKOUT_VIEW_ACTION_PAUSE:
        post_workout_command(SMART_BAND_WORKOUT_COMMAND_PAUSE,
                             SMART_BAND_WORKOUT_MODE_WALK);
        break;
      case SMART_BAND_WORKOUT_VIEW_ACTION_RESUME:
        post_workout_command(SMART_BAND_WORKOUT_COMMAND_RESUME,
                             SMART_BAND_WORKOUT_MODE_WALK);
        break;
      case SMART_BAND_WORKOUT_VIEW_ACTION_FINISH:
        post_workout_command(SMART_BAND_WORKOUT_COMMAND_FINISH,
                             SMART_BAND_WORKOUT_MODE_WALK);
        break;
      case SMART_BAND_WORKOUT_VIEW_ACTION_ABORT:
      case SMART_BAND_WORKOUT_VIEW_ACTION_RECOVER_DISCARD:
        post_workout_command(SMART_BAND_WORKOUT_COMMAND_ABORT,
                             SMART_BAND_WORKOUT_MODE_WALK);
        break;
      case SMART_BAND_WORKOUT_VIEW_ACTION_RECOVER_RESUME:
        post_workout_command(SMART_BAND_WORKOUT_COMMAND_CONFIRM_RECOVERY,
                             SMART_BAND_WORKOUT_MODE_WALK);
        post_workout_command(SMART_BAND_WORKOUT_COMMAND_RESUME,
                             SMART_BAND_WORKOUT_MODE_WALK);
        break;
      case SMART_BAND_WORKOUT_VIEW_ACTION_DONE:
        g_ui.runtime.last_workout_result =
          smart_band_workout_service_dismiss_summary(
            &g_ui.runtime.workout, g_ui.runtime.last_clock.elapsed_ms);
        if (g_ui.runtime.last_workout_result ==
            SMART_BAND_WORKOUT_SERVICE_OK)
          {
            close_system_view();
          }
        break;
      default:
        break;
    }

  smart_band_runtime_mark_dirty(&g_ui.runtime,
                                SMART_BAND_DIRTY_WORKOUT |
                                SMART_BAND_DIRTY_HISTORY);
  render_system_view();
}

static void notification_action_cb(
  void *context, uint32_t notification_id,
  smart_band_notification_command_t command)
{
  (void)context;

  if (smart_band_runtime_post_notification_action(
        &g_ui.runtime, notification_id, command, lv_tick_get()))
    {
      smart_band_runtime_dispatch_pending(&g_ui.runtime);
      render_pending();
    }
}

static void step_goal_cb(lv_event_t *event)
{
  uintptr_t direction = (uintptr_t)lv_event_get_user_data(event);
  int delta = direction == 0 ? -SMART_BAND_STEP_GOAL_DELTA :
              SMART_BAND_STEP_GOAL_DELTA;

  if (smart_band_notification_view_captures_input(&g_ui.notification_view))
    {
      return;
    }

  smart_band_adjust_step_goal(&g_ui.runtime.model, delta);
  smart_band_runtime_mark_dirty(&g_ui.runtime, SMART_BAND_DIRTY_STEPS);
  render_pending();
}

static void timer_cb(lv_timer_t *timer)
{
  (void)timer;
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
  g_ui.diagnostic_runtime_ticks++;
#endif
  (void)smart_band_runtime_tick(
    &g_ui.runtime, g_ui.runtime.model.page == SMART_BAND_PAGE_APPS);

  render_pending();
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
  emit_q3_diagnostics();
  emit_q4_diagnostics();
#endif
}

static void event_pump_timer_cb(lv_timer_t *timer)
{
  (void)timer;
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
  g_ui.diagnostic_event_pumps++;
#endif
  smart_band_runtime_dispatch_pending(&g_ui.runtime);
  consume_notification_effects();
  render_pending();
}

static void next_page(void)
{
  smart_band_next_page(&g_ui.runtime.model);
  smart_band_runtime_mark_dirty(&g_ui.runtime, SMART_BAND_DIRTY_PAGE);
  render_pending();
}

static void prev_page(void)
{
  smart_band_prev_page(&g_ui.runtime.model);
  smart_band_runtime_mark_dirty(&g_ui.runtime, SMART_BAND_DIRTY_PAGE);
  render_pending();
}

static void page_drag_cb(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);
  lv_indev_t *indev = lv_indev_get_act();
  lv_point_t point;
  lv_coord_t dx;
  lv_coord_t dy;
  lv_coord_t threshold = max_coord(sx(36), 28);
  uint32_t press_duration;

  if (g_ui.system_view != SMART_BAND_SYSTEM_VIEW_NONE ||
      smart_band_notification_view_captures_input(&g_ui.notification_view))
    {
      return;
    }

  if (indev == NULL)
    {
      return;
    }

  if (code == LV_EVENT_PRESSED)
    {
      lv_indev_get_point(indev, &g_ui.press_point);
      g_ui.press_tick = lv_tick_get();
      g_ui.press_valid = true;
      g_ui.page_swipe_consumed = false;
      return;
    }

  if (code != LV_EVENT_RELEASED || !g_ui.press_valid)
    {
      return;
    }

  g_ui.press_valid = false;
  lv_indev_get_point(indev, &point);
  dx = point.x - g_ui.press_point.x;
  dy = point.y - g_ui.press_point.y;
  press_duration = lv_tick_elaps(g_ui.press_tick);

  if (smart_band_watch_face_picker_is_visible(&g_ui.face_picker))
    {
      if (abs_coord(dx) >= threshold && abs_coord(dx) > abs_coord(dy))
        {
          if (dx < 0)
            {
              smart_band_watch_face_picker_preview_next(&g_ui.face_picker);
            }
          else
            {
              smart_band_watch_face_picker_preview_previous(
                &g_ui.face_picker);
            }

          g_ui.page_swipe_consumed = true;
          g_ui.page_swipe_at = lv_tick_get();
        }

      return;
    }

  if (g_ui.runtime.model.page == SMART_BAND_PAGE_FACE &&
      press_duration >= SMART_BAND_FACE_LONG_PRESS_MS &&
      abs_coord(dx) < threshold && abs_coord(dy) < threshold)
    {
      smart_band_watch_face_picker_show(&g_ui.face_picker,
                                        g_ui.selected_face);
      g_ui.page_swipe_consumed = true;
      g_ui.page_swipe_at = lv_tick_get();
      return;
    }

  if (abs_coord(dx) < threshold || abs_coord(dx) <= abs_coord(dy))
    {
      return;
    }

  g_ui.page_swipe_consumed = true;
  g_ui.page_swipe_at = lv_tick_get();
  if (dx < 0)
    {
      next_page();
    }
  else
    {
      prev_page();
    }
}

static void dot_click_cb(lv_event_t *event)
{
  smart_band_page_t page =
    (smart_band_page_t)(uintptr_t)lv_event_get_user_data(event);

  if (smart_band_notification_view_captures_input(&g_ui.notification_view))
    {
      return;
    }

  switch_to_page(page);
}

static void enable_touch_navigation(lv_obj_t *obj)
{
  if (obj == NULL)
    {
      return;
    }

  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj, page_drag_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(obj, page_drag_cb, LV_EVENT_RELEASED, NULL);
}

static void enable_touch_navigation_tree(lv_obj_t *obj)
{
  uint32_t child_count;

  if (obj == NULL)
    {
      return;
    }

  enable_touch_navigation(obj);

  child_count = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < child_count; i++)
    {
      enable_touch_navigation_tree(lv_obj_get_child(obj, i));
    }
}

int smart_band_lvgl_create(lv_obj_t *parent)
{
  lv_obj_t *parent_root = parent != NULL ? parent : lv_scr_act();
  lv_obj_t *owned_root;
  lv_coord_t root_w;
  lv_coord_t root_h;
  smart_band_clock_source_t clock_source;

  if (parent_root == NULL)
    {
      return -1;
    }

  if (g_ui.root != NULL || g_ui.runtime_timer != NULL ||
      g_ui.event_pump_timer != NULL || g_ui.runtime.initialized ||
      g_ui.event_mutex.initialized
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
      || g_ui.diagnostic_q4_timer != NULL
#endif
     )
    {
      smart_band_lvgl_destroy();
    }

  memset(&g_ui, 0, sizeof(g_ui));
  lv_obj_update_layout(parent_root);
  root_w = lv_obj_get_width(parent_root);
  root_h = lv_obj_get_height(parent_root);
  if (root_w <= 0 || root_h <= 0)
    {
      return -1;
    }

  owned_root = lv_obj_create(parent_root);
  if (owned_root == NULL)
    {
      return -1;
    }

  strip_obj(owned_root);
  lv_obj_set_pos(owned_root, 0, 0);
  lv_obj_set_size(owned_root, root_w, root_h);

  g_ui.root = owned_root;
  configure_local_time();
  memset(&clock_source, 0, sizeof(clock_source));
  clock_source.wall_now = runtime_wall_now;
  clock_source.monotonic_now = runtime_monotonic_now;
  if (runtime_init(&clock_source) != 0)
    {
      lv_obj_del(owned_root);
      g_ui.root = NULL;
      return -1;
    }

  g_ui.selected_face = SMART_BAND_WATCH_FACE_LOTUS;
  g_ui.face_settings_result = SMART_BAND_STORE_UNAVAILABLE;
  if (g_ui.runtime.storage_initialized)
    {
      g_ui.face_settings_result = smart_band_watch_face_settings_load(
        &g_ui.runtime.storage, &g_ui.selected_face);
    }

  if (smart_band_watch_face_registry_find(g_ui.selected_face) == NULL)
    {
      g_ui.selected_face = SMART_BAND_WATCH_FACE_LOTUS;
      g_ui.face_settings_result = SMART_BAND_STORE_DEGRADED;
    }

  if (create_ui_tree(owned_root) != 0)
    {
      smart_band_watch_face_unmount(&g_ui.watch_face);
      smart_band_runtime_deinit(&g_ui.runtime);
      smart_band_event_mutex_deinit(&g_ui.event_mutex);
      lv_obj_del(owned_root);
      g_ui.root = NULL;
      return -1;
    }

  if (smart_band_notification_view_mount(
        &g_ui.notification_view, g_ui.screen, &g_ui.components,
        notification_action_cb, NULL) != 0)
    {
      smart_band_notification_view_unmount(&g_ui.notification_view);
      smart_band_watch_face_unmount(&g_ui.watch_face);
      smart_band_runtime_deinit(&g_ui.runtime);
      smart_band_event_mutex_deinit(&g_ui.event_mutex);
      lv_obj_del(owned_root);
      g_ui.root = NULL;
      return -1;
    }

  g_ui.runtime_timer = lv_timer_create(timer_cb, 1000, NULL);
  if (g_ui.runtime_timer == NULL)
    {
      smart_band_notification_view_unmount(&g_ui.notification_view);
      smart_band_watch_face_unmount(&g_ui.watch_face);
      smart_band_runtime_deinit(&g_ui.runtime);
      smart_band_event_mutex_deinit(&g_ui.event_mutex);
      lv_obj_del(owned_root);
      g_ui.root = NULL;
      return -1;
    }

  g_ui.event_pump_timer = lv_timer_create(event_pump_timer_cb, 50, NULL);
  if (g_ui.event_pump_timer == NULL)
    {
      lv_timer_del(g_ui.runtime_timer);
      g_ui.runtime_timer = NULL;
      smart_band_notification_view_unmount(&g_ui.notification_view);
      smart_band_watch_face_unmount(&g_ui.watch_face);
      smart_band_runtime_deinit(&g_ui.runtime);
      smart_band_event_mutex_deinit(&g_ui.event_mutex);
      lv_obj_del(owned_root);
      g_ui.root = NULL;
      return -1;
    }

  (void)smart_band_runtime_refresh_sensors(&g_ui.runtime);
  render_pending();
  return 0;
}

void smart_band_lvgl_destroy(void)
{
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
  if (g_ui.diagnostic_q4_timer != NULL)
    {
      lv_timer_del(g_ui.diagnostic_q4_timer);
      g_ui.diagnostic_q4_timer = NULL;
    }
#endif

  if (g_ui.event_pump_timer != NULL)
    {
      lv_timer_del(g_ui.event_pump_timer);
      g_ui.event_pump_timer = NULL;
    }

  if (g_ui.runtime_timer != NULL)
    {
      lv_timer_del(g_ui.runtime_timer);
      g_ui.runtime_timer = NULL;
    }

  smart_band_workout_view_unmount(&g_ui.workout_view);
  smart_band_history_view_unmount(&g_ui.history_view);
  smart_band_notification_view_unmount(&g_ui.notification_view);
  smart_band_watch_face_picker_unmount(&g_ui.face_picker);
  smart_band_runtime_deinit(&g_ui.runtime);
  smart_band_event_mutex_deinit(&g_ui.event_mutex);
  smart_band_watch_face_unmount(&g_ui.watch_face);

  if (g_ui.root != NULL)
    {
      if (lv_obj_is_valid(g_ui.root))
        {
          lv_obj_del(g_ui.root);
        }

      g_ui.root = NULL;
    }

  g_ui.watch = NULL;
  g_ui.screen = NULL;
  g_ui.face_host = NULL;
}

bool smart_band_lvgl_post_notification_external(
  const smart_band_notification_utf8_input_t *input,
  uint32_t monotonic_ms)
{
  return g_ui.runtime.initialized &&
         smart_band_runtime_post_notification_external(
           &g_ui.runtime, input, monotonic_ms);
}

bool smart_band_lvgl_set_notification_policy(
  const smart_band_notification_policy_t *policy)
{
  if (!smart_band_runtime_set_notification_policy(&g_ui.runtime, policy))
    {
      return false;
    }

  render_pending();
  return true;
}

#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
bool smart_band_lvgl_get_diagnostics(
  smart_band_lvgl_diagnostics_t *diagnostics)
{
  if (diagnostics == NULL || !g_ui.runtime.initialized)
    {
      return false;
    }

  diagnostics->runtime_ticks = g_ui.diagnostic_runtime_ticks;
  diagnostics->event_pumps = g_ui.diagnostic_event_pumps;
  diagnostics->haptic_events = g_ui.diagnostic_haptic_events;
  diagnostics->wake_requests = g_ui.diagnostic_wake_requests;
  diagnostics->haptic_retries = g_ui.diagnostic_haptic_retries;
  diagnostics->haptic_log_dropped = g_ui.diagnostic_haptic_log_dropped;
  diagnostics->wake_log_dropped = g_ui.diagnostic_wake_log_dropped;
  diagnostics->last_haptic_notification_id =
    g_ui.diagnostic_last_haptic_notification_id;
  diagnostics->last_haptic_generation =
    g_ui.diagnostic_last_haptic_generation;
  diagnostics->last_wake_notification_id =
    g_ui.diagnostic_last_wake_notification_id;
  diagnostics->last_wake_generation =
    g_ui.diagnostic_last_wake_generation;
  diagnostics->last_haptic = g_ui.diagnostic_last_haptic;
  diagnostics->last_haptic_platform_result =
    g_ui.diagnostic_last_haptic_platform_result;
  return true;
}

bool smart_band_lvgl_diagnostics_is_idle(void)
{
  return g_ui.root == NULL && g_ui.runtime_timer == NULL &&
         g_ui.event_pump_timer == NULL &&
         g_ui.diagnostic_q4_timer == NULL && !g_ui.runtime.initialized &&
         !g_ui.event_mutex.initialized;
}

bool smart_band_lvgl_inject_q4_native_scenario_for_test(
  const char *scenario)
{
  static const char *const center_titles[] =
  {
    "Center one", "Center two", "Center three", "Center four", "Center five"
  };
  smart_band_notification_policy_t policy = {true, false};
  size_t accepted = 0u;
  size_t index;

  if (!g_ui.runtime.initialized || scenario == NULL ||
      g_ui.diagnostic_q4_timer != NULL)
    {
      return false;
    }

  if (strcmp(scenario, "ordinary") == 0)
    {
      bool initial = diagnostic_post_notification(
        701u, SMART_BAND_NOTIFICATION_TYPE_SMS,
        SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, "Messages",
        "Native message", "Initial explicit-length notification");

      emit_q4_inject_marker("ordinary", "initial", initial ? 1u : 0u, 1u);
      if (!initial)
        {
          return false;
        }
      g_ui.diagnostic_q4_timer = lv_timer_create(
        diagnostic_q4_ordinary_update_cb, 2500u, NULL);
      return g_ui.diagnostic_q4_timer != NULL;
    }

  if (strcmp(scenario, "center") == 0)
    {
      if (!smart_band_lvgl_set_notification_policy(&policy))
        {
          emit_q4_inject_marker("center", "ready", 0u, 5u);
          return false;
        }
      for (index = 0u; index < 5u; index++)
        {
          if (diagnostic_post_notification(
                711u + (uint32_t)index, SMART_BAND_NOTIFICATION_TYPE_APP,
                SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Inbox",
                center_titles[index], "DND-retained center row"))
            {
              accepted++;
            }
        }
      emit_q4_inject_marker("center", "ready", accepted, 5u);
      return accepted == 5u;
    }

  if (strcmp(scenario, "calls") == 0)
    {
      if (diagnostic_post_notification(
            721u, SMART_BAND_NOTIFICATION_TYPE_CALL,
            SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Phone", "Alice",
            "First native incoming call"))
        {
          accepted++;
        }
      if (diagnostic_post_notification(
            722u, SMART_BAND_NOTIFICATION_TYPE_CALL,
            SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Phone", "Bob",
            "Promoted native incoming call"))
        {
          accepted++;
        }
      emit_q4_inject_marker("calls", "ready", accepted, 2u);
      return accepted == 2u;
    }

  if (strcmp(scenario, "workout") == 0)
    {
      g_ui.diagnostic_q4_timer = lv_timer_create(
        diagnostic_q4_workout_poll_cb, 100u, NULL);
      if (g_ui.diagnostic_q4_timer == NULL)
        {
          emit_q4_inject_marker("workout", "failed", 0u, 1u);
          return false;
        }
      emit_q4_inject_marker("workout", "armed", 0u, 1u);
      return true;
    }

  emit_q4_inject_marker(scenario, "rejected", 0u, 0u);
  return false;
}

bool smart_band_lvgl_set_haptic_adapter_for_test(
  const smart_band_haptic_t *haptic)
{
  if (!g_ui.runtime.initialized || haptic == NULL || haptic->ops == NULL ||
      haptic->ops->play == NULL)
    {
      return false;
    }

  g_ui.runtime.platform.haptic = *haptic;
  return true;
}

bool smart_band_lvgl_set_effect_logger_for_test(
  smart_band_lvgl_effect_log_for_test_t logger, void *context)
{
  if (!g_ui.runtime.initialized)
    {
      return false;
    }

  g_ui.effect_log_for_test = logger;
  g_ui.effect_log_context = context;
  return true;
}
#endif
