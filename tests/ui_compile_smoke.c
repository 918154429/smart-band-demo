#include "app_lvgl.h"
#include "fake_lvgl/fake_lvgl_test.h"
#include "smart_band_apps.h"
#include "smart_band_event_mutex.h"
#include "smart_band_watch_face_id.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if !defined(_WIN32)
#  include <sched.h>
#endif

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
  lv_obj_t *screen;
  lv_obj_t *face;
  lv_obj_t *picker;
  lv_obj_t *heart;
  lv_obj_t *steps;
  lv_obj_t *apps;
  lv_obj_t *launcher;
  lv_obj_t *dots_row;
} ui_tree_t;

static unsigned int g_event_calls;
static uintptr_t g_event_data;
static unsigned int g_timer_calls;

typedef struct
{
  smart_band_platform_result_t results[4];
  size_t result_count;
  size_t result_index;
  size_t play_calls;
  smart_band_haptic_pulse_t pulses[3];
  size_t pulse_count;
} fake_haptic_t;

typedef struct
{
  size_t calls;
  size_t haptic_lines;
  size_t wake_lines;
  bool fail;
} fake_effect_logger_t;

static smart_band_platform_result_t fake_haptic_play(
  void *context, const smart_band_haptic_pulse_t *pulses,
  size_t pulse_count)
{
  fake_haptic_t *fake = context;
  smart_band_platform_result_t result;

  if (fake == NULL || pulses == NULL || pulse_count == 0u ||
      pulse_count > 3u || fake->result_count == 0u)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }
  fake->play_calls++;
  fake->pulse_count = pulse_count;
  memcpy(fake->pulses, pulses, pulse_count * sizeof(pulses[0]));
  result = fake->results[fake->result_index];
  if (fake->result_index + 1u < fake->result_count)
    {
      fake->result_index++;
    }
  return result;
}

static smart_band_platform_result_t fake_haptic_stop(void *context)
{
  (void)context;
  return SMART_BAND_PLATFORM_OK;
}

static const smart_band_haptic_ops_t g_fake_haptic_ops =
{
  fake_haptic_play,
  fake_haptic_stop
};

static bool fake_effect_logger(void *context, const char *line)
{
  fake_effect_logger_t *logger = context;

  if (logger == NULL || line == NULL)
    {
      return false;
    }
  logger->calls++;
  if (strstr(line, "smart_band:q4:haptic:v1") != NULL)
    {
      logger->haptic_lines++;
    }
  if (strstr(line, "smart_band:q4:wake:v1") != NULL)
    {
      logger->wake_lines++;
    }
  return !logger->fail;
}

typedef struct
{
  smart_band_event_lock_t lock;
  unsigned int counter;
} event_mutex_shared_t;

typedef struct
{
  event_mutex_shared_t *shared;
  bool failed;
} event_mutex_worker_arg_t;

static void event_mutex_yield(void)
{
#if defined(_WIN32)
  (void)SwitchToThread();
#else
  (void)sched_yield();
#endif
}

#if defined(_WIN32)
static DWORD WINAPI event_mutex_worker(LPVOID context)
#else
static void *event_mutex_worker(void *context)
#endif
{
  event_mutex_worker_arg_t *arg = context;
  unsigned int iteration;

  for (iteration = 0u; iteration < 2000u; iteration++)
    {
      unsigned int value;

      if (!arg->shared->lock.lock(arg->shared->lock.context))
        {
          arg->failed = true;
          break;
        }
      value = arg->shared->counter;
      event_mutex_yield();
      arg->shared->counter = value + 1u;
      arg->shared->lock.unlock(arg->shared->lock.context);
    }
#if defined(_WIN32)
  return 0u;
#else
  return NULL;
#endif
}

static void test_event_cb(lv_event_t *event)
{
  g_event_calls++;
  g_event_data = (uintptr_t)lv_event_get_user_data(event);
}

static void test_timer_cb(lv_timer_t *timer)
{
  (void)timer;
  g_timer_calls++;
}

static int resources_are_zero(void)
{
  return fake_lvgl_live_object_count() == 0 &&
         fake_lvgl_live_event_count() == 0 &&
         fake_lvgl_live_timer_count() == 0 &&
         smart_band_lvgl_diagnostics_is_idle();
}

static lv_obj_t *ancestor(lv_obj_t *object, unsigned int levels)
{
  while (object != NULL && levels > 0)
    {
      object = fake_lvgl_obj_parent(object);
      levels--;
    }

  return object;
}

static lv_coord_t absolute_y(lv_obj_t *object)
{
  lv_coord_t y = 0;

  while (object != NULL)
    {
      y += lv_obj_get_y(object);
      object = fake_lvgl_obj_parent(object);
    }
  return y;
}

static bool object_is_effectively_hidden(lv_obj_t *object)
{
  while (object != NULL)
    {
      if (fake_lvgl_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN))
        {
          return true;
        }
      object = fake_lvgl_obj_parent(object);
    }
  return false;
}

static lv_obj_t *find_visible_text(const char *text, size_t occurrence)
{
  size_t raw_occurrence = 0u;
  lv_obj_t *object;

  while ((object = fake_lvgl_find_text(
            lv_scr_act(), text, raw_occurrence)) != NULL)
    {
      raw_occurrence++;
      if (!object_is_effectively_hidden(object))
        {
          if (occurrence == 0u)
            {
              return object;
            }
          occurrence--;
        }
    }
  return NULL;
}

static lv_obj_t *click_object_center(lv_obj_t *object)
{
  lv_coord_t x;
  lv_coord_t y;

  if (object == NULL)
    {
      return NULL;
    }
  x = fake_lvgl_obj_absolute_x(object) + lv_obj_get_width(object) / 2;
  y = fake_lvgl_obj_absolute_y(object) + lv_obj_get_height(object) / 2;
  return fake_lvgl_send_event_at(lv_scr_act(), x, y, LV_EVENT_CLICKED);
}

static lv_obj_t *find_dots_row(lv_obj_t *screen)
{
  uint32_t child_count = lv_obj_get_child_count(screen);

  for (uint32_t index = 0; index < child_count; index++)
    {
      lv_obj_t *candidate = lv_obj_get_child(screen, (int32_t)index);
      uint32_t dot_count = lv_obj_get_child_count(candidate);
      bool dots = dot_count == 4u;

      for (uint32_t dot_index = 0; dots && dot_index < dot_count; dot_index++)
        {
          lv_obj_t *dot = lv_obj_get_child(candidate, (int32_t)dot_index);

          dots = fake_lvgl_obj_has_flag(dot, LV_OBJ_FLAG_CLICKABLE) &&
                 fake_lvgl_obj_text(dot)[0] == '\0';
        }

      if (dots)
        {
          return candidate;
        }
    }

  return NULL;
}

static int inspect_ui_tree(ui_tree_t *tree)
{
  lv_obj_t *sleep = fake_lvgl_find_text(lv_scr_act(), "Sleep", 0);
  lv_obj_t *resting = fake_lvgl_find_text(lv_scr_act(), "Resting", 0);
  lv_obj_t *activity = fake_lvgl_find_text(lv_scr_act(), "Activity", 0);
  lv_obj_t *apps = fake_lvgl_find_text(lv_scr_act(), "Apps", 0);
  lv_obj_t *calculator =
    fake_lvgl_find_text(lv_scr_act(), "Calculator", 0);
  lv_obj_t *picker = fake_lvgl_find_text(lv_scr_act(), "Watch faces", 0);

  if (tree == NULL || sleep == NULL || resting == NULL || activity == NULL ||
      apps == NULL || calculator == NULL || picker == NULL)
    {
      return -1;
    }

  tree->face = ancestor(sleep, 2);
  tree->picker = ancestor(picker, 1);
  tree->heart = ancestor(resting, 2);
  tree->steps = ancestor(activity, 1);
  tree->apps = ancestor(apps, 1);
  tree->launcher = ancestor(calculator, 2);
  tree->screen = fake_lvgl_obj_parent(tree->steps);
  tree->dots_row = find_dots_row(tree->screen);
  return tree->face != NULL && tree->heart != NULL && tree->steps != NULL &&
         tree->apps != NULL && tree->launcher != NULL && tree->screen != NULL &&
         tree->picker != NULL && tree->dots_row != NULL ?
         0 : -1;
}

static void swipe_left(lv_obj_t *screen)
{
  fake_lvgl_set_pointer(250, 200);
  fake_lvgl_send_event(screen, LV_EVENT_PRESSED);
  fake_lvgl_set_pointer(50, 200);
  fake_lvgl_send_event(screen, LV_EVENT_RELEASED);
}

static void long_press(lv_obj_t *object)
{
  fake_lvgl_set_pointer(160, 220);
  fake_lvgl_send_event(object, LV_EVENT_PRESSED);
  (void)fake_lvgl_advance_tick(650);
  fake_lvgl_set_pointer(160, 220);
  fake_lvgl_send_event(object, LV_EVENT_RELEASED);
}

static lv_obj_t *active_face_root(smart_band_watch_face_id_t id)
{
  lv_obj_t *marker;

  switch (id)
    {
      case SMART_BAND_WATCH_FACE_ACTIVITY:
        marker = fake_lvgl_find_text(lv_scr_act(), "ACTIVITY", 0);
        break;
      case SMART_BAND_WATCH_FACE_MINIMAL:
        marker = fake_lvgl_find_text(lv_scr_act(), "MINIMAL", 0);
        break;
      case SMART_BAND_WATCH_FACE_LOTUS:
      default:
        marker = fake_lvgl_find_text(lv_scr_act(), "Sleep", 0);
        return ancestor(marker, 2);
    }

  return ancestor(marker, 1);
}

static int navigate_to_apps(ui_tree_t *tree)
{
  if (inspect_ui_tree(tree) != 0)
    {
      return -1;
    }

  swipe_left(tree->screen);
  if (fake_lvgl_obj_has_flag(tree->heart, LV_OBJ_FLAG_HIDDEN))
    {
      return -1;
    }

  swipe_left(tree->screen);
  if (fake_lvgl_obj_has_flag(tree->steps, LV_OBJ_FLAG_HIDDEN))
    {
      return -1;
    }

  swipe_left(tree->screen);
  if (fake_lvgl_obj_has_flag(tree->apps, LV_OBJ_FLAG_HIDDEN))
    {
      return -1;
    }

  (void)fake_lvgl_advance_tick(300);
  return 0;
}

static int test_fake_primitives(void)
{
  lv_obj_t *parent;
  lv_obj_t *first;
  lv_obj_t *second;
  lv_timer_t *timer;
  uint32_t before_wrap;

  fake_lvgl_reset();
  CHECK(resources_are_zero());
  parent = lv_obj_create(lv_scr_act());
  first = lv_label_create(parent);
  second = lv_obj_create(parent);
  CHECK(parent != NULL && first != NULL && second != NULL);
  lv_obj_set_size(parent, 100, 100);
  CHECK(fake_lvgl_live_object_count() == 3);
  CHECK(lv_obj_get_child_count(parent) == 2);
  CHECK(lv_obj_get_child(parent, 0) == first);
  CHECK(lv_obj_get_child(parent, -1) == second);
  CHECK(fake_lvgl_obj_parent(first) == parent);

  lv_label_set_text(first, "before");
  CHECK(strcmp(fake_lvgl_obj_text(first), "before") == 0);
  lv_label_set_text(first, "after");
  CHECK(strcmp(fake_lvgl_obj_text(first), "after") == 0);
  lv_obj_add_flag(first, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
  CHECK(fake_lvgl_obj_has_flag(first, LV_OBJ_FLAG_CLICKABLE));
  CHECK(fake_lvgl_obj_has_flag(first, LV_OBJ_FLAG_HIDDEN));
  lv_obj_clear_flag(first, LV_OBJ_FLAG_HIDDEN);
  CHECK(!fake_lvgl_obj_has_flag(first, LV_OBJ_FLAG_HIDDEN));

  g_event_calls = 0;
  g_event_data = 0;
  lv_obj_add_event_cb(first, test_event_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)1234);
  CHECK(fake_lvgl_live_event_count() == 1);
  fake_lvgl_send_event(first, LV_EVENT_PRESSED);
  CHECK(g_event_calls == 0);
  fake_lvgl_send_event(first, LV_EVENT_CLICKED);
  CHECK(g_event_calls == 1 && g_event_data == 1234);

  lv_obj_set_pos(first, 0, 0);
  lv_obj_set_size(first, 50, 50);
  lv_obj_set_pos(second, 0, 0);
  lv_obj_set_size(second, 50, 50);
  lv_obj_add_flag(second, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(second, test_event_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)5678);
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 10, 10,
                                LV_EVENT_CLICKED) == second);
  CHECK(g_event_data == 5678);
  lv_obj_move_foreground(first);
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 10, 10,
                                LV_EVENT_CLICKED) == first);
  CHECK(g_event_data == 1234);
  lv_obj_add_flag(first, LV_OBJ_FLAG_HIDDEN);
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 10, 10,
                                LV_EVENT_CLICKED) == second);
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 50, 10,
                                LV_EVENT_CLICKED) == NULL);

  lv_obj_clean(parent);
  CHECK(lv_obj_get_child_count(parent) == 0);
  CHECK(fake_lvgl_live_object_count() == 1);
  CHECK(fake_lvgl_live_event_count() == 0);
  lv_obj_del(parent);
  CHECK(resources_are_zero());

  g_timer_calls = 0;
  timer = lv_timer_create(test_timer_cb, 100, NULL);
  CHECK(timer != NULL && fake_lvgl_live_timer_count() == 1);
  CHECK(fake_lvgl_advance_tick(250) == 2);
  CHECK(g_timer_calls == 2);
  lv_timer_del(timer);
  CHECK(fake_lvgl_live_timer_count() == 0);

  fake_lvgl_set_tick(UINT32_MAX - 15u);
  before_wrap = lv_tick_get();
  CHECK(fake_lvgl_advance_tick(32) == 0);
  CHECK(lv_tick_elaps(before_wrap) == 32);

  fake_lvgl_fail_object_create_at(1);
  CHECK(lv_obj_create(lv_scr_act()) == NULL);
  CHECK(fake_lvgl_object_create_attempts() == 1);
  fake_lvgl_fail_object_create_at(0);
  fake_lvgl_fail_timer_create_at(1);
  CHECK(lv_timer_create(test_timer_cb, 100, NULL) == NULL);
  CHECK(fake_lvgl_timer_create_attempts() == 1);
  fake_lvgl_fail_timer_create_at(0);
  CHECK(resources_are_zero());
  return 0;
}

static int run_object_failure_sweep(lv_coord_t width, lv_coord_t height)
{
  size_t create_count;
  size_t failure;

  fake_lvgl_reset();
  lv_obj_set_size(lv_scr_act(), width, height);
  fake_lvgl_fail_object_create_at(0);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  create_count = fake_lvgl_object_create_attempts();
  CHECK(create_count > 0);
  smart_band_lvgl_destroy();
  CHECK(resources_are_zero());

  for (failure = 1; failure <= create_count; failure++)
    {
      fake_lvgl_fail_object_create_at(failure);
      if (smart_band_lvgl_create(NULL) == 0)
        {
          fprintf(stderr,
                  "object create failure %zu/%zu was not propagated at %dx%d\n",
                  failure, create_count, (int)width, (int)height);
          smart_band_lvgl_destroy();
          return 1;
        }

      CHECK(resources_are_zero());
      fake_lvgl_fail_object_create_at(0);
      CHECK(smart_band_lvgl_create(NULL) == 0);
      smart_band_lvgl_destroy();
      CHECK(resources_are_zero());
    }

  return 0;
}

static int test_timer_failure_retry(void)
{
  size_t failure;

  fake_lvgl_reset();
  lv_obj_set_size(lv_scr_act(), 320, 480);
  for (failure = 1u; failure <= 2u; failure++)
    {
      fake_lvgl_fail_timer_create_at(failure);
      CHECK(smart_band_lvgl_create(NULL) != 0);
      CHECK(resources_are_zero());
    }
  fake_lvgl_fail_timer_create_at(0);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(fake_lvgl_live_timer_count() == 2);
  smart_band_lvgl_destroy();
  CHECK(resources_are_zero());
  return 0;
}

static int run_app_mount_failure_sweep(void)
{
  static const char *const app_names[SMART_BAND_APP_COUNT] =
  {
    "Weather", "Calculator", "Timer", "2048", "Stopwatch", "Mines",
    "Tetris", "Wooden Fish"
  };
  size_t app_index;

  for (app_index = 0; app_index < SMART_BAND_APP_COUNT; app_index++)
    {
      ui_tree_t tree;
      lv_obj_t *launcher;
      lv_obj_t *back;
      size_t create_count;
      size_t failure;
      size_t static_objects;
      size_t static_events;

      fake_lvgl_reset();
      lv_obj_set_size(lv_scr_act(), 320, 480);
      CHECK(smart_band_lvgl_create(NULL) == 0);
      CHECK(navigate_to_apps(&tree) == 0);
      launcher = fake_lvgl_find_text(tree.launcher, app_names[app_index], 0);
      CHECK(launcher != NULL);
      back = fake_lvgl_find_text(tree.apps, "<", 0);
      CHECK(back != NULL);
      static_objects = fake_lvgl_live_object_count();
      static_events = fake_lvgl_live_event_count();

      fake_lvgl_fail_object_create_at(0);
      fake_lvgl_send_event(launcher, LV_EVENT_CLICKED);
      CHECK(fake_lvgl_find_text(tree.apps, "App failed", 0) == NULL);
      create_count = fake_lvgl_object_create_attempts();
      CHECK(create_count > 0);
      fake_lvgl_send_event(back, LV_EVENT_CLICKED);
      CHECK(fake_lvgl_live_object_count() == static_objects);
      CHECK(fake_lvgl_live_event_count() == static_events);

      for (failure = 1; failure <= create_count; failure++)
        {
          fake_lvgl_fail_object_create_at(failure);
          fake_lvgl_send_event(launcher, LV_EVENT_CLICKED);
          if (fake_lvgl_find_text(tree.apps, "App failed", 0) == NULL)
            {
              fprintf(stderr,
                      "%s mount create failure %zu/%zu was not propagated\n",
                      app_names[app_index], failure, create_count);
              smart_band_lvgl_destroy();
              return 1;
            }

          CHECK(fake_lvgl_live_object_count() == static_objects);
          CHECK(fake_lvgl_live_event_count() == static_events);
          fake_lvgl_send_event(back, LV_EVENT_CLICKED);

          fake_lvgl_fail_object_create_at(0);
          fake_lvgl_send_event(launcher, LV_EVENT_CLICKED);
          CHECK(fake_lvgl_find_text(tree.apps, "App failed", 0) == NULL);
          fake_lvgl_send_event(back, LV_EVENT_CLICKED);
          CHECK(fake_lvgl_live_object_count() == static_objects);
          CHECK(fake_lvgl_live_event_count() == static_events);
        }

      smart_band_lvgl_destroy();
      CHECK(resources_are_zero());
    }

  return 0;
}

static int test_navigation_and_timer(void)
{
  ui_tree_t tree;
  lv_obj_t *goal_up;
  lv_obj_t *timer_launcher;
  lv_obj_t *start;
  lv_obj_t *back;
  size_t static_objects;
  size_t static_events;

  fake_lvgl_reset();
  lv_obj_set_size(lv_scr_act(), 320, 480);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(inspect_ui_tree(&tree) == 0);
  CHECK(!fake_lvgl_obj_has_flag(tree.face, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_obj_has_flag(tree.heart, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_obj_has_flag(tree.steps, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_obj_has_flag(tree.apps, LV_OBJ_FLAG_HIDDEN));

  swipe_left(tree.screen);
  CHECK(!fake_lvgl_obj_has_flag(tree.heart, LV_OBJ_FLAG_HIDDEN));
  swipe_left(tree.screen);
  CHECK(!fake_lvgl_obj_has_flag(tree.steps, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_find_text(tree.steps, "8000", 0) != NULL);
  goal_up = fake_lvgl_find_text(tree.steps, "+", 0);
  CHECK(goal_up != NULL);
  fake_lvgl_send_event(goal_up, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_find_text(tree.steps, "9000", 0) != NULL);

  swipe_left(tree.screen);
  CHECK(!fake_lvgl_obj_has_flag(tree.apps, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_advance_tick(300) == 6);
  {
    lv_obj_t *last_card = fake_lvgl_obj_parent(
      find_visible_text("Wooden Fish", 0u));

    CHECK(last_card != NULL);
    CHECK(absolute_y(last_card) + lv_obj_get_height(last_card) <=
          lv_obj_get_height(tree.screen));
  }
  static_objects = fake_lvgl_live_object_count();
  static_events = fake_lvgl_live_event_count();
  timer_launcher = fake_lvgl_find_text(tree.launcher, "Timer", 0);
  CHECK(timer_launcher != NULL);
  fake_lvgl_send_event(timer_launcher, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_live_object_count() > static_objects);
  CHECK(fake_lvgl_find_text(tree.apps, "05:00", 0) != NULL);
  start = fake_lvgl_find_text(tree.apps, "Start", 0);
  CHECK(start != NULL);
  fake_lvgl_send_event(start, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_advance_tick(1000) == 21);
  CHECK(fake_lvgl_find_text(tree.apps, "05:00", 0) != NULL);
  CHECK(fake_lvgl_advance_tick(700) == 15);
  CHECK(fake_lvgl_find_text(tree.apps, "04:59", 0) != NULL);
  back = fake_lvgl_find_text(tree.apps, "<", 0);
  CHECK(back != NULL);
  fake_lvgl_send_event(back, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_live_object_count() == static_objects);
  CHECK(fake_lvgl_live_event_count() == static_events);

  smart_band_lvgl_destroy();
  CHECK(resources_are_zero());
  return 0;
}

static int test_watch_face_mount_failure_rollback(void)
{
  ui_tree_t tree;
  lv_obj_t *next;
  lv_obj_t *apply;

  fake_lvgl_reset();
  lv_obj_set_size(lv_scr_act(), 320, 480);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(inspect_ui_tree(&tree) == 0);
  long_press(tree.face);
  CHECK(!fake_lvgl_obj_has_flag(tree.picker, LV_OBJ_FLAG_HIDDEN));
  next = fake_lvgl_find_text(tree.picker, "Next", 0);
  apply = fake_lvgl_find_text(tree.picker, "Apply", 0);
  CHECK(next != NULL && apply != NULL);
  fake_lvgl_send_event(next, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_find_text(tree.picker, "Activity Rings", 0) != NULL);

  fake_lvgl_fail_object_create_at(1);
  fake_lvgl_send_event(apply, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_find_text(tree.picker, "Face unavailable", 0) != NULL);
  CHECK(active_face_root(SMART_BAND_WATCH_FACE_LOTUS) != NULL);
  CHECK(!fake_lvgl_obj_has_flag(tree.picker, LV_OBJ_FLAG_HIDDEN));

  fake_lvgl_fail_object_create_at(0);
  fake_lvgl_send_event(apply, LV_EVENT_CLICKED);
  CHECK(active_face_root(SMART_BAND_WATCH_FACE_ACTIVITY) != NULL);
  CHECK(fake_lvgl_obj_has_flag(tree.picker, LV_OBJ_FLAG_HIDDEN));
  smart_band_lvgl_destroy();
  CHECK(resources_are_zero());
  return 0;
}

static int test_workout_and_history_system_views(void)
{
  ui_tree_t tree;
  lv_obj_t *workout;
  lv_obj_t *walk;
  lv_obj_t *pause;
  lv_obj_t *resume;
  lv_obj_t *finish;
  lv_obj_t *confirm;
  lv_obj_t *done;
  lv_obj_t *history;
  lv_obj_t *back;
  size_t static_objects;
  size_t static_events;

  fake_lvgl_reset();
  lv_obj_set_size(lv_scr_act(), 320, 480);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(navigate_to_apps(&tree) == 0);
  static_objects = fake_lvgl_live_object_count();
  static_events = fake_lvgl_live_event_count();
  workout = fake_lvgl_find_text(tree.launcher, "Workout", 0);
  CHECK(workout != NULL);
  CHECK(!fake_lvgl_obj_has_flag(tree.dots_row, LV_OBJ_FLAG_HIDDEN));
  fake_lvgl_send_event(workout, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_obj_has_flag(tree.dots_row, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_find_text(tree.apps, "Choose a workout", 0) != NULL);
  walk = fake_lvgl_find_text(tree.apps, "Walk", 0);
  CHECK(walk != NULL);
  fake_lvgl_send_event(walk, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_find_text(tree.apps, "Starting", 0) != NULL);
  CHECK(fake_lvgl_advance_tick(3700) == 78);
  CHECK(fake_lvgl_find_text(tree.apps, "Active", 0) != NULL);

  pause = fake_lvgl_find_text(tree.apps, "Pause", 0);
  CHECK(pause != NULL);
  CHECK(absolute_y(fake_lvgl_obj_parent(pause)) >= 0);
  CHECK(absolute_y(fake_lvgl_obj_parent(pause)) +
        lv_obj_get_height(fake_lvgl_obj_parent(pause)) <= 480);
  fake_lvgl_send_event(pause, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_find_text(tree.apps, "Paused", 0) != NULL);
  resume = fake_lvgl_find_text(tree.apps, "Resume", 0);
  CHECK(resume != NULL);
  fake_lvgl_send_event(resume, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_find_text(tree.apps, "Active", 0) != NULL);

  back = fake_lvgl_find_text(tree.apps, "<", 0);
  CHECK(back != NULL);
  fake_lvgl_send_event(back, LV_EVENT_CLICKED);
  CHECK(!fake_lvgl_obj_has_flag(tree.dots_row, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_live_object_count() == static_objects);
  CHECK(fake_lvgl_live_event_count() == static_events);
  fake_lvgl_send_event(workout, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_obj_has_flag(tree.dots_row, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_find_text(tree.apps, "Active", 0) != NULL);

  finish = fake_lvgl_find_text(tree.apps, "Finish", 0);
  CHECK(finish != NULL);
  fake_lvgl_send_event(finish, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_find_text(tree.apps, "Finish workout?", 0) != NULL);
  swipe_left(tree.screen);
  CHECK(!fake_lvgl_obj_has_flag(tree.apps, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_find_text(tree.apps, "Finish workout?", 0) != NULL);
  back = fake_lvgl_find_text(tree.apps, "<", 0);
  CHECK(back != NULL);
  fake_lvgl_send_event(back, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_find_text(tree.apps, "Finish workout?", 0) != NULL);
  confirm = fake_lvgl_find_text(tree.apps, "Confirm", 0);
  CHECK(confirm != NULL);
  fake_lvgl_send_event(confirm, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_find_text(tree.apps, "Walk complete", 0) != NULL);
  done = fake_lvgl_find_text(tree.apps, "Done", 0);
  CHECK(done != NULL);
  fake_lvgl_send_event(done, LV_EVENT_CLICKED);
  CHECK(!fake_lvgl_obj_has_flag(tree.dots_row, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_live_object_count() == static_objects);
  CHECK(fake_lvgl_live_event_count() == static_events);

  history = fake_lvgl_find_text(tree.launcher, "History", 0);
  CHECK(history != NULL);
  fake_lvgl_send_event(history, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_obj_has_flag(tree.dots_row, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_find_text(tree.apps, "7-day steps", 0) != NULL);
  CHECK(fake_lvgl_find_text(tree.apps, "Latest Walk", 0) != NULL);
  back = fake_lvgl_find_text(tree.apps, "<", 0);
  CHECK(back != NULL);
  fake_lvgl_send_event(back, LV_EVENT_CLICKED);
  CHECK(!fake_lvgl_obj_has_flag(tree.dots_row, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_live_object_count() == static_objects);
  CHECK(fake_lvgl_live_event_count() == static_events);
  smart_band_lvgl_destroy();
  CHECK(resources_are_zero());
  return 0;
}

static int test_watch_face_picker_switch_soak(void)
{
  size_t object_counts[SMART_BAND_WATCH_FACE_COUNT] = {0, 0, 0};
  size_t event_counts[SMART_BAND_WATCH_FACE_COUNT] = {0, 0, 0};
  bool seen[SMART_BAND_WATCH_FACE_COUNT] = {false, false, false};
  smart_band_watch_face_id_t current = SMART_BAND_WATCH_FACE_LOTUS;
  ui_tree_t tree;
  int iteration;

  fake_lvgl_reset();
  lv_obj_set_size(lv_scr_act(), 320, 480);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(inspect_ui_tree(&tree) == 0);
  CHECK(fake_lvgl_obj_has_flag(tree.picker, LV_OBJ_FLAG_HIDDEN));

  object_counts[current] = fake_lvgl_live_object_count();
  event_counts[current] = fake_lvgl_live_event_count();
  seen[current] = true;

  for (iteration = 0; iteration < 100; iteration++)
    {
      lv_obj_t *face = active_face_root(current);
      lv_obj_t *apply;

      CHECK(face != NULL);
      long_press(face);
      CHECK(!fake_lvgl_obj_has_flag(tree.picker, LV_OBJ_FLAG_HIDDEN));
      CHECK(fake_lvgl_obj_has_flag(tree.heart, LV_OBJ_FLAG_HIDDEN));

      if (iteration == 0)
        {
          swipe_left(tree.picker);
        }
      else
        {
          lv_obj_t *next = fake_lvgl_find_text(tree.picker, "Next", 0);
          CHECK(next != NULL);
          fake_lvgl_send_event(next, LV_EVENT_CLICKED);
        }

      current = (smart_band_watch_face_id_t)(
        ((int)current + 1) % SMART_BAND_WATCH_FACE_COUNT);
      apply = fake_lvgl_find_text(tree.picker, "Apply", 0);
      CHECK(apply != NULL);
      fake_lvgl_send_event(apply, LV_EVENT_CLICKED);
      CHECK(fake_lvgl_obj_has_flag(tree.picker, LV_OBJ_FLAG_HIDDEN));
      CHECK(active_face_root(current) != NULL);

      if (!seen[current])
        {
          object_counts[current] = fake_lvgl_live_object_count();
          event_counts[current] = fake_lvgl_live_event_count();
          seen[current] = true;
        }
      else
        {
          CHECK(fake_lvgl_live_object_count() == object_counts[current]);
          CHECK(fake_lvgl_live_event_count() == event_counts[current]);
        }
    }

  CHECK(seen[SMART_BAND_WATCH_FACE_LOTUS]);
  CHECK(seen[SMART_BAND_WATCH_FACE_ACTIVITY]);
  CHECK(seen[SMART_BAND_WATCH_FACE_MINIMAL]);
  smart_band_lvgl_destroy();
  CHECK(resources_are_zero());
  return 0;
}

static int test_event_mutex_contention(void)
{
  smart_band_event_mutex_t mutex;
  event_mutex_shared_t shared;
  event_mutex_worker_arg_t args[4];
  size_t index;

#if defined(_WIN32)
  HANDLE threads[4];
#else
  pthread_t threads[4];
#endif

  (void)memset(&mutex, 0, sizeof(mutex));
  (void)memset(&shared, 0, sizeof(shared));
  (void)memset(args, 0, sizeof(args));
  CHECK(smart_band_event_mutex_init(NULL) != 0);
  CHECK(!smart_band_event_mutex_get_lock(NULL, &shared.lock));
  CHECK(smart_band_event_mutex_init(&mutex) == 0);
  CHECK(!smart_band_event_mutex_get_lock(&mutex, NULL));
  CHECK(smart_band_event_mutex_get_lock(&mutex, &shared.lock));

  for (index = 0u; index < 4u; index++)
    {
      args[index].shared = &shared;
#if defined(_WIN32)
      threads[index] = CreateThread(NULL, 0u, event_mutex_worker,
                                    &args[index], 0u, NULL);
      CHECK(threads[index] != NULL);
#else
      CHECK(pthread_create(&threads[index], NULL, event_mutex_worker,
                           &args[index]) == 0);
#endif
    }

#if defined(_WIN32)
  CHECK(WaitForMultipleObjects(4u, threads, TRUE, INFINITE) == WAIT_OBJECT_0);
  for (index = 0u; index < 4u; index++)
    {
      CHECK(CloseHandle(threads[index]) != 0);
    }
#else
  for (index = 0u; index < 4u; index++)
    {
      CHECK(pthread_join(threads[index], NULL) == 0);
    }
#endif
  for (index = 0u; index < 4u; index++)
    {
      CHECK(!args[index].failed);
    }
  CHECK(shared.counter == 8000u);
  smart_band_event_mutex_deinit(&mutex);
  smart_band_event_mutex_deinit(&mutex);
  CHECK(!smart_band_event_mutex_get_lock(&mutex, &shared.lock));
  return 0;
}

static int test_event_pump_is_independent_from_runtime_tick(void)
{
  smart_band_lvgl_diagnostics_t diagnostics;

  fake_lvgl_reset();
  lv_obj_set_size(lv_scr_act(), 320, 480);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.runtime_ticks == 0u);
  CHECK(diagnostics.event_pumps == 0u);
  CHECK(fake_lvgl_advance_tick(999u) == 19u);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.runtime_ticks == 0u);
  CHECK(diagnostics.event_pumps == 19u);
  CHECK(fake_lvgl_advance_tick(1u) == 2u);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.runtime_ticks == 1u);
  CHECK(diagnostics.event_pumps == 20u);
  smart_band_lvgl_destroy();
  CHECK(!smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(resources_are_zero());
  return 0;
}

static smart_band_notification_input_t make_notification(
  uint32_t id, smart_band_notification_type_t type,
  smart_band_notification_priority_t priority, const char *source,
  const char *title, const char *body)
{
  smart_band_notification_input_t input;

  input.id = id;
  input.type = type;
  input.priority = priority;
  input.source = source;
  input.title = title;
  input.body = body;
  input.wall_timestamp = id;
  return input;
}

static int test_notification_overlay_pump_and_timeout(void)
{
  ui_tree_t tree;
  smart_band_notification_input_t first = make_notification(
    41u, SMART_BAND_NOTIFICATION_TYPE_SMS,
    SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, "Messages", "Message one",
    "Pump-owned delivery");
  smart_band_notification_input_t second = make_notification(
    42u, SMART_BAND_NOTIFICATION_TYPE_APP,
    SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, "Calendar", "Timeout card",
    "Five second boundary");
  lv_obj_t *title;
  lv_obj_t *overlay;
  lv_obj_t *target;

  fake_lvgl_reset();
  CHECK(!smart_band_lvgl_post_notification_external(&first, 0u));
  lv_obj_set_size(lv_scr_act(), 320, 480);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(navigate_to_apps(&tree) == 0);
  CHECK(smart_band_lvgl_post_notification_external(&first, 300u));
  CHECK(fake_lvgl_advance_tick(49u) == 0u);
  CHECK(find_visible_text("Message one", 0u) == NULL);
  CHECK(fake_lvgl_advance_tick(1u) == 1u);
  title = find_visible_text("Message one", 0u);
  CHECK(title != NULL);
  overlay = fake_lvgl_obj_parent(title);
  CHECK(overlay != NULL &&
        fake_lvgl_obj_has_flag(overlay, LV_OBJ_FLAG_CLICKABLE));
  target = fake_lvgl_send_event_at(lv_scr_act(), 86, 120,
                                   LV_EVENT_CLICKED);
  if (target != overlay)
    {
      fprintf(stderr,
              "overlay hit mismatch target=%s overlay=(%d,%d,%d,%d)\n",
              target == NULL ? "NULL" : fake_lvgl_obj_text(target),
              (int)fake_lvgl_obj_absolute_x(overlay),
              (int)fake_lvgl_obj_absolute_y(overlay),
              (int)lv_obj_get_width(overlay),
              (int)lv_obj_get_height(overlay));
    }
  CHECK(target == overlay);
  CHECK(find_visible_text("Choose a workout", 0u) == NULL);
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 180, 120,
                                LV_EVENT_PRESSED) == overlay);
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 50, 120,
                                LV_EVENT_RELEASED) == overlay);
  CHECK(!fake_lvgl_obj_has_flag(tree.apps, LV_OBJ_FLAG_HIDDEN));

  CHECK(click_object_center(find_visible_text("Dismiss", 0u)) != NULL);
  CHECK(find_visible_text("Message one", 0u) == NULL);
  CHECK(smart_band_lvgl_post_notification_external(&first, 350u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(find_visible_text("Message one", 0u) == NULL);

  CHECK(smart_band_lvgl_post_notification_external(&second, 400u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(find_visible_text("Timeout card", 0u) != NULL);
  (void)fake_lvgl_advance_tick(4949u);
  CHECK(find_visible_text("Timeout card", 0u) != NULL);
  (void)fake_lvgl_advance_tick(1u);
  CHECK(find_visible_text("Timeout card", 0u) == NULL);

  smart_band_lvgl_destroy();
  CHECK(!smart_band_lvgl_post_notification_external(&second, 5400u));
  CHECK(resources_are_zero());
  return 0;
}

static int test_notification_haptic_platform_contract(void)
{
  smart_band_lvgl_diagnostics_t diagnostics;
  fake_haptic_t fake;
  fake_effect_logger_t logger;
  smart_band_haptic_t adapter = {&g_fake_haptic_ops, &fake};
  smart_band_notification_input_t input;
  size_t calls_before;

  memset(&fake, 0, sizeof(fake));
  memset(&logger, 0, sizeof(logger));
  fake_lvgl_reset();
  CHECK(!smart_band_lvgl_set_haptic_adapter_for_test(&adapter));
  CHECK(!smart_band_lvgl_set_effect_logger_for_test(fake_effect_logger,
                                                     &logger));
  lv_obj_set_size(lv_scr_act(), 320, 480);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(!smart_band_lvgl_set_haptic_adapter_for_test(NULL));
  CHECK(smart_band_lvgl_set_effect_logger_for_test(fake_effect_logger,
                                                    &logger));

  fake.results[0] = SMART_BAND_PLATFORM_OK;
  fake.result_count = 1u;
  CHECK(smart_band_lvgl_set_haptic_adapter_for_test(&adapter));
  input = make_notification(
    501u, SMART_BAND_NOTIFICATION_TYPE_APP,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Platform", "Adapter OK",
    "Play before ack");
  CHECK(smart_band_lvgl_post_notification_external(&input, 0u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(fake.play_calls == 1u && fake.pulse_count == 2u);
  CHECK(fake.pulses[0].on_ms == 70u && fake.pulses[0].off_ms == 50u &&
        fake.pulses[0].strength == 60u);
  CHECK(fake.pulses[1].on_ms == 70u && fake.pulses[1].off_ms == 0u &&
        fake.pulses[1].strength == 60u);
  CHECK(logger.haptic_lines == 0u && logger.wake_lines == 1u);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.haptic_events == 1u &&
        diagnostics.wake_requests == 1u &&
        diagnostics.last_haptic_platform_result == SMART_BAND_PLATFORM_OK);

  memset(&fake, 0, sizeof(fake));
  fake.results[0] = SMART_BAND_PLATFORM_BUSY;
  fake.results[1] = SMART_BAND_PLATFORM_OK;
  fake.result_count = 2u;
  CHECK(smart_band_lvgl_set_haptic_adapter_for_test(&adapter));
  input = make_notification(
    502u, SMART_BAND_NOTIFICATION_TYPE_APP,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Platform", "Adapter busy",
    "Retry same generation");
  CHECK(smart_band_lvgl_post_notification_external(&input, 50u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(fake.play_calls == 1u);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.haptic_events == 1u &&
        diagnostics.wake_requests == 2u &&
        diagnostics.haptic_retries == 1u &&
        diagnostics.last_haptic_platform_result == SMART_BAND_PLATFORM_BUSY);
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(fake.play_calls == 2u);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.haptic_events == 2u &&
        diagnostics.haptic_retries == 1u &&
        diagnostics.last_haptic_notification_id == 502u);

  memset(&fake, 0, sizeof(fake));
  fake.results[0] = SMART_BAND_PLATFORM_IO;
  fake.results[1] = SMART_BAND_PLATFORM_OK;
  fake.result_count = 2u;
  CHECK(smart_band_lvgl_set_haptic_adapter_for_test(&adapter));
  input = make_notification(
    503u, SMART_BAND_NOTIFICATION_TYPE_APP,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Platform", "Adapter IO",
    "Retain pending on IO");
  CHECK(smart_band_lvgl_post_notification_external(&input, 150u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(fake.play_calls == 1u);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.haptic_events == 2u &&
        diagnostics.wake_requests == 3u &&
        diagnostics.haptic_retries == 2u &&
        diagnostics.last_haptic_platform_result == SMART_BAND_PLATFORM_IO);
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(fake.play_calls == 2u);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.haptic_events == 3u &&
        diagnostics.last_haptic_notification_id == 503u);

  memset(&fake, 0, sizeof(fake));
  fake.results[0] = SMART_BAND_PLATFORM_UNAVAILABLE;
  fake.result_count = 1u;
  CHECK(smart_band_lvgl_set_haptic_adapter_for_test(&adapter));
  input = make_notification(
    504u, SMART_BAND_NOTIFICATION_TYPE_APP,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Platform", "Fallback",
    "Structured simulator marker");
  CHECK(smart_band_lvgl_post_notification_external(&input, 250u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(fake.play_calls == 1u && logger.haptic_lines == 1u &&
        logger.wake_lines == 4u);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.haptic_events == 4u &&
        diagnostics.wake_requests == 4u &&
        diagnostics.last_haptic_platform_result ==
          SMART_BAND_PLATFORM_UNAVAILABLE);

  logger.fail = true;
  input = make_notification(
    505u, SMART_BAND_NOTIFICATION_TYPE_APP,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Platform", "Log failure",
    "Best effort does not block ack");
  CHECK(smart_band_lvgl_post_notification_external(&input, 300u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.haptic_events == 5u &&
        diagnostics.wake_requests == 5u &&
        diagnostics.haptic_log_dropped == 1u &&
        diagnostics.wake_log_dropped == 1u);
  calls_before = fake.play_calls;
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(fake.play_calls == calls_before);

  logger.fail = false;
  memset(&fake, 0, sizeof(fake));
  fake.results[0] = SMART_BAND_PLATFORM_OK;
  fake.result_count = 1u;
  CHECK(smart_band_lvgl_set_haptic_adapter_for_test(&adapter));
  input = make_notification(
    506u, SMART_BAND_NOTIFICATION_TYPE_SMS,
    SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, "Platform", "Subtle",
    "Single pulse mapping");
  CHECK(smart_band_lvgl_post_notification_external(&input, 400u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(fake.pulse_count == 1u && fake.pulses[0].on_ms == 40u &&
        fake.pulses[0].off_ms == 0u && fake.pulses[0].strength == 35u);
  input = make_notification(
    507u, SMART_BAND_NOTIFICATION_TYPE_CALL,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Phone", "Urgent",
    "Three pulse mapping");
  CHECK(smart_band_lvgl_post_notification_external(&input, 450u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(fake.pulse_count == 3u);
  CHECK(fake.pulses[0].on_ms == 120u && fake.pulses[0].off_ms == 70u &&
        fake.pulses[0].strength == 100u);
  CHECK(fake.pulses[1].on_ms == 120u && fake.pulses[1].off_ms == 70u &&
        fake.pulses[1].strength == 100u);
  CHECK(fake.pulses[2].on_ms == 120u && fake.pulses[2].off_ms == 0u &&
        fake.pulses[2].strength == 100u);

  smart_band_lvgl_destroy();
  CHECK(!smart_band_lvgl_set_haptic_adapter_for_test(&adapter));
  CHECK(!smart_band_lvgl_set_effect_logger_for_test(NULL, NULL));
  CHECK(resources_are_zero());
  return 0;
}

static int test_notification_effects_dnd_long_text_and_content_update(void)
{
  ui_tree_t tree;
  smart_band_lvgl_diagnostics_t diagnostics;
  smart_band_notification_policy_t policy = {false, false};
  char long_body[SMART_BAND_NOTIFICATION_BODY_CAPACITY * 2u];
  char truncated_body[SMART_BAND_NOTIFICATION_BODY_CAPACITY];
  smart_band_notification_input_t update;
  smart_band_notification_input_t dnd;
  lv_obj_t *body_label;
  lv_obj_t *overlay;
  uint32_t first_generation;

  memset(long_body, 'L', sizeof(long_body));
  long_body[sizeof(long_body) - 1u] = '\0';
  memset(truncated_body, 'L', sizeof(truncated_body));
  truncated_body[sizeof(truncated_body) - 1u] = '\0';
  update = make_notification(
    401u, SMART_BAND_NOTIFICATION_TYPE_APP,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Mail", "Original notice",
    "Original content");
  dnd = make_notification(
    402u, SMART_BAND_NOTIFICATION_TYPE_SYSTEM,
    SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL, "System", "DND retained",
    "Stored without presentation");

  fake_lvgl_reset();
  CHECK(!smart_band_lvgl_set_notification_policy(&policy));
  lv_obj_set_size(lv_scr_act(), 320, 480);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(!smart_band_lvgl_set_notification_policy(NULL));
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.haptic_events == 0u &&
        diagnostics.wake_requests == 0u);

  CHECK(smart_band_lvgl_post_notification_external(&update, 0u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(find_visible_text("Original notice", 0u) != NULL);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.haptic_events == 1u &&
        diagnostics.wake_requests == 1u);
  CHECK(diagnostics.last_haptic_notification_id == 401u &&
        diagnostics.last_wake_notification_id == 401u);
  CHECK(diagnostics.last_haptic == SMART_BAND_NOTIFICATION_HAPTIC_NORMAL);
  CHECK(diagnostics.last_haptic_generation != 0u &&
        diagnostics.last_haptic_generation ==
          diagnostics.last_wake_generation);
  first_generation = diagnostics.last_haptic_generation;

  update.title = "Updated notice";
  update.body = long_body;
  CHECK(smart_band_lvgl_post_notification_external(&update, 50u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(find_visible_text("Original notice", 0u) == NULL);
  CHECK(find_visible_text("Updated notice", 0u) != NULL);
  body_label = find_visible_text(truncated_body, 0u);
  CHECK(body_label != NULL);
  overlay = fake_lvgl_obj_parent(body_label);
  CHECK(overlay != NULL);
  CHECK(fake_lvgl_obj_absolute_x(body_label) >=
        fake_lvgl_obj_absolute_x(overlay));
  CHECK(fake_lvgl_obj_absolute_y(body_label) >=
        fake_lvgl_obj_absolute_y(overlay));
  CHECK(fake_lvgl_obj_absolute_x(body_label) + lv_obj_get_width(body_label) <=
        fake_lvgl_obj_absolute_x(overlay) + lv_obj_get_width(overlay));
  CHECK(fake_lvgl_obj_absolute_y(body_label) + lv_obj_get_height(body_label) <=
        fake_lvgl_obj_absolute_y(overlay) + lv_obj_get_height(overlay));
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.haptic_events == 2u &&
        diagnostics.wake_requests == 2u);
  CHECK(diagnostics.last_haptic_generation != first_generation &&
        diagnostics.last_haptic_generation ==
          diagnostics.last_wake_generation);

  CHECK(smart_band_lvgl_post_notification_external(&update, 100u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.haptic_events == 2u &&
        diagnostics.wake_requests == 2u);
  CHECK(click_object_center(find_visible_text("Dismiss", 0u)) != NULL);

  policy.dnd_enabled = true;
  CHECK(smart_band_lvgl_set_notification_policy(&policy));
  CHECK(smart_band_lvgl_post_notification_external(&dnd, 150u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(find_visible_text("DND retained", 0u) == NULL);
  CHECK(smart_band_lvgl_get_diagnostics(&diagnostics));
  CHECK(diagnostics.haptic_events == 2u &&
        diagnostics.wake_requests == 2u);

  CHECK(navigate_to_apps(&tree) == 0);
  fake_lvgl_send_event(find_visible_text("Notifications", 0u),
                       LV_EVENT_CLICKED);
  CHECK(find_visible_text("New: System / DND retained", 0u) != NULL);
  CHECK(find_visible_text("Stored without presentation", 0u) != NULL);
  CHECK(find_visible_text("Mail / Updated notice", 0u) != NULL);
  CHECK(find_visible_text(truncated_body, 0u) != NULL);

  smart_band_lvgl_destroy();
  CHECK(resources_are_zero());
  return 0;
}

static int test_notification_center_actions_and_paging(void)
{
  ui_tree_t tree;
  char titles[5][8];
  smart_band_notification_input_t input;
  lv_obj_t *launcher;
  lv_obj_t *next;
  lv_obj_t *previous;
  lv_obj_t *back;
  size_t static_objects;
  size_t static_events;
  size_t index;

  fake_lvgl_reset();
  lv_obj_set_size(lv_scr_act(), 320, 480);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  for (index = 0u; index < 5u; index++)
    {
      (void)snprintf(titles[index], sizeof(titles[index]), "N%lu",
                     (unsigned long)(index + 1u));
      input = make_notification(
        (uint32_t)(101u + index), SMART_BAND_NOTIFICATION_TYPE_APP,
        SMART_BAND_NOTIFICATION_PRIORITY_LOW, "Mail", titles[index],
        "Center only");
      CHECK(smart_band_lvgl_post_notification_external(
              &input, (uint32_t)index));
    }
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(navigate_to_apps(&tree) == 0);
  static_objects = fake_lvgl_live_object_count();
  static_events = fake_lvgl_live_event_count();
  launcher = find_visible_text("Notifications", 0u);
  CHECK(launcher != NULL);
  fake_lvgl_send_event(launcher, LV_EVENT_CLICKED);
  CHECK(find_visible_text("5 items", 0u) != NULL);
  CHECK(find_visible_text("New: Mail / N5", 0u) != NULL);

  next = find_visible_text("Next", 0u);
  CHECK(next != NULL);
  fake_lvgl_send_event(next, LV_EVENT_CLICKED);
  CHECK(find_visible_text("New: Mail / N1", 0u) != NULL);
  previous = find_visible_text("Previous", 0u);
  CHECK(previous != NULL);
  fake_lvgl_send_event(previous, LV_EVENT_CLICKED);
  CHECK(find_visible_text("New: Mail / N5", 0u) != NULL);

  CHECK(click_object_center(find_visible_text("Mark read", 0u)) != NULL);
  CHECK(find_visible_text("Mail / N5", 0u) != NULL);
  CHECK(click_object_center(find_visible_text("Delete", 0u)) != NULL);
  CHECK(find_visible_text("4 items", 0u) != NULL);
  CHECK(find_visible_text("New: Mail / N4", 0u) != NULL);

  back = find_visible_text("<", 0u);
  CHECK(back != NULL);
  fake_lvgl_send_event(back, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_live_object_count() == static_objects);
  CHECK(fake_lvgl_live_event_count() == static_events);
  smart_band_lvgl_destroy();
  CHECK(resources_are_zero());
  return 0;
}

static int test_notification_center_failure_rollback(void)
{
  ui_tree_t tree;
  lv_obj_t *launcher;
  lv_obj_t *back;
  size_t static_objects;
  size_t static_events;
  size_t create_count;
  size_t failure;

  fake_lvgl_reset();
  lv_obj_set_size(lv_scr_act(), 320, 480);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(navigate_to_apps(&tree) == 0);
  launcher = find_visible_text("Notifications", 0u);
  CHECK(launcher != NULL);
  static_objects = fake_lvgl_live_object_count();
  static_events = fake_lvgl_live_event_count();

  fake_lvgl_fail_object_create_at(0u);
  fake_lvgl_send_event(launcher, LV_EVENT_CLICKED);
  CHECK(find_visible_text("View failed", 0u) == NULL);
  create_count = fake_lvgl_object_create_attempts();
  CHECK(create_count > 0u);
  back = find_visible_text("<", 0u);
  CHECK(back != NULL);
  fake_lvgl_send_event(back, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_live_object_count() == static_objects);
  CHECK(fake_lvgl_live_event_count() == static_events);

  for (failure = 1u; failure <= create_count; failure++)
    {
      fake_lvgl_fail_object_create_at(failure);
      fake_lvgl_send_event(launcher, LV_EVENT_CLICKED);
      CHECK(find_visible_text("View failed", 0u) != NULL);
      CHECK(fake_lvgl_live_object_count() == static_objects);
      CHECK(fake_lvgl_live_event_count() == static_events);
      back = find_visible_text("<", 0u);
      CHECK(back != NULL);
      fake_lvgl_send_event(back, LV_EVENT_CLICKED);

      fake_lvgl_fail_object_create_at(0u);
      fake_lvgl_send_event(launcher, LV_EVENT_CLICKED);
      CHECK(find_visible_text("View failed", 0u) == NULL);
      back = find_visible_text("<", 0u);
      CHECK(back != NULL);
      fake_lvgl_send_event(back, LV_EVENT_CLICKED);
      CHECK(fake_lvgl_live_object_count() == static_objects);
      CHECK(fake_lvgl_live_event_count() == static_events);
    }

  smart_band_lvgl_destroy();
  CHECK(resources_are_zero());
  return 0;
}

static int test_notification_call_capture_and_backlog(void)
{
  ui_tree_t tree;
  smart_band_notification_input_t alice = make_notification(
    201u, SMART_BAND_NOTIFICATION_TYPE_CALL,
    SMART_BAND_NOTIFICATION_PRIORITY_LOW, "Phone", "Alice",
    "Incoming mobile call");
  smart_band_notification_input_t bob = make_notification(
    202u, SMART_BAND_NOTIFICATION_TYPE_CALL,
    SMART_BAND_NOTIFICATION_PRIORITY_NORMAL, "Phone", "Bob",
    "Second incoming call");
  lv_obj_t *call_layer;

  fake_lvgl_reset();
  lv_obj_set_size(lv_scr_act(), 320, 480);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(inspect_ui_tree(&tree) == 0);
  CHECK(smart_band_lvgl_post_notification_external(&alice, 0u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(find_visible_text("Alice", 0u) != NULL);
  call_layer = fake_lvgl_obj_parent(find_visible_text("Alice", 0u));
  CHECK(call_layer != NULL &&
        fake_lvgl_obj_has_flag(call_layer, LV_OBJ_FLAG_CLICKABLE));
  CHECK(lv_obj_get_width(call_layer) == lv_obj_get_width(tree.screen));
  CHECK(lv_obj_get_height(call_layer) == lv_obj_get_height(tree.screen));
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 5, 5,
                                LV_EVENT_CLICKED) == call_layer);
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 315, 5,
                                LV_EVENT_CLICKED) == call_layer);
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 5, 390,
                                LV_EVENT_CLICKED) == call_layer);
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 315, 390,
                                LV_EVENT_CLICKED) == call_layer);
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 160, 300,
                                LV_EVENT_CLICKED) == call_layer);
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 250, 300,
                                LV_EVENT_PRESSED) == call_layer);
  CHECK(fake_lvgl_send_event_at(lv_scr_act(), 50, 300,
                                LV_EVENT_RELEASED) == call_layer);
  CHECK(!fake_lvgl_obj_has_flag(tree.face, LV_OBJ_FLAG_HIDDEN));
  CHECK(fake_lvgl_obj_has_flag(tree.heart, LV_OBJ_FLAG_HIDDEN));

  CHECK(smart_band_lvgl_post_notification_external(&bob, 50u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(find_visible_text("Alice", 0u) != NULL);
  CHECK(find_visible_text("Bob", 0u) == NULL);
  CHECK(click_object_center(find_visible_text("Accept", 0u)) != NULL);
  CHECK(find_visible_text("Alice", 0u) == NULL);
  CHECK(find_visible_text("Bob", 0u) != NULL);
  CHECK(click_object_center(find_visible_text("Reject", 0u)) != NULL);
  CHECK(find_visible_text("Bob", 0u) == NULL);

  swipe_left(tree.screen);
  CHECK(!fake_lvgl_obj_has_flag(tree.heart, LV_OBJ_FLAG_HIDDEN));
  smart_band_lvgl_destroy();
  CHECK(resources_are_zero());
  return 0;
}

static int test_workout_call_overlay_is_non_blocking(void)
{
  ui_tree_t tree;
  smart_band_notification_input_t call = make_notification(
    301u, SMART_BAND_NOTIFICATION_TYPE_CALL,
    SMART_BAND_NOTIFICATION_PRIORITY_HIGH, "Phone", "Coach",
    "Workout call");
  lv_obj_t *workout;
  lv_obj_t *walk;
  lv_obj_t *pause;
  lv_obj_t *overlay;

  fake_lvgl_reset();
  lv_obj_set_size(lv_scr_act(), 320, 480);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(navigate_to_apps(&tree) == 0);
  workout = find_visible_text("Workout", 0u);
  CHECK(workout != NULL);
  fake_lvgl_send_event(workout, LV_EVENT_CLICKED);
  walk = find_visible_text("Walk", 0u);
  CHECK(walk != NULL);
  fake_lvgl_send_event(walk, LV_EVENT_CLICKED);
  (void)fake_lvgl_advance_tick(3700u);
  CHECK(find_visible_text("Active", 0u) != NULL);

  CHECK(smart_band_lvgl_post_notification_external(&call, 4000u));
  CHECK(fake_lvgl_advance_tick(50u) == 1u);
  CHECK(find_visible_text("Coach", 0u) != NULL);
  overlay = fake_lvgl_obj_parent(find_visible_text("Coach", 0u));
  CHECK(overlay != NULL &&
        lv_obj_get_width(overlay) < lv_obj_get_width(tree.screen));
  pause = find_visible_text("Pause", 0u);
  CHECK(pause != NULL);
  CHECK(click_object_center(pause) == pause);
  CHECK(find_visible_text("Paused", 0u) != NULL);
  CHECK(click_object_center(find_visible_text("Reject", 0u)) != NULL);
  CHECK(find_visible_text("Coach", 0u) == NULL);

  smart_band_lvgl_destroy();
  CHECK(resources_are_zero());
  return 0;
}

static int test_create_destroy_navigation_soak(void)
{
  static const char *const app_names[SMART_BAND_APP_COUNT] =
  {
    "Weather", "Calculator", "Timer", "2048", "Stopwatch", "Mines",
    "Tetris", "Wooden Fish"
  };
  int iteration;

  fake_lvgl_reset();
  for (iteration = 0; iteration < 1000; iteration++)
    {
      ui_tree_t tree;
      lv_obj_t *launcher;
      lv_obj_t *back;

      lv_obj_set_size(lv_scr_act(), iteration % 2 == 0 ? 320 : 667,
                      iteration % 2 == 0 ? 480 : 375);
      CHECK(smart_band_lvgl_create(NULL) == 0);
      CHECK(navigate_to_apps(&tree) == 0);
      launcher = fake_lvgl_find_text(
        tree.launcher, app_names[iteration % SMART_BAND_APP_COUNT], 0);
      CHECK(launcher != NULL);
      CHECK(absolute_y(fake_lvgl_obj_parent(launcher)) +
            lv_obj_get_height(fake_lvgl_obj_parent(launcher)) <=
            absolute_y(tree.screen) + lv_obj_get_height(tree.screen));
      fake_lvgl_send_event(launcher, LV_EVENT_CLICKED);
      CHECK(fake_lvgl_find_text(tree.apps, "App failed", 0) == NULL);
      CHECK(fake_lvgl_advance_tick(1000) == 21);
      back = fake_lvgl_find_text(tree.apps, "<", 0);
      CHECK(back != NULL);
      fake_lvgl_send_event(back, LV_EVENT_CLICKED);
      smart_band_lvgl_destroy();
      CHECK(resources_are_zero());
    }

  return 0;
}

int main(void)
{
  size_t count = 0;
  const smart_band_app_def_t *catalog = smart_band_apps_catalog(&count);

  CHECK(catalog != NULL && count == SMART_BAND_APP_COUNT);
  CHECK(test_fake_primitives() == 0);
  CHECK(test_event_mutex_contention() == 0);
  CHECK(run_object_failure_sweep(320, 480) == 0);
  CHECK(run_object_failure_sweep(667, 375) == 0);
  CHECK(test_timer_failure_retry() == 0);
  CHECK(test_event_pump_is_independent_from_runtime_tick() == 0);
  CHECK(run_app_mount_failure_sweep() == 0);
  CHECK(test_navigation_and_timer() == 0);
  CHECK(test_workout_and_history_system_views() == 0);
  CHECK(test_notification_overlay_pump_and_timeout() == 0);
  CHECK(test_notification_haptic_platform_contract() == 0);
  CHECK(test_notification_effects_dnd_long_text_and_content_update() == 0);
  CHECK(test_notification_center_actions_and_paging() == 0);
  CHECK(test_notification_center_failure_rollback() == 0);
  CHECK(test_notification_call_capture_and_backlog() == 0);
  CHECK(test_workout_call_overlay_is_non_blocking() == 0);
  CHECK(test_watch_face_mount_failure_rollback() == 0);
  CHECK(test_watch_face_picker_switch_soak() == 0);
  CHECK(test_create_destroy_navigation_soak() == 0);
  return 0;
}
