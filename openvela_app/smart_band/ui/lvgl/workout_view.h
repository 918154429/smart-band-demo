#ifndef SMART_BAND_UI_LVGL_WORKOUT_VIEW_H
#define SMART_BAND_UI_LVGL_WORKOUT_VIEW_H

#include "components.h"
#include "smart_band_workout_model.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  SMART_BAND_WORKOUT_VIEW_ACTION_START_WALK = 0,
  SMART_BAND_WORKOUT_VIEW_ACTION_START_RUN,
  SMART_BAND_WORKOUT_VIEW_ACTION_PAUSE,
  SMART_BAND_WORKOUT_VIEW_ACTION_RESUME,
  SMART_BAND_WORKOUT_VIEW_ACTION_FINISH,
  SMART_BAND_WORKOUT_VIEW_ACTION_ABORT,
  SMART_BAND_WORKOUT_VIEW_ACTION_RECOVER_RESUME,
  SMART_BAND_WORKOUT_VIEW_ACTION_RECOVER_DISCARD,
  SMART_BAND_WORKOUT_VIEW_ACTION_DONE
} smart_band_workout_view_action_t;

typedef void (*smart_band_workout_view_action_cb_t)(
  void *context, smart_band_workout_view_action_t action);

typedef struct
{
  smart_band_workout_snapshot_t snapshot;
  uint64_t countdown_duration_ms;
  uint32_t pause_count;
} smart_band_workout_view_state_t;

typedef struct
{
  smart_band_ui_components_t ui;
  smart_band_workout_view_action_cb_t action_cb;
  void *action_context;
  lv_obj_t *root;
  lv_obj_t *selection_layer;
  lv_obj_t *session_layer;
  lv_obj_t *recovery_layer;
  lv_obj_t *summary_layer;
  lv_obj_t *confirm_layer;
  lv_obj_t *session_phase;
  lv_obj_t *session_mode;
  lv_obj_t *session_main;
  lv_obj_t *session_metric_values[4];
  lv_obj_t *session_secondary;
  lv_obj_t *session_primary_button;
  lv_obj_t *session_finish_button;
  lv_obj_t *session_abort_button;
  lv_obj_t *summary_mode;
  lv_obj_t *summary_main;
  lv_obj_t *summary_lines[4];
  lv_obj_t *confirm_title;
  lv_obj_t *confirm_detail;
  smart_band_workout_state_t rendered_state;
  smart_band_workout_view_action_t pending_action;
  bool mounted;
  bool confirm_visible;
} smart_band_workout_view_t;

/* The view must be zero-initialized before its first mount. */
int smart_band_workout_view_mount(
  smart_band_workout_view_t *view, lv_obj_t *parent,
  const smart_band_ui_components_t *ui,
  smart_band_workout_view_action_cb_t action_cb, void *action_context);
void smart_band_workout_view_unmount(smart_band_workout_view_t *view);
void smart_band_workout_view_render(
  smart_band_workout_view_t *view,
  const smart_band_workout_view_state_t *state);
bool smart_band_workout_view_captures_input(
  const smart_band_workout_view_t *view);
lv_obj_t *smart_band_workout_view_root(smart_band_workout_view_t *view);

#ifdef __cplusplus
}
#endif

#endif
