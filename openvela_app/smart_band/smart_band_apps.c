#include "smart_band_apps.h"

int smart_band_weather_app_build(lv_obj_t *parent,
                                 const smart_band_app_host_t *host);
void smart_band_weather_app_update(const smart_band_app_host_t *host);
int smart_band_calculator_app_build(lv_obj_t *parent,
                                    const smart_band_app_host_t *host);
void smart_band_calculator_app_update(const smart_band_app_host_t *host);
int smart_band_timer_app_build(lv_obj_t *parent,
                               const smart_band_app_host_t *host);
void smart_band_timer_app_update(const smart_band_app_host_t *host);
void smart_band_timer_app_tick(const smart_band_app_host_t *host);
int smart_band_music_app_build(lv_obj_t *parent,
                               const smart_band_app_host_t *host);
void smart_band_music_app_update(const smart_band_app_host_t *host);
int smart_band_stopwatch_app_build(lv_obj_t *parent,
                                   const smart_band_app_host_t *host);
void smart_band_stopwatch_app_update(const smart_band_app_host_t *host);
void smart_band_stopwatch_app_tick(const smart_band_app_host_t *host);
int smart_band_mines_app_build(lv_obj_t *parent,
                               const smart_band_app_host_t *host);
void smart_band_mines_app_update(const smart_band_app_host_t *host);
int smart_band_tetris_app_build(lv_obj_t *parent,
                                const smart_band_app_host_t *host);
void smart_band_tetris_app_update(const smart_band_app_host_t *host);
void smart_band_tetris_app_tick(const smart_band_app_host_t *host);
int smart_band_wooden_fish_app_build(lv_obj_t *parent,
                                     const smart_band_app_host_t *host);
void smart_band_wooden_fish_app_update(const smart_band_app_host_t *host);
void smart_band_wooden_fish_app_tick(const smart_band_app_host_t *host);

static const smart_band_app_def_t g_apps[SMART_BAND_APP_COUNT] =
{
  {SMART_BAND_APP_WEATHER, "Weather", "WX", 0xf5c66e},
  {SMART_BAND_APP_CALCULATOR, "Calculator", "12", 0x80cbc3},
  {SMART_BAND_APP_TIMER, "Timer", "TM", 0xa98bd6},
  {SMART_BAND_APP_MUSIC, "2048", "2K", 0xf08d88},
  {SMART_BAND_APP_STOPWATCH, "Stopwatch", "SW", 0x73a1d6},
  {SMART_BAND_APP_MINES, "Mines", "MI", 0x8aa8d8},
  {SMART_BAND_APP_TETRIS, "Tetris", "TE", 0x62bfb6},
  {SMART_BAND_APP_WOODEN_FISH, "Wooden Fish", "WF", 0xd9a85f}
};

const smart_band_app_def_t *smart_band_apps_catalog(void)
{
  return g_apps;
}

const smart_band_app_def_t *smart_band_app_find(smart_band_app_id_t id)
{
  for (int i = 0; i < SMART_BAND_APP_COUNT; i++)
    {
      if (g_apps[i].id == id)
        {
          return &g_apps[i];
        }
    }

  return NULL;
}

int smart_band_app_build(smart_band_app_id_t id, lv_obj_t *parent,
                         const smart_band_app_host_t *host)
{
  switch (id)
    {
      case SMART_BAND_APP_WEATHER:
        return smart_band_weather_app_build(parent, host);
      case SMART_BAND_APP_CALCULATOR:
        return smart_band_calculator_app_build(parent, host);
      case SMART_BAND_APP_TIMER:
        return smart_band_timer_app_build(parent, host);
      case SMART_BAND_APP_MUSIC:
        return smart_band_music_app_build(parent, host);
      case SMART_BAND_APP_STOPWATCH:
        return smart_band_stopwatch_app_build(parent, host);
      case SMART_BAND_APP_MINES:
        return smart_band_mines_app_build(parent, host);
      case SMART_BAND_APP_TETRIS:
        return smart_band_tetris_app_build(parent, host);
      case SMART_BAND_APP_WOODEN_FISH:
        return smart_band_wooden_fish_app_build(parent, host);
      default:
        return -1;
    }
}

void smart_band_app_update(smart_band_app_id_t id,
                           const smart_band_app_host_t *host)
{
  switch (id)
    {
      case SMART_BAND_APP_WEATHER:
        smart_band_weather_app_update(host);
        break;
      case SMART_BAND_APP_CALCULATOR:
        smart_band_calculator_app_update(host);
        break;
      case SMART_BAND_APP_TIMER:
        smart_band_timer_app_update(host);
        break;
      case SMART_BAND_APP_MUSIC:
        smart_band_music_app_update(host);
        break;
      case SMART_BAND_APP_STOPWATCH:
        smart_band_stopwatch_app_update(host);
        break;
      case SMART_BAND_APP_MINES:
        smart_band_mines_app_update(host);
        break;
      case SMART_BAND_APP_TETRIS:
        smart_band_tetris_app_update(host);
        break;
      case SMART_BAND_APP_WOODEN_FISH:
        smart_band_wooden_fish_app_update(host);
        break;
      default:
        break;
    }
}

void smart_band_apps_tick(smart_band_app_id_t active_id,
                          const smart_band_app_host_t *host)
{
  smart_band_timer_app_tick(host);
  smart_band_stopwatch_app_tick(host);

  if (active_id == SMART_BAND_APP_TETRIS)
    {
      smart_band_tetris_app_tick(host);
    }

  if (active_id == SMART_BAND_APP_WOODEN_FISH)
    {
      smart_band_wooden_fish_app_tick(host);
    }
}
