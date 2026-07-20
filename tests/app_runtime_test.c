#include "smart_band_apps.h"

#include "icon_assets.h"

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

struct lv_obj_s
{
  int width;
  int height;
  bool valid;
};

struct lv_font_s
{
  int unused;
};

typedef struct
{
  unsigned int init_count;
  unsigned int mount_count;
  unsigned int unmount_count;
  unsigned int tick_count;
  unsigned int render_count;
  unsigned int visibility_count;
  uint32_t visibility_at;
  bool mounted;
  bool visible;
  bool fail_mount;
} fake_app_context_t;

static bool g_render_error;
static lv_obj_t g_owned_objects[16];
static unsigned int g_owned_creates;
static unsigned int g_owned_deletes;

lv_obj_t *lv_obj_create(lv_obj_t *parent)
{
  size_t index;

  if (parent == NULL || !parent->valid)
    {
      return NULL;
    }

  for (index = 0; index < sizeof(g_owned_objects) / sizeof(g_owned_objects[0]);
       index++)
    {
      if (!g_owned_objects[index].valid)
        {
          memset(&g_owned_objects[index], 0, sizeof(g_owned_objects[index]));
          g_owned_objects[index].valid = true;
          g_owned_creates++;
          return &g_owned_objects[index];
        }
    }

  return NULL;
}

int lv_obj_get_width(lv_obj_t *object)
{
  return object->width;
}

int lv_obj_get_height(lv_obj_t *object)
{
  return object->height;
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
  object->width = width;
  object->height = height;
}

int lv_obj_is_valid(lv_obj_t *object)
{
  return object != NULL && object->valid;
}

void lv_obj_del(lv_obj_t *object)
{
  if (lv_obj_is_valid(object))
    {
      object->valid = false;
      g_owned_deletes++;
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

static int fake_init(void *context)
{
  fake_app_context_t *app = context;

  memset(app, 0, sizeof(*app));
  app->init_count++;
  return 0;
}

static int fake_mount(void *context, lv_obj_t *parent,
                      const smart_band_app_host_t *host)
{
  fake_app_context_t *app = context;

  CHECK(parent != NULL);
  CHECK(host != NULL);
  app->mount_count++;
  if (app->fail_mount)
    {
      app->fail_mount = false;
      return -1;
    }

  app->mounted = true;
  return 0;
}

static void fake_unmount(void *context)
{
  fake_app_context_t *app = context;

  app->unmount_count++;
  app->mounted = false;
}

static bool fake_tick(void *context, uint32_t now_ms)
{
  fake_app_context_t *app = context;

  (void)now_ms;
  app->tick_count++;
  return true;
}

static void fake_set_visible(void *context, bool visible, uint32_t now_ms)
{
  fake_app_context_t *app = context;

  app->visibility_count++;
  app->visibility_at = now_ms;
  app->visible = visible;
}

static void fake_render(void *context, const smart_band_app_host_t *host)
{
  fake_app_context_t *app = context;

  if (host == NULL || !app->mounted)
    {
      g_render_error = true;
      return;
    }

  app->render_count++;
}

#define FAKE_OPS(name, tick_fn, visible_fn)      \
  const smart_band_app_ops_t name =             \
  {                                              \
    .context_size = sizeof(fake_app_context_t),  \
    .init = fake_init,                           \
    .mount = fake_mount,                         \
    .unmount = fake_unmount,                     \
    .tick = tick_fn,                             \
    .set_visible = visible_fn,                   \
    .render = fake_render                        \
  }

FAKE_OPS(smart_band_weather_app_ops, NULL, NULL);
FAKE_OPS(smart_band_calculator_app_ops, NULL, NULL);
FAKE_OPS(smart_band_timer_app_ops, fake_tick, NULL);
FAKE_OPS(smart_band_2048_app_ops, NULL, NULL);
FAKE_OPS(smart_band_stopwatch_app_ops, fake_tick, NULL);
FAKE_OPS(smart_band_mines_app_ops, NULL, NULL);
FAKE_OPS(smart_band_tetris_app_ops, fake_tick, fake_set_visible);
FAKE_OPS(smart_band_wooden_fish_app_ops, fake_tick, NULL);

static fake_app_context_t *context_at(smart_band_apps_runtime_t *runtime,
                                      size_t index)
{
  return (fake_app_context_t *)runtime->contexts[index].bytes;
}

static fake_app_context_t *context_for_id(smart_band_apps_runtime_t *runtime,
                                          smart_band_app_id_t id)
{
  const smart_band_app_def_t *catalog;
  size_t count;
  size_t index;

  catalog = smart_band_apps_catalog(&count);
  for (index = 0; index < count; index++)
    {
      if (catalog[index].id == id)
        {
          return context_at(runtime, index);
        }
    }

  return NULL;
}

static int test_catalog_and_lifecycle(void)
{
  smart_band_apps_runtime_t runtime;
  smart_band_app_host_t host;
  lv_obj_t parent;
  const smart_band_app_def_t *catalog;
  size_t count = 0;
  size_t index;
  int iteration;

  memset(&host, 0, sizeof(host));
  memset(&parent, 0, sizeof(parent));
  parent.width = 330;
  parent.height = 452;
  parent.valid = true;
  memset(g_owned_objects, 0, sizeof(g_owned_objects));
  g_owned_creates = 0;
  g_owned_deletes = 0;
  CHECK(smart_band_apps_init(&runtime) == 0);
  g_render_error = false;
  CHECK(smart_band_apps_active(&runtime) == SMART_BAND_APP_NONE);

  catalog = smart_band_apps_catalog(&count);
  CHECK(catalog != NULL);
  CHECK(count == SMART_BAND_APP_COUNT);
  CHECK(smart_band_app_find(SMART_BAND_APP_2048) != NULL);
  CHECK(strcmp(smart_band_app_find(SMART_BAND_APP_2048)->title, "2048") == 0);
  CHECK(smart_band_app_find(SMART_BAND_APP_2048)->icon ==
        &smart_band_icon_game2048);

  for (iteration = 0; iteration < 1000; iteration++)
    {
      for (index = 0; index < count; index++)
        {
          CHECK(smart_band_app_mount(&runtime, catalog[index].id,
                                     &parent, &host) == 0);
          CHECK(smart_band_apps_active(&runtime) == catalog[index].id);
          smart_band_app_render(&runtime, &host);
          smart_band_app_unmount(&runtime);
          CHECK(smart_band_apps_active(&runtime) == SMART_BAND_APP_NONE);
        }
    }

  for (index = 0; index < count; index++)
    {
      fake_app_context_t *app = context_at(&runtime, index);

      CHECK(app->init_count == 1);
      CHECK(app->mount_count == 1000);
      CHECK(app->unmount_count == 1000);
      CHECK(!app->mounted);
    }

  CHECK(!g_render_error);
  CHECK(g_owned_creates == 8000);
  CHECK(g_owned_deletes == 8000);

  smart_band_apps_deinit(&runtime);
  return 0;
}

static int test_failure_rollback_and_retry(void)
{
  smart_band_apps_runtime_t runtime;
  smart_band_app_host_t host;
  lv_obj_t parent;
  fake_app_context_t *calculator;

  memset(&host, 0, sizeof(host));
  memset(&parent, 0, sizeof(parent));
  parent.width = 330;
  parent.height = 452;
  parent.valid = true;
  CHECK(smart_band_apps_init(&runtime) == 0);
  g_render_error = false;
  calculator = context_for_id(&runtime, SMART_BAND_APP_CALCULATOR);
  CHECK(calculator != NULL);
  calculator->fail_mount = true;

  CHECK(smart_band_app_mount(&runtime, SMART_BAND_APP_CALCULATOR,
                             &parent, &host) != 0);
  CHECK(smart_band_apps_active(&runtime) == SMART_BAND_APP_NONE);
  CHECK(calculator->unmount_count == 1);
  CHECK(!calculator->mounted);
  CHECK(runtime.owned_root == NULL);

  CHECK(smart_band_app_mount(&runtime, SMART_BAND_APP_CALCULATOR,
                             &parent, &host) == 0);
  CHECK(smart_band_apps_active(&runtime) == SMART_BAND_APP_CALCULATOR);
  CHECK(!g_render_error);
  smart_band_apps_deinit(&runtime);
  return 0;
}

static int test_tick_policy_and_runtime_isolation(void)
{
  smart_band_apps_runtime_t first;
  smart_band_apps_runtime_t second;
  smart_band_app_host_t host;
  lv_obj_t parent;
  fake_app_context_t *first_timer;
  fake_app_context_t *second_timer;
  fake_app_context_t *stopwatch;
  fake_app_context_t *tetris;

  memset(&host, 0, sizeof(host));
  memset(&parent, 0, sizeof(parent));
  parent.width = 330;
  parent.height = 452;
  parent.valid = true;
  CHECK(smart_band_apps_init(&first) == 0);
  CHECK(smart_band_apps_init(&second) == 0);
  g_render_error = false;
  first_timer = context_for_id(&first, SMART_BAND_APP_TIMER);
  second_timer = context_for_id(&second, SMART_BAND_APP_TIMER);
  stopwatch = context_for_id(&first, SMART_BAND_APP_STOPWATCH);
  tetris = context_for_id(&first, SMART_BAND_APP_TETRIS);
  CHECK(first_timer != NULL);
  CHECK(second_timer != NULL);
  CHECK(stopwatch != NULL);
  CHECK(tetris != NULL);

  smart_band_apps_tick_at(&first, false, 1000);
  CHECK(first_timer->tick_count == 1);
  CHECK(stopwatch->tick_count == 1);
  CHECK(tetris->tick_count == 0);
  CHECK(first_timer->render_count == 0);
  CHECK(second_timer->tick_count == 0);

  CHECK(smart_band_app_mount(&first, SMART_BAND_APP_TETRIS,
                             &parent, &host) == 0);
  smart_band_apps_tick_at(&first, true, 2000);
  CHECK(first_timer->tick_count == 2);
  CHECK(stopwatch->tick_count == 2);
  CHECK(tetris->tick_count == 1);
  CHECK(tetris->render_count == 1);
  CHECK(tetris->visibility_count == 1);
  CHECK(tetris->visible);
  CHECK(tetris->visibility_at == 2000);
  CHECK(!g_render_error);

  smart_band_apps_tick_at(&first, false, 2500);
  CHECK(first_timer->tick_count == 3);
  CHECK(stopwatch->tick_count == 3);
  CHECK(tetris->tick_count == 1);
  CHECK(tetris->visibility_count == 2);
  CHECK(!tetris->visible);
  CHECK(tetris->visibility_at == 2500);

  smart_band_app_unmount(&first);
  smart_band_apps_tick_at(&first, false, 3000);
  CHECK(first_timer->tick_count == 4);
  CHECK(stopwatch->tick_count == 4);
  CHECK(tetris->tick_count == 1);
  CHECK(tetris->render_count == 1);

  smart_band_apps_deinit(&first);
  smart_band_apps_deinit(&second);
  return 0;
}

int main(void)
{
  CHECK(test_catalog_and_lifecycle() == 0);
  CHECK(test_failure_rollback_and_retry() == 0);
  CHECK(test_tick_policy_and_runtime_isolation() == 0);
  puts("smart band app runtime production tests passed");
  return 0;
}
