#include "app_lvgl.h"
#include "fake_lvgl/fake_lvgl_test.h"
#include "smart_band_apps.h"
#include "smart_band_watch_face_id.h"

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
  lv_obj_t *screen;
  lv_obj_t *face;
  lv_obj_t *picker;
  lv_obj_t *heart;
  lv_obj_t *steps;
  lv_obj_t *apps;
  lv_obj_t *launcher;
} ui_tree_t;

static unsigned int g_event_calls;
static uintptr_t g_event_data;
static unsigned int g_timer_calls;

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
         fake_lvgl_live_timer_count() == 0;
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
  return tree->face != NULL && tree->heart != NULL && tree->steps != NULL &&
         tree->apps != NULL && tree->launcher != NULL && tree->screen != NULL &&
         tree->picker != NULL ?
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
  fake_lvgl_reset();
  lv_obj_set_size(lv_scr_act(), 320, 480);
  fake_lvgl_fail_timer_create_at(1);
  CHECK(smart_band_lvgl_create(NULL) != 0);
  CHECK(resources_are_zero());
  fake_lvgl_fail_timer_create_at(0);
  CHECK(smart_band_lvgl_create(NULL) == 0);
  CHECK(fake_lvgl_live_timer_count() == 1);
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
  CHECK(fake_lvgl_advance_tick(300) == 0);
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
  CHECK(fake_lvgl_advance_tick(1000) == 1);
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
  fake_lvgl_send_event(workout, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_find_text(tree.apps, "Choose a workout", 0) != NULL);
  walk = fake_lvgl_find_text(tree.apps, "Walk", 0);
  CHECK(walk != NULL);
  fake_lvgl_send_event(walk, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_find_text(tree.apps, "Starting", 0) != NULL);
  CHECK(fake_lvgl_advance_tick(3000) == 3);
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
  CHECK(fake_lvgl_live_object_count() == static_objects);
  CHECK(fake_lvgl_live_event_count() == static_events);
  fake_lvgl_send_event(workout, LV_EVENT_CLICKED);
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
  CHECK(fake_lvgl_live_object_count() == static_objects);
  CHECK(fake_lvgl_live_event_count() == static_events);

  history = fake_lvgl_find_text(tree.launcher, "History", 0);
  CHECK(history != NULL);
  fake_lvgl_send_event(history, LV_EVENT_CLICKED);
  CHECK(fake_lvgl_find_text(tree.apps, "7-day steps", 0) != NULL);
  CHECK(fake_lvgl_find_text(tree.apps, "Latest Walk", 0) != NULL);
  back = fake_lvgl_find_text(tree.apps, "<", 0);
  CHECK(back != NULL);
  fake_lvgl_send_event(back, LV_EVENT_CLICKED);
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
      fake_lvgl_send_event(launcher, LV_EVENT_CLICKED);
      CHECK(fake_lvgl_find_text(tree.apps, "App failed", 0) == NULL);
      CHECK(fake_lvgl_advance_tick(1000) == 1);
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
  CHECK(run_object_failure_sweep(320, 480) == 0);
  CHECK(run_object_failure_sweep(667, 375) == 0);
  CHECK(test_timer_failure_retry() == 0);
  CHECK(run_app_mount_failure_sweep() == 0);
  CHECK(test_navigation_and_timer() == 0);
  CHECK(test_workout_and_history_system_views() == 0);
  CHECK(test_watch_face_mount_failure_rollback() == 0);
  CHECK(test_watch_face_picker_switch_soak() == 0);
  CHECK(test_create_destroy_navigation_soak() == 0);
  return 0;
}
