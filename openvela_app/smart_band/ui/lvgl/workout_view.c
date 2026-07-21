#include "workout_view.h"

#include <stdio.h>
#include <string.h>

#define WORKOUT_METRIC_STEPS 0
#define WORKOUT_METRIC_DISTANCE 1
#define WORKOUT_METRIC_CALORIES 2
#define WORKOUT_METRIC_HEART 3

static lv_coord_t scaled_x(const smart_band_workout_view_t *view, int value)
{
  return smart_band_ui_sx(&view->ui, value);
}

static lv_coord_t scaled_y(const smart_band_workout_view_t *view, int value)
{
  return smart_band_ui_sy(&view->ui, value);
}

static lv_coord_t max_coord(lv_coord_t left, lv_coord_t right)
{
  return smart_band_ui_max(left, right);
}

static void set_visible(lv_obj_t *object, bool visible)
{
  if (object == NULL)
    {
      return;
    }

  if (visible)
    {
      lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_text(lv_obj_t *label, const char *text)
{
  smart_band_ui_set_label_text(label, text);
}

static void set_button_text(lv_obj_t *button, const char *text)
{
  lv_obj_t *label = button == NULL ? NULL : lv_obj_get_child(button, 0);

  if (label != NULL)
    {
      lv_label_set_text(label, text);
    }
}

static void use_compact_button_font(const smart_band_workout_view_t *view,
                                    lv_obj_t *button)
{
  lv_obj_t *label;

  if (view->ui.screen_w >= 240 || button == NULL)
    {
      return;
    }

  label = lv_obj_get_child(button, 0);
  if (label != NULL)
    {
      lv_obj_set_style_text_font(label, smart_band_ui_font_12(), 0);
    }
}

static lv_obj_t *create_layer(smart_band_workout_view_t *view)
{
  lv_obj_t *layer = lv_obj_create(view->root);

  if (layer != NULL)
    {
      smart_band_ui_strip_obj(layer);
      lv_obj_set_pos(layer, 0, 0);
      lv_obj_set_size(layer, lv_obj_get_width(view->root),
                      lv_obj_get_height(view->root));
      lv_obj_set_style_bg_opa(layer, LV_OPA_TRANSP, 0);
    }

  return layer;
}

static lv_obj_t *create_label(smart_band_workout_view_t *view,
                              lv_obj_t *parent, const char *text,
                              const lv_font_t *font, lv_color_t color,
                              lv_text_align_t align, lv_coord_t x,
                              lv_coord_t y, lv_coord_t width,
                              lv_coord_t height)
{
  lv_obj_t *label = smart_band_ui_create_label(parent, text, font, color,
                                                align);

  if (label != NULL)
    {
      smart_band_ui_place_label(label, x, y, width, height);
    }

  (void)view;
  return label;
}

static void emit_action(smart_band_workout_view_t *view,
                        smart_band_workout_view_action_t action)
{
  if (view != NULL && view->action_cb != NULL)
    {
      view->action_cb(view->action_context, action);
    }
}

static smart_band_workout_view_t *event_view(lv_event_t *event)
{
  return (smart_band_workout_view_t *)lv_event_get_user_data(event);
}

static void start_walk_cb(lv_event_t *event)
{
  emit_action(event_view(event), SMART_BAND_WORKOUT_VIEW_ACTION_START_WALK);
}

static void start_run_cb(lv_event_t *event)
{
  emit_action(event_view(event), SMART_BAND_WORKOUT_VIEW_ACTION_START_RUN);
}

static void session_primary_cb(lv_event_t *event)
{
  smart_band_workout_view_t *view = event_view(event);

  if (view == NULL)
    {
      return;
    }

  if (view->rendered_state == SMART_BAND_WORKOUT_STATE_ACTIVE)
    {
      emit_action(view, SMART_BAND_WORKOUT_VIEW_ACTION_PAUSE);
    }
  else if (view->rendered_state == SMART_BAND_WORKOUT_STATE_PAUSED)
    {
      emit_action(view, SMART_BAND_WORKOUT_VIEW_ACTION_RESUME);
    }
}

static void show_confirmation(smart_band_workout_view_t *view,
                              smart_band_workout_view_action_t action,
                              const char *title, const char *detail)
{
  if (view == NULL || view->confirm_layer == NULL)
    {
      return;
    }

  view->pending_action = action;
  view->confirm_visible = true;
  set_text(view->confirm_title, title);
  set_text(view->confirm_detail, detail);
  set_visible(view->confirm_layer, true);
  lv_obj_move_foreground(view->confirm_layer);
}

static void finish_request_cb(lv_event_t *event)
{
  show_confirmation(event_view(event), SMART_BAND_WORKOUT_VIEW_ACTION_FINISH,
                    "Finish workout?", "Save this session to history");
}

static void abort_request_cb(lv_event_t *event)
{
  show_confirmation(event_view(event), SMART_BAND_WORKOUT_VIEW_ACTION_ABORT,
                    "Discard workout?", "Active session will be lost");
}

static void recovery_resume_cb(lv_event_t *event)
{
  emit_action(event_view(event),
              SMART_BAND_WORKOUT_VIEW_ACTION_RECOVER_RESUME);
}

static void recovery_discard_cb(lv_event_t *event)
{
  show_confirmation(event_view(event),
                    SMART_BAND_WORKOUT_VIEW_ACTION_RECOVER_DISCARD,
                    "Discard recovery?", "Clear paused checkpoint");
}

static void summary_done_cb(lv_event_t *event)
{
  emit_action(event_view(event), SMART_BAND_WORKOUT_VIEW_ACTION_DONE);
}

static void confirmation_cancel_cb(lv_event_t *event)
{
  smart_band_workout_view_t *view = event_view(event);

  if (view != NULL)
    {
      view->confirm_visible = false;
      set_visible(view->confirm_layer, false);
    }
}

static void confirmation_accept_cb(lv_event_t *event)
{
  smart_band_workout_view_t *view = event_view(event);
  smart_band_workout_view_action_t action;

  if (view == NULL || !view->confirm_visible)
    {
      return;
    }

  action = view->pending_action;
  view->confirm_visible = false;
  set_visible(view->confirm_layer, false);
  emit_action(view, action);
}

static int create_selection_layer(smart_band_workout_view_t *view)
{
  lv_coord_t width = lv_obj_get_width(view->root);
  lv_coord_t height = lv_obj_get_height(view->root);
  lv_coord_t margin = max_coord(scaled_x(view, 18), 8);
  lv_coord_t gap = max_coord(scaled_x(view, 10), 6);
  lv_coord_t button_h = max_coord(scaled_y(view, 66), 38);
  lv_coord_t button_w = (width - margin * 2 - gap) / 2;
  lv_coord_t button_y = (height - button_h) / 2 + scaled_y(view, 28);
  lv_obj_t *title;
  lv_obj_t *hint;

  view->selection_layer = create_layer(view);
  if (view->selection_layer == NULL)
    {
      return -1;
    }

  title = create_label(view, view->selection_layer, "Choose a workout",
                       smart_band_ui_font_20(), lv_color_hex(0x293b53),
                       LV_TEXT_ALIGN_CENTER, margin, scaled_y(view, 18),
                       width - margin * 2, max_coord(scaled_y(view, 32), 24));
  hint = create_label(view, view->selection_layer,
                      "Walk or Run",
                      smart_band_ui_font_12(), lv_color_hex(0x71858d),
                      LV_TEXT_ALIGN_CENTER, margin, scaled_y(view, 58),
                      width - margin * 2, max_coord(scaled_y(view, 22), 18));
  if (title == NULL || hint == NULL ||
      smart_band_ui_create_action_button(
        &view->ui, view->selection_layer, "Walk", margin, button_y,
        button_w, button_h, lv_color_hex(0x49a89f), start_walk_cb,
        (uintptr_t)view) == NULL ||
      smart_band_ui_create_action_button(
        &view->ui, view->selection_layer, "Run", margin + button_w + gap,
        button_y, button_w, button_h, lv_color_hex(0xd87862), start_run_cb,
        (uintptr_t)view) == NULL)
    {
      return -1;
    }

  return 0;
}

static int create_metric_cell(smart_band_workout_view_t *view,
                              lv_obj_t *parent, int index,
                              const char *title, lv_coord_t x,
                              lv_coord_t y, lv_coord_t width,
                              lv_coord_t height)
{
  lv_obj_t *cell = smart_band_ui_create_box(
    parent, x, y, width, height, lv_color_hex(0xf2f6f5),
    max_coord(scaled_x(view, 10), 4));
  lv_obj_t *caption;

  if (cell == NULL)
    {
      return -1;
    }

  caption = create_label(view, cell, title, smart_band_ui_font_12(),
                         lv_color_hex(0x71858d), LV_TEXT_ALIGN_LEFT,
                         scaled_x(view, 8), scaled_y(view, 5),
                         width - scaled_x(view, 16),
                         max_coord(scaled_y(view, 17), 14));
  view->session_metric_values[index] = create_label(
    view, cell, "--", smart_band_ui_font_16(), lv_color_hex(0x293b53),
    LV_TEXT_ALIGN_LEFT, scaled_x(view, 8), scaled_y(view, 23),
    width - scaled_x(view, 16), max_coord(scaled_y(view, 24), 18));
  return caption != NULL && view->session_metric_values[index] != NULL ?
         0 : -1;
}

static int create_session_layer(smart_band_workout_view_t *view)
{
  lv_coord_t width = lv_obj_get_width(view->root);
  lv_coord_t height = lv_obj_get_height(view->root);
  lv_coord_t margin = max_coord(scaled_x(view, 12), 6);
  lv_coord_t gap = max_coord(scaled_x(view, 7), 4);
  lv_coord_t button_h = max_coord(scaled_y(view, 46), 30);
  lv_coord_t button_y = height - button_h - max_coord(scaled_y(view, 7), 4);
  lv_coord_t button_w = (width - margin * 2 - gap * 2) / 3;
  lv_coord_t metrics_y = max_coord(scaled_y(view, 82), 56);
  lv_coord_t secondary_h = max_coord(scaled_y(view, 19), 15);
  lv_coord_t secondary_y = button_y - secondary_h - max_coord(scaled_y(view, 4), 2);
  lv_coord_t metric_gap = max_coord(scaled_y(view, 6), 3);
  lv_coord_t metric_h = (secondary_y - metrics_y - metric_gap) / 2;
  lv_coord_t metric_w = (width - margin * 2 - gap) / 2;

  if (metric_h < 36)
    {
      metric_h = 36;
    }

  view->session_layer = create_layer(view);
  if (view->session_layer == NULL)
    {
      return -1;
    }

  view->session_mode = create_label(
    view, view->session_layer, "Walk", smart_band_ui_font_12(),
    lv_color_hex(0x71858d), LV_TEXT_ALIGN_LEFT, margin, scaled_y(view, 5),
    width / 2 - margin, max_coord(scaled_y(view, 19), 15));
  view->session_phase = create_label(
    view, view->session_layer, "Ready", smart_band_ui_font_12(),
    lv_color_hex(0x49a89f), LV_TEXT_ALIGN_RIGHT, width / 2,
    scaled_y(view, 5), width / 2 - margin,
    max_coord(scaled_y(view, 19), 15));
  view->session_main = create_label(
    view, view->session_layer, "00:00:00", smart_band_ui_font_32(),
    lv_color_hex(0x293b53), LV_TEXT_ALIGN_CENTER, margin,
    scaled_y(view, 24), width - margin * 2,
    max_coord(scaled_y(view, 48), 30));
  if (view->session_mode == NULL || view->session_phase == NULL ||
      view->session_main == NULL ||
      create_metric_cell(view, view->session_layer, WORKOUT_METRIC_STEPS,
                         "Steps", margin, metrics_y, metric_w, metric_h) != 0 ||
      create_metric_cell(view, view->session_layer, WORKOUT_METRIC_DISTANCE,
                         "Distance est.", margin + metric_w + gap, metrics_y,
                         metric_w, metric_h) != 0 ||
      create_metric_cell(view, view->session_layer, WORKOUT_METRIC_CALORIES,
                         "Calories est.", margin,
                         metrics_y + metric_h + metric_gap,
                         metric_w, metric_h) != 0 ||
      create_metric_cell(view, view->session_layer, WORKOUT_METRIC_HEART,
                         "Heart rate", margin + metric_w + gap,
                         metrics_y + metric_h + metric_gap,
                         metric_w, metric_h) != 0)
    {
      return -1;
    }

  view->session_secondary = create_label(
    view, view->session_layer, "Average -- / Max --",
    smart_band_ui_font_12(), lv_color_hex(0x71858d), LV_TEXT_ALIGN_CENTER,
    margin, secondary_y, width - margin * 2, secondary_h);
  view->session_primary_button = smart_band_ui_create_action_button(
    &view->ui, view->session_layer, "Pause", margin, button_y, button_w,
    button_h, lv_color_hex(0x497f9f), session_primary_cb, (uintptr_t)view);
  view->session_finish_button = smart_band_ui_create_action_button(
    &view->ui, view->session_layer, "Finish", margin + button_w + gap,
    button_y, button_w, button_h, lv_color_hex(0x49a89f),
    finish_request_cb, (uintptr_t)view);
  view->session_abort_button = smart_band_ui_create_action_button(
    &view->ui, view->session_layer, "Discard",
    margin + (button_w + gap) * 2, button_y, button_w, button_h,
    lv_color_hex(0xb86858), abort_request_cb, (uintptr_t)view);
  return view->session_secondary != NULL &&
         view->session_primary_button != NULL &&
         view->session_finish_button != NULL &&
         view->session_abort_button != NULL ? 0 : -1;
}

static int create_recovery_layer(smart_band_workout_view_t *view)
{
  lv_coord_t width = lv_obj_get_width(view->root);
  lv_coord_t height = lv_obj_get_height(view->root);
  lv_coord_t margin = max_coord(scaled_x(view, 18), 8);
  lv_coord_t gap = max_coord(scaled_x(view, 10), 6);
  lv_coord_t button_h = max_coord(scaled_y(view, 54), 34);
  lv_coord_t button_w = (width - margin * 2 - gap) / 2;
  lv_coord_t button_y = height - button_h - max_coord(scaled_y(view, 20), 10);
  lv_obj_t *title;
  lv_obj_t *detail;

  view->recovery_layer = create_layer(view);
  if (view->recovery_layer == NULL)
    {
      return -1;
    }

  title = create_label(view, view->recovery_layer, "Workout recovered",
                       smart_band_ui_font_20(), lv_color_hex(0x293b53),
                       LV_TEXT_ALIGN_CENTER, margin, scaled_y(view, 36),
                       width - margin * 2, max_coord(scaled_y(view, 32), 24));
  detail = create_label(
    view, view->recovery_layer,
    "Paused safely after restart",
    smart_band_ui_font_14(), lv_color_hex(0x71858d), LV_TEXT_ALIGN_CENTER,
    margin, scaled_y(view, 86), width - margin * 2,
    max_coord(scaled_y(view, 46), 32));
  if (title == NULL || detail == NULL ||
      smart_band_ui_create_action_button(
        &view->ui, view->recovery_layer, "Resume", margin, button_y,
        button_w, button_h, lv_color_hex(0x49a89f), recovery_resume_cb,
        (uintptr_t)view) == NULL ||
      smart_band_ui_create_action_button(
        &view->ui, view->recovery_layer, "Discard",
        margin + button_w + gap, button_y, button_w, button_h,
        lv_color_hex(0xb86858), recovery_discard_cb,
        (uintptr_t)view) == NULL)
    {
      return -1;
    }

  return 0;
}

static int create_summary_layer(smart_band_workout_view_t *view)
{
  lv_coord_t width = lv_obj_get_width(view->root);
  lv_coord_t height = lv_obj_get_height(view->root);
  lv_coord_t margin = max_coord(scaled_x(view, 16), 8);
  lv_coord_t line_h = max_coord(scaled_y(view, 24), 18);
  lv_coord_t button_h = max_coord(scaled_y(view, 48), 32);
  lv_coord_t button_y = height - button_h - max_coord(scaled_y(view, 8), 4);
  int index;

  view->summary_layer = create_layer(view);
  if (view->summary_layer == NULL)
    {
      return -1;
    }

  view->summary_mode = create_label(
    view, view->summary_layer, "Walk complete", smart_band_ui_font_20(),
    lv_color_hex(0x293b53), LV_TEXT_ALIGN_CENTER, margin, scaled_y(view, 8),
    width - margin * 2, max_coord(scaled_y(view, 30), 22));
  view->summary_main = create_label(
    view, view->summary_layer, "00:00:00", smart_band_ui_font_32(),
    lv_color_hex(0x49a89f), LV_TEXT_ALIGN_CENTER, margin,
    scaled_y(view, 42), width - margin * 2,
    max_coord(scaled_y(view, 46), 30));
  if (view->summary_mode == NULL || view->summary_main == NULL)
    {
      return -1;
    }

  for (index = 0; index < 4; index++)
    {
      view->summary_lines[index] = create_label(
        view, view->summary_layer, "--", smart_band_ui_font_14(),
        lv_color_hex(index == 3 ? 0x71858d : 0x293b53),
        LV_TEXT_ALIGN_CENTER, margin,
        scaled_y(view, 100) + line_h * index, width - margin * 2, line_h);
      if (view->summary_lines[index] == NULL)
        {
          return -1;
        }
    }

  return smart_band_ui_create_action_button(
           &view->ui, view->summary_layer, "Done", margin, button_y,
           width - margin * 2, button_h, lv_color_hex(0x49a89f),
           summary_done_cb, (uintptr_t)view) == NULL ? -1 : 0;
}

static int create_confirmation_layer(smart_band_workout_view_t *view)
{
  lv_coord_t width = lv_obj_get_width(view->root);
  lv_coord_t height = lv_obj_get_height(view->root);
  lv_coord_t margin = max_coord(scaled_x(view, 18), 8);
  lv_coord_t gap = max_coord(scaled_x(view, 10), 6);
  lv_coord_t button_h = max_coord(scaled_y(view, 50), 34);
  lv_coord_t button_w = (width - margin * 2 - gap) / 2;
  lv_coord_t button_y = height - button_h - max_coord(scaled_y(view, 28), 12);

  view->confirm_layer = create_layer(view);
  if (view->confirm_layer == NULL)
    {
      return -1;
    }

  lv_obj_set_style_bg_color(view->confirm_layer, lv_color_hex(0x182326), 0);
  lv_obj_set_style_bg_opa(view->confirm_layer, LV_OPA_COVER, 0);
  lv_obj_add_flag(view->confirm_layer, LV_OBJ_FLAG_CLICKABLE);
  view->confirm_title = create_label(
    view, view->confirm_layer, "Finish workout?", smart_band_ui_font_20(),
    lv_color_hex(0xffffff), LV_TEXT_ALIGN_CENTER, margin, scaled_y(view, 52),
    width - margin * 2, max_coord(scaled_y(view, 34), 24));
  view->confirm_detail = create_label(
    view, view->confirm_layer, "Save session to history",
    smart_band_ui_font_14(), lv_color_hex(0xc8d5d7), LV_TEXT_ALIGN_CENTER,
    margin, scaled_y(view, 100), width - margin * 2,
    max_coord(scaled_y(view, 40), 28));
  if (view->confirm_title == NULL || view->confirm_detail == NULL ||
      smart_band_ui_create_action_button(
        &view->ui, view->confirm_layer, "Keep", margin, button_y, button_w,
        button_h, lv_color_hex(0x52676d), confirmation_cancel_cb,
        (uintptr_t)view) == NULL ||
      smart_band_ui_create_action_button(
        &view->ui, view->confirm_layer, "Confirm",
        margin + button_w + gap, button_y, button_w, button_h,
        lv_color_hex(0xd87862), confirmation_accept_cb,
        (uintptr_t)view) == NULL)
    {
      return -1;
    }

  set_visible(view->confirm_layer, false);
  return 0;
}

static void format_duration(uint64_t milliseconds, char *buffer, size_t size)
{
  uint64_t seconds = milliseconds / 1000u;

  (void)snprintf(buffer, size, "%02llu:%02llu:%02llu",
                 (unsigned long long)(seconds / 3600u),
                 (unsigned long long)((seconds / 60u) % 60u),
                 (unsigned long long)(seconds % 60u));
}

static void format_distance(uint64_t millimeters, char *buffer, size_t size)
{
  (void)snprintf(buffer, size, "%llu.%02llu km",
                 (unsigned long long)(millimeters / 1000000u),
                 (unsigned long long)((millimeters % 1000000u) / 10000u));
}

static void format_calories(uint64_t milli_kcal, char *buffer, size_t size)
{
  (void)snprintf(buffer, size, "%llu.%01llu kcal",
                 (unsigned long long)(milli_kcal / 1000u),
                 (unsigned long long)((milli_kcal % 1000u) / 100u));
}

static void format_pace(const smart_band_workout_snapshot_t *snapshot,
                        char *buffer, size_t size)
{
  uint64_t pace = smart_band_workout_pace_ms_per_km(snapshot);
  uint64_t seconds;

  if (pace == 0u)
    {
      (void)snprintf(buffer, size, "Pace --");
      return;
    }

  seconds = pace / 1000u;
  (void)snprintf(buffer, size, "Pace %llu:%02llu /km",
                 (unsigned long long)(seconds / 60u),
                 (unsigned long long)(seconds % 60u));
}

static const char *mode_text(smart_band_workout_mode_t mode)
{
  return mode == SMART_BAND_WORKOUT_MODE_RUN ? "Run" : "Walk";
}

static void render_session(smart_band_workout_view_t *view,
                           const smart_band_workout_view_state_t *state)
{
  const smart_band_workout_snapshot_t *snapshot = &state->snapshot;
  char text[48];
  uint64_t average = smart_band_workout_average_heart_rate_bpm(snapshot);

  set_text(view->session_mode, mode_text(snapshot->mode));
  if (snapshot->state == SMART_BAND_WORKOUT_STATE_COUNTDOWN)
    {
      uint64_t remaining = state->countdown_duration_ms >
                           snapshot->countdown_elapsed_ms ?
                           state->countdown_duration_ms -
                           snapshot->countdown_elapsed_ms : 0u;

      (void)snprintf(text, sizeof(text), "%llu",
                     (unsigned long long)((remaining + 999u) / 1000u));
      set_text(view->session_phase, "Starting");
      set_text(view->session_main, text);
      set_visible(view->session_primary_button, false);
      set_visible(view->session_finish_button, false);
      set_button_text(view->session_abort_button, "Cancel");
      set_visible(view->session_abort_button, true);
    }
  else
    {
      format_duration(snapshot->active_duration_ms, text, sizeof(text));
      set_text(view->session_main, text);
      set_text(view->session_phase,
               snapshot->state == SMART_BAND_WORKOUT_STATE_PAUSED ?
               "Paused" : "Active");
      set_button_text(view->session_primary_button,
                      snapshot->state == SMART_BAND_WORKOUT_STATE_PAUSED ?
                      "Resume" : "Pause");
      set_button_text(view->session_abort_button, "Discard");
      set_visible(view->session_primary_button, true);
      set_visible(view->session_finish_button, true);
      set_visible(view->session_abort_button,
                  snapshot->state == SMART_BAND_WORKOUT_STATE_PAUSED);
    }

  (void)snprintf(text, sizeof(text), "%llu",
                 (unsigned long long)snapshot->steps);
  set_text(view->session_metric_values[WORKOUT_METRIC_STEPS], text);
  format_distance(snapshot->distance_mm, text, sizeof(text));
  set_text(view->session_metric_values[WORKOUT_METRIC_DISTANCE], text);
  format_calories(snapshot->calories_milli_kcal, text, sizeof(text));
  set_text(view->session_metric_values[WORKOUT_METRIC_CALORIES], text);
  if (snapshot->heart_rate_current_valid)
    {
      (void)snprintf(text, sizeof(text), "%u bpm",
                     (unsigned int)snapshot->heart_rate_current_bpm);
    }
  else
    {
      (void)snprintf(text, sizeof(text), "-- bpm");
    }
  set_text(view->session_metric_values[WORKOUT_METRIC_HEART], text);
  if (snapshot->heart_rate_aggregate_valid)
    {
      (void)snprintf(
        text, sizeof(text),
        view->ui.screen_w < 240 ? "Avg %llu / Max %u / P %lu" :
        "Average %llu / Max %u / Pauses %lu",
        (unsigned long long)average,
        (unsigned int)snapshot->heart_rate_max_bpm,
        (unsigned long)state->pause_count);
    }
  else
    {
      (void)snprintf(
        text, sizeof(text),
        view->ui.screen_w < 240 ? "Avg -- / Max -- / P %lu" :
        "Average -- / Max -- / Pauses %lu",
        (unsigned long)state->pause_count);
    }
  set_text(view->session_secondary, text);
}

static void render_summary(smart_band_workout_view_t *view,
                           const smart_band_workout_view_state_t *state)
{
  const smart_band_workout_snapshot_t *snapshot = &state->snapshot;
  char text[48];
  char distance[24];
  char calories[24];

  (void)snprintf(text, sizeof(text), "%s complete", mode_text(snapshot->mode));
  set_text(view->summary_mode, text);
  format_duration(snapshot->active_duration_ms, text, sizeof(text));
  set_text(view->summary_main, text);
  (void)snprintf(text, sizeof(text), "%llu steps",
                 (unsigned long long)snapshot->steps);
  set_text(view->summary_lines[0], text);
  format_distance(snapshot->distance_mm, distance, sizeof(distance));
  format_calories(snapshot->calories_milli_kcal, calories, sizeof(calories));
  (void)snprintf(text, sizeof(text), "%s / %s", distance, calories);
  set_text(view->summary_lines[1], text);
  if (snapshot->heart_rate_aggregate_valid)
    {
      (void)snprintf(
        text, sizeof(text), "Heart %llu avg / %u max",
        (unsigned long long)smart_band_workout_average_heart_rate_bpm(snapshot),
        (unsigned int)snapshot->heart_rate_max_bpm);
    }
  else
    {
      (void)snprintf(text, sizeof(text), "Heart -- avg / -- max");
    }
  set_text(view->summary_lines[2], text);
  format_pace(snapshot, text, sizeof(text));
  set_text(view->summary_lines[3], text);
}

int smart_band_workout_view_mount(
  smart_band_workout_view_t *view, lv_obj_t *parent,
  const smart_band_ui_components_t *ui,
  smart_band_workout_view_action_cb_t action_cb, void *action_context)
{
  if (view == NULL || parent == NULL || ui == NULL || ui->screen_w <= 0 ||
      ui->screen_h <= 0)
    {
      return -1;
    }

  smart_band_workout_view_unmount(view);
  view->ui = *ui;
  view->action_cb = action_cb;
  view->action_context = action_context;
  view->root = lv_obj_create(parent);
  if (view->root == NULL)
    {
      smart_band_workout_view_unmount(view);
      return -1;
    }

  smart_band_ui_strip_obj(view->root);
  lv_obj_set_pos(view->root, 0, 0);
  lv_obj_set_size(view->root, lv_obj_get_width(parent),
                  lv_obj_get_height(parent));
  lv_obj_set_style_bg_opa(view->root, LV_OPA_TRANSP, 0);
  if (create_selection_layer(view) != 0 || create_session_layer(view) != 0 ||
      create_recovery_layer(view) != 0 || create_summary_layer(view) != 0 ||
      create_confirmation_layer(view) != 0)
    {
      smart_band_workout_view_unmount(view);
      return -1;
    }

  use_compact_button_font(view, view->session_primary_button);
  use_compact_button_font(view, view->session_finish_button);
  use_compact_button_font(view, view->session_abort_button);

  set_visible(view->session_layer, false);
  set_visible(view->recovery_layer, false);
  set_visible(view->summary_layer, false);
  view->rendered_state = SMART_BAND_WORKOUT_STATE_IDLE;
  view->mounted = true;
  return 0;
}

void smart_band_workout_view_unmount(smart_band_workout_view_t *view)
{
  if (view == NULL)
    {
      return;
    }

  if (view->root != NULL && lv_obj_is_valid(view->root))
    {
      lv_obj_del(view->root);
    }

  (void)memset(view, 0, sizeof(*view));
}

void smart_band_workout_view_render(
  smart_band_workout_view_t *view,
  const smart_band_workout_view_state_t *state)
{
  smart_band_workout_state_t next_state;

  if (view == NULL || state == NULL || !view->mounted)
    {
      return;
    }

  next_state = state->snapshot.state;
  if (next_state != view->rendered_state)
    {
      view->confirm_visible = false;
    }
  view->rendered_state = next_state;
  set_visible(view->selection_layer, false);
  set_visible(view->session_layer, false);
  set_visible(view->recovery_layer, false);
  set_visible(view->summary_layer, false);

  switch (next_state)
    {
      case SMART_BAND_WORKOUT_STATE_COUNTDOWN:
      case SMART_BAND_WORKOUT_STATE_ACTIVE:
      case SMART_BAND_WORKOUT_STATE_PAUSED:
        render_session(view, state);
        set_visible(view->session_layer, true);
        break;
      case SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION:
        set_visible(view->recovery_layer, true);
        break;
      case SMART_BAND_WORKOUT_STATE_FINISHED:
        render_summary(view, state);
        set_visible(view->summary_layer, true);
        break;
      case SMART_BAND_WORKOUT_STATE_IDLE:
      case SMART_BAND_WORKOUT_STATE_ABORTED:
      default:
        set_visible(view->selection_layer, true);
        break;
    }

  set_visible(view->confirm_layer, view->confirm_visible);
  if (view->confirm_visible)
    {
      lv_obj_move_foreground(view->confirm_layer);
    }
}

bool smart_band_workout_view_captures_input(
  const smart_band_workout_view_t *view)
{
  return view != NULL && view->mounted &&
         (view->confirm_visible ||
          view->rendered_state ==
            SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION);
}

lv_obj_t *smart_band_workout_view_root(smart_band_workout_view_t *view)
{
  return view == NULL ? NULL : view->root;
}
