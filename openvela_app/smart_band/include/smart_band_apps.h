#ifndef SMART_BAND_APPS_H
#define SMART_BAND_APPS_H

#include "watch_model.h"

#include <lvgl/lvgl.h>
#include <stddef.h>
#include <stdint.h>

#define SMART_BAND_APP_COUNT 8
#define SMART_BAND_MINE_SIZE 6
#define SMART_BAND_MINE_CELLS (SMART_BAND_MINE_SIZE * SMART_BAND_MINE_SIZE)
#define SMART_BAND_TETRIS_ROWS 8
#define SMART_BAND_TETRIS_COLS 6

typedef enum
{
  SMART_BAND_APP_NONE = -1,
  SMART_BAND_APP_WEATHER = 0,
  SMART_BAND_APP_CALCULATOR,
  SMART_BAND_APP_TIMER,
  SMART_BAND_APP_MUSIC,
  SMART_BAND_APP_STOPWATCH,
  SMART_BAND_APP_MINES,
  SMART_BAND_APP_TETRIS,
  SMART_BAND_APP_WOODEN_FISH
} smart_band_app_id_t;

typedef struct
{
  smart_band_app_id_t id;
  const char *title;
  uint32_t color;
} smart_band_app_def_t;

typedef struct smart_band_app_host_s
{
  lv_coord_t screen_w;
  lv_coord_t screen_h;
  const smart_band_state_t *model;

  lv_coord_t (*sx)(int value);
  lv_coord_t (*sy)(int value);
  const lv_font_t *(*font_12)(void);
  const lv_font_t *(*font_14)(void);
  const lv_font_t *(*font_16)(void);
  const lv_font_t *(*font_20)(void);
  const lv_font_t *(*font_32)(void);
  const lv_font_t *(*font_time)(void);

  lv_obj_t *(*create_box)(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, lv_coord_t h, lv_color_t color,
                          lv_coord_t radius);
  lv_obj_t *(*create_label)(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, lv_color_t color,
                            lv_text_align_t align);
  lv_obj_t *(*create_action_button)(lv_obj_t *parent, const char *text,
                                    lv_coord_t x, lv_coord_t y,
                                    lv_coord_t w, lv_coord_t h,
                                    lv_color_t color, lv_event_cb_t cb,
                                    uintptr_t data);
  int (*create_app_stat)(lv_obj_t *parent, int col, int row,
                         const char *title, lv_obj_t **value_out);
  void (*place_label)(lv_obj_t *label, lv_coord_t x, lv_coord_t y,
                      lv_coord_t w, lv_coord_t h);
  void (*set_label_text)(lv_obj_t *label, const char *text);
  void (*set_label_text_fmt_int)(lv_obj_t *label, const char *fmt, int value);
  void (*format_temperature)(char *buffer, size_t size);
  void (*format_duration)(char *buffer, size_t size, int seconds);
} smart_band_app_host_t;

const smart_band_app_def_t *smart_band_apps_catalog(void);
const smart_band_app_def_t *smart_band_app_find(smart_band_app_id_t id);
int smart_band_app_build(smart_band_app_id_t id, lv_obj_t *parent,
                         const smart_band_app_host_t *host);
void smart_band_app_unmount(smart_band_app_id_t id);
void smart_band_app_update(smart_band_app_id_t id,
                           const smart_band_app_host_t *host);
void smart_band_apps_tick(smart_band_app_id_t active_id,
                          const smart_band_app_host_t *host);

#endif
