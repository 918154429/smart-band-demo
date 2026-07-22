#ifndef SMART_BAND_UI_LVGL_HISTORY_VIEW_H
#define SMART_BAND_UI_LVGL_HISTORY_VIEW_H

#include "components.h"
#include "smart_band_history.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_HISTORY_VIEW_DAY_COUNT 7u
#define SMART_BAND_HISTORY_VIEW_DAY_LABEL_CAPACITY 6u

typedef struct
{
  smart_band_daily_summary_t summary;
  char label[SMART_BAND_HISTORY_VIEW_DAY_LABEL_CAPACITY];
  bool present;
} smart_band_history_view_day_t;

typedef struct
{
  smart_band_history_view_day_t days[SMART_BAND_HISTORY_VIEW_DAY_COUNT];
  smart_band_workout_session_t latest_session;
  bool latest_session_present;
} smart_band_history_view_state_t;

typedef struct
{
  smart_band_ui_components_t ui;
  lv_obj_t *root;
  lv_obj_t *trend_title;
  lv_obj_t *bar_tracks[SMART_BAND_HISTORY_VIEW_DAY_COUNT];
  lv_obj_t *bar_fills[SMART_BAND_HISTORY_VIEW_DAY_COUNT];
  lv_obj_t *bar_values[SMART_BAND_HISTORY_VIEW_DAY_COUNT];
  lv_obj_t *bar_days[SMART_BAND_HISTORY_VIEW_DAY_COUNT];
  lv_obj_t *session_title;
  lv_obj_t *session_lines[4];
  lv_coord_t chart_height;
  bool mounted;
} smart_band_history_view_t;

/* The view must be zero-initialized before its first mount. */
int smart_band_history_view_mount(
  smart_band_history_view_t *view, lv_obj_t *parent,
  const smart_band_ui_components_t *ui);
void smart_band_history_view_unmount(smart_band_history_view_t *view);
void smart_band_history_view_render(
  smart_band_history_view_t *view,
  const smart_band_history_view_state_t *state);
lv_obj_t *smart_band_history_view_root(smart_band_history_view_t *view);

#ifdef __cplusplus
}
#endif

#endif
