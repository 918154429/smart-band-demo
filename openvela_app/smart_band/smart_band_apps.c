#include "smart_band_apps.h"

#include "icon_assets.h"

#include <string.h>

extern const smart_band_app_ops_t smart_band_weather_app_ops;
extern const smart_band_app_ops_t smart_band_calculator_app_ops;
extern const smart_band_app_ops_t smart_band_timer_app_ops;
extern const smart_band_app_ops_t smart_band_2048_app_ops;
extern const smart_band_app_ops_t smart_band_stopwatch_app_ops;
extern const smart_band_app_ops_t smart_band_mines_app_ops;
extern const smart_band_app_ops_t smart_band_tetris_app_ops;
extern const smart_band_app_ops_t smart_band_wooden_fish_app_ops;

static const smart_band_app_def_t g_apps[SMART_BAND_APP_COUNT] =
{
  {SMART_BAND_APP_WEATHER, "Weather", 0xf5c66e, &smart_band_icon_weather,
   0, &smart_band_weather_app_ops},
  {SMART_BAND_APP_CALCULATOR, "Calculator", 0x80cbc3,
   &smart_band_icon_calculator, 0, &smart_band_calculator_app_ops},
  {SMART_BAND_APP_TIMER, "Timer", 0xa98bd6, &smart_band_icon_timer,
   SMART_BAND_APP_TICK_WHEN_INACTIVE, &smart_band_timer_app_ops},
  {SMART_BAND_APP_2048, "2048", 0xf08d88, &smart_band_icon_game2048,
   0, &smart_band_2048_app_ops},
  {SMART_BAND_APP_STOPWATCH, "Stopwatch", 0x73a1d6,
   &smart_band_icon_stopwatch, SMART_BAND_APP_TICK_WHEN_INACTIVE,
   &smart_band_stopwatch_app_ops},
  {SMART_BAND_APP_MINES, "Mines", 0x8aa8d8, &smart_band_icon_mines,
   0, &smart_band_mines_app_ops},
  {SMART_BAND_APP_TETRIS, "Tetris", 0x62bfb6, &smart_band_icon_tetris,
   0, &smart_band_tetris_app_ops},
  {SMART_BAND_APP_WOODEN_FISH, "Wooden Fish", 0xd9a85f,
   &smart_band_icon_wooden_fish, 0, &smart_band_wooden_fish_app_ops}
};

static int app_index(smart_band_app_id_t id)
{
  size_t index;

  for (index = 0; index < SMART_BAND_APP_COUNT; index++)
    {
      if (g_apps[index].id == id)
        {
          return (int)index;
        }
    }

  return -1;
}

static void *app_context(smart_band_apps_runtime_t *runtime, int index)
{
  return runtime->contexts[index].bytes;
}

const smart_band_app_def_t *smart_band_apps_catalog(size_t *count)
{
  if (count != NULL)
    {
      *count = SMART_BAND_APP_COUNT;
    }

  return g_apps;
}

const smart_band_app_def_t *smart_band_app_find(smart_band_app_id_t id)
{
  int index = app_index(id);

  return index >= 0 ? &g_apps[index] : NULL;
}

int smart_band_apps_init(smart_band_apps_runtime_t *runtime)
{
  size_t index;

  if (runtime == NULL)
    {
      return -1;
    }

  memset(runtime, 0, sizeof(*runtime));
  runtime->active_id = SMART_BAND_APP_NONE;

  for (index = 0; index < SMART_BAND_APP_COUNT; index++)
    {
      const smart_band_app_ops_t *ops = g_apps[index].ops;

      if (ops == NULL || ops->context_size > SMART_BAND_APP_CONTEXT_CAPACITY ||
          (ops->init != NULL &&
           ops->init(app_context(runtime, (int)index)) != 0))
        {
          smart_band_apps_deinit(runtime);
          return -1;
        }
    }

  runtime->initialized = true;
  return 0;
}

void smart_band_apps_deinit(smart_band_apps_runtime_t *runtime)
{
  if (runtime == NULL)
    {
      return;
    }

  smart_band_app_unmount(runtime);
  memset(runtime, 0, sizeof(*runtime));
  runtime->active_id = SMART_BAND_APP_NONE;
}

int smart_band_app_mount(smart_band_apps_runtime_t *runtime,
                         smart_band_app_id_t id, lv_obj_t *parent,
                         const smart_band_app_host_t *host)
{
  const smart_band_app_ops_t *ops;
  int index;

  if (runtime == NULL || !runtime->initialized || parent == NULL ||
      host == NULL)
    {
      return -1;
    }

  index = app_index(id);
  if (index < 0)
    {
      return -1;
    }

  smart_band_app_unmount(runtime);
  ops = g_apps[index].ops;
  runtime->owned_root = lv_obj_create(parent);
  if (runtime->owned_root == NULL)
    {
      return -1;
    }

  lv_obj_remove_style_all(runtime->owned_root);
  lv_obj_set_pos(runtime->owned_root, 0, 0);
  lv_obj_set_size(runtime->owned_root, lv_obj_get_width(parent),
                  lv_obj_get_height(parent));
  runtime->active_id = id;
  runtime->active_visible = false;
  if (ops->mount == NULL ||
      ops->mount(app_context(runtime, index), runtime->owned_root, host) != 0)
    {
      if (ops->unmount != NULL)
        {
          ops->unmount(app_context(runtime, index));
        }

      runtime->active_id = SMART_BAND_APP_NONE;
      runtime->mounted = false;
      runtime->active_visible = false;
      if (lv_obj_is_valid(runtime->owned_root))
        {
          lv_obj_del(runtime->owned_root);
        }

      runtime->owned_root = NULL;
      return -1;
    }

  runtime->mounted = true;
  if (ops->render != NULL)
    {
      ops->render(app_context(runtime, index), host);
    }

  return 0;
}

void smart_band_app_unmount(smart_band_apps_runtime_t *runtime)
{
  int index;

  if (runtime == NULL || !runtime->mounted)
    {
      return;
    }

  index = app_index(runtime->active_id);
  if (index >= 0 && g_apps[index].ops->unmount != NULL)
    {
      g_apps[index].ops->unmount(app_context(runtime, index));
    }

  if (runtime->owned_root != NULL && lv_obj_is_valid(runtime->owned_root))
    {
      lv_obj_del(runtime->owned_root);
    }

  runtime->owned_root = NULL;
  runtime->active_id = SMART_BAND_APP_NONE;
  runtime->mounted = false;
  runtime->active_visible = false;
}

void smart_band_app_render(smart_band_apps_runtime_t *runtime,
                           const smart_band_app_host_t *host)
{
  int index;

  if (runtime == NULL || !runtime->mounted || host == NULL)
    {
      return;
    }

  index = app_index(runtime->active_id);
  if (index >= 0 && g_apps[index].ops->render != NULL)
    {
      g_apps[index].ops->render(app_context(runtime, index), host);
    }
}

void smart_band_apps_tick_at(smart_band_apps_runtime_t *runtime,
                             bool active_visible, uint32_t now_ms)
{
  size_t index;

  if (runtime == NULL || !runtime->initialized)
    {
      return;
    }

  if (runtime->mounted && active_visible != runtime->active_visible)
    {
      int active_index = app_index(runtime->active_id);

      if (active_index >= 0 &&
          g_apps[active_index].ops->set_visible != NULL)
        {
          g_apps[active_index].ops->set_visible(
            app_context(runtime, active_index), active_visible, now_ms);
        }

      runtime->active_visible = active_visible;
    }

  for (index = 0; index < SMART_BAND_APP_COUNT; index++)
    {
      const smart_band_app_def_t *def = &g_apps[index];
      bool active = active_visible && runtime->mounted &&
                    runtime->active_id == def->id;

      if (def->ops->tick == NULL ||
          (!active && (def->flags & SMART_BAND_APP_TICK_WHEN_INACTIVE) == 0))
        {
          continue;
        }

      (void)def->ops->tick(app_context(runtime, (int)index), now_ms);
    }
}

smart_band_app_id_t
smart_band_apps_active(const smart_band_apps_runtime_t *runtime)
{
  if (runtime == NULL || !runtime->mounted)
    {
      return SMART_BAND_APP_NONE;
    }

  return runtime->active_id;
}
