#include "history_view.h"

#include <stdio.h>
#include <string.h>

static const char *const g_fallback_day_labels
  [SMART_BAND_HISTORY_VIEW_DAY_COUNT] =
{
  "6d", "5d", "4d", "3d", "2d", "1d", "Now"
};

static lv_coord_t scaled_x(const smart_band_history_view_t *view, int value)
{
  return smart_band_ui_sx(&view->ui, value);
}

static lv_coord_t scaled_y(const smart_band_history_view_t *view, int value)
{
  return smart_band_ui_sy(&view->ui, value);
}

static lv_coord_t max_coord(lv_coord_t left, lv_coord_t right)
{
  return smart_band_ui_max(left, right);
}

static lv_obj_t *create_label(smart_band_history_view_t *view,
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

static void set_text(lv_obj_t *label, const char *text)
{
  smart_band_ui_set_label_text(label, text);
}

static void format_duration(uint64_t milliseconds, char *buffer, size_t size)
{
  uint64_t seconds = milliseconds / 1000u;

  (void)snprintf(buffer, size, "%02llu:%02llu:%02llu",
                 (unsigned long long)(seconds / 3600u),
                 (unsigned long long)((seconds / 60u) % 60u),
                 (unsigned long long)(seconds % 60u));
}

static void format_distance(uint32_t millimeters, char *buffer, size_t size)
{
  (void)snprintf(buffer, size, "%lu.%02lu km est.",
                 (unsigned long)(millimeters / 1000000u),
                 (unsigned long)((millimeters % 1000000u) / 10000u));
}

static void format_calories(uint32_t milli_kcal, char *buffer, size_t size)
{
  (void)snprintf(buffer, size, "%lu.%01lu kcal est.",
                 (unsigned long)(milli_kcal / 1000u),
                 (unsigned long)((milli_kcal % 1000u) / 100u));
}

static void format_step_value(uint32_t steps, char *buffer, size_t size)
{
  if (steps >= 10000u)
    {
      (void)snprintf(buffer, size, "%luk", (unsigned long)(steps / 1000u));
    }
  else
    {
      (void)snprintf(buffer, size, "%lu", (unsigned long)steps);
    }
}

static int create_trend(smart_band_history_view_t *view)
{
  lv_coord_t width = lv_obj_get_width(view->root);
  lv_coord_t margin = max_coord(scaled_x(view, 10), 5);
  lv_coord_t gap = max_coord(scaled_x(view, 4), 2);
  lv_coord_t chart_y = max_coord(scaled_y(view, 46), 28);
  lv_coord_t value_h = max_coord(scaled_y(view, 17), 13);
  lv_coord_t day_h = max_coord(scaled_y(view, 18), 14);
  lv_coord_t chart_width = width - margin * 2;
  lv_coord_t column_width = (chart_width - gap * 6) / 7;
  size_t index;

  view->chart_height = max_coord(scaled_y(view, 92), 56);
  view->trend_title = create_label(
    view, view->root, "7-day steps", smart_band_ui_font_16(),
    lv_color_hex(0x293b53), LV_TEXT_ALIGN_LEFT, margin, scaled_y(view, 4),
    width - margin * 2, max_coord(scaled_y(view, 24), 18));
  if (view->trend_title == NULL || column_width <= 0)
    {
      return -1;
    }

  for (index = 0; index < SMART_BAND_HISTORY_VIEW_DAY_COUNT; index++)
    {
      lv_coord_t x = margin + (lv_coord_t)index * (column_width + gap);

      view->bar_values[index] = create_label(
        view, view->root, "--", smart_band_ui_font_12(),
        lv_color_hex(0x71858d), LV_TEXT_ALIGN_CENTER, x,
        chart_y - value_h - scaled_y(view, 2), column_width, value_h);
      view->bar_tracks[index] = smart_band_ui_create_box(
        view->root, x, chart_y, column_width, view->chart_height,
        lv_color_hex(0xe4ecea), max_coord(scaled_x(view, 5), 2));
      if (view->bar_tracks[index] != NULL)
        {
          view->bar_fills[index] = smart_band_ui_create_box(
            view->bar_tracks[index], 0, view->chart_height, column_width,
            0, lv_color_hex(0x49a89f), max_coord(scaled_x(view, 5), 2));
        }
      view->bar_days[index] = create_label(
        view, view->root, g_fallback_day_labels[index],
        smart_band_ui_font_12(), lv_color_hex(0x71858d),
        LV_TEXT_ALIGN_CENTER, x,
        chart_y + view->chart_height + scaled_y(view, 3),
        column_width, day_h);
      if (view->bar_values[index] == NULL ||
          view->bar_tracks[index] == NULL ||
          view->bar_fills[index] == NULL ||
          view->bar_days[index] == NULL)
        {
          return -1;
        }
    }

  return 0;
}

static int create_session_detail(smart_band_history_view_t *view)
{
  lv_coord_t width = lv_obj_get_width(view->root);
  lv_coord_t height = lv_obj_get_height(view->root);
  lv_coord_t margin = max_coord(scaled_x(view, 12), 6);
  lv_coord_t line_h = max_coord(scaled_y(view, 22), 16);
  lv_coord_t title_y = max_coord(scaled_y(view, 184),
                                 height - line_h * 5 - scaled_y(view, 8));
  int index;

  view->session_title = create_label(
    view, view->root, "Latest workout", smart_band_ui_font_16(),
    lv_color_hex(0x293b53), LV_TEXT_ALIGN_LEFT, margin, title_y,
    width - margin * 2, line_h);
  if (view->session_title == NULL)
    {
      return -1;
    }

  for (index = 0; index < 4; index++)
    {
      view->session_lines[index] = create_label(
        view, view->root, index == 0 ? "No workouts yet" : "",
        smart_band_ui_font_12(),
        lv_color_hex(index == 3 ? 0x71858d : 0x465c65),
        LV_TEXT_ALIGN_LEFT, margin, title_y + line_h * (index + 1),
        width - margin * 2, line_h);
      if (view->session_lines[index] == NULL)
        {
          return -1;
        }
    }

  return 0;
}

int smart_band_history_view_mount(
  smart_band_history_view_t *view, lv_obj_t *parent,
  const smart_band_ui_components_t *ui)
{
  if (view == NULL || parent == NULL || ui == NULL || ui->screen_w <= 0 ||
      ui->screen_h <= 0)
    {
      return -1;
    }

  smart_band_history_view_unmount(view);
  view->ui = *ui;
  view->root = lv_obj_create(parent);
  if (view->root == NULL)
    {
      smart_band_history_view_unmount(view);
      return -1;
    }

  smart_band_ui_strip_obj(view->root);
  lv_obj_set_pos(view->root, 0, 0);
  lv_obj_set_size(view->root, lv_obj_get_width(parent),
                  lv_obj_get_height(parent));
  lv_obj_set_style_bg_opa(view->root, LV_OPA_TRANSP, 0);
  if (create_trend(view) != 0 || create_session_detail(view) != 0)
    {
      smart_band_history_view_unmount(view);
      return -1;
    }

  view->mounted = true;
  return 0;
}

void smart_band_history_view_unmount(smart_band_history_view_t *view)
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

static void render_trend(smart_band_history_view_t *view,
                         const smart_band_history_view_state_t *state)
{
  uint32_t maximum = 0u;
  size_t index;

  for (index = 0; index < SMART_BAND_HISTORY_VIEW_DAY_COUNT; index++)
    {
      if (state->days[index].present &&
          state->days[index].summary.steps > maximum)
        {
          maximum = state->days[index].summary.steps;
        }
    }
  if (maximum == 0u)
    {
      maximum = 1u;
    }

  for (index = 0; index < SMART_BAND_HISTORY_VIEW_DAY_COUNT; index++)
    {
      const smart_band_history_view_day_t *day = &state->days[index];
      lv_coord_t fill_height = 0;
      char value[16];

      if (day->present)
        {
          fill_height = (lv_coord_t)(((uint64_t)day->summary.steps *
                                      (uint64_t)view->chart_height) /
                                     maximum);
          if (fill_height == 0)
            {
              fill_height = max_coord(scaled_y(view, 3), 2);
            }
          format_step_value(day->summary.steps, value, sizeof(value));
          lv_obj_set_style_bg_opa(view->bar_fills[index], LV_OPA_COVER, 0);
          lv_obj_set_style_bg_color(
            view->bar_fills[index],
            (day->summary.flags & SMART_BAND_HISTORY_DAY_COMPLETE) != 0u ?
            lv_color_hex(0x49a89f) : lv_color_hex(0xd8a454), 0);
        }
      else
        {
          (void)snprintf(value, sizeof(value), "--");
          lv_obj_set_style_bg_opa(view->bar_fills[index], LV_OPA_TRANSP, 0);
        }

      lv_obj_set_pos(view->bar_fills[index], 0,
                     view->chart_height - fill_height);
      lv_obj_set_size(view->bar_fills[index],
                      lv_obj_get_width(view->bar_tracks[index]), fill_height);
      set_text(view->bar_values[index], value);
      set_text(view->bar_days[index],
               day->label[0] == '\0' ? g_fallback_day_labels[index] :
               day->label);
    }
}

static void render_session(smart_band_history_view_t *view,
                           const smart_band_history_view_state_t *state)
{
  const smart_band_workout_session_t *session = &state->latest_session;
  char text[64];
  char duration[24];

  if (!state->latest_session_present)
    {
      set_text(view->session_title, "Latest workout");
      set_text(view->session_lines[0], "No workouts yet");
      set_text(view->session_lines[1], "");
      set_text(view->session_lines[2], "");
      set_text(view->session_lines[3], "");
      return;
    }

  (void)snprintf(text, sizeof(text), "Latest %s",
                 session->mode == SMART_BAND_WORKOUT_MODE_RUN ?
                 "Run" : "Walk");
  set_text(view->session_title, text);
  format_duration(session->active_duration_ms, duration, sizeof(duration));
  (void)snprintf(text, sizeof(text), "%s / %lu steps", duration,
                 (unsigned long)session->steps);
  set_text(view->session_lines[0], text);
  format_distance(session->distance_mm, text, sizeof(text));
  set_text(view->session_lines[1], text);
  format_calories(session->calories_milli_kcal, text, sizeof(text));
  set_text(view->session_lines[2], text);
  if (session->heart_average_bpm != 0u)
    {
      (void)snprintf(text, sizeof(text),
                     view->ui.screen_w < 240 ? "HR %u/%u%s" :
                     "Heart %u avg / %u max%s",
                     (unsigned int)session->heart_average_bpm,
                     (unsigned int)session->heart_max_bpm,
                     (session->flags & SMART_BAND_HISTORY_SESSION_RECOVERED) !=
                       0u ? " / recovered" : "");
    }
  else
    {
      (void)snprintf(text, sizeof(text), "Heart --%s",
                     (session->flags & SMART_BAND_HISTORY_SESSION_RECOVERED) !=
                       0u ? " / recovered" : "");
    }
  set_text(view->session_lines[3], text);
}

void smart_band_history_view_render(
  smart_band_history_view_t *view,
  const smart_band_history_view_state_t *state)
{
  if (view == NULL || state == NULL || !view->mounted)
    {
      return;
    }

  render_trend(view, state);
  render_session(view, state);
}

lv_obj_t *smart_band_history_view_root(smart_band_history_view_t *view)
{
  return view == NULL ? NULL : view->root;
}
