#include "activity_face.h"

#include "../components.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
  smart_band_ui_components_t ui;
  lv_obj_t *page;
  lv_obj_t *time;
  lv_obj_t *date;
  lv_obj_t *steps;
  lv_obj_t *calories;
  lv_obj_t *heart;
  lv_obj_t *heart_zone;
  lv_obj_t *battery;
  lv_obj_t *move_bar;
  lv_obj_t *steps_bar;
  lv_obj_t *heart_bar;
} smart_band_activity_context_t;

_Static_assert(sizeof(smart_band_activity_context_t) <=
               SMART_BAND_WATCH_FACE_CONTEXT_CAPACITY,
               "Activity context exceeds watch-face storage");

#define sx(value) smart_band_ui_sx(ui, (value))
#define sy(value) smart_band_ui_sy(ui, (value))
#define strip_obj(obj) smart_band_ui_strip_obj((obj))
#define create_box smart_band_ui_create_box
#define create_label smart_band_ui_create_label
#define place_label smart_band_ui_place_label
#define set_label_text smart_band_ui_set_label_text
#define font_12 smart_band_ui_font_12
#define font_14 smart_band_ui_font_14
#define font_20 smart_band_ui_font_20
#define font_32 smart_band_ui_font_32

static lv_obj_t *create_progress(const smart_band_ui_components_t *ui,
                                 lv_obj_t *parent, lv_coord_t x,
                                 lv_coord_t y, lv_coord_t width,
                                 lv_color_t color)
{
  lv_obj_t *bar = lv_bar_create(parent);

  if (bar == NULL)
    {
      return NULL;
    }

  strip_obj(bar);
  lv_obj_set_pos(bar, x, y);
  lv_obj_set_size(bar, width, sy(12));
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x2a3035), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar, sy(6), 0);
  lv_obj_set_style_radius(bar, sy(6), LV_PART_INDICATOR);
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, 0, 0);
  return bar;
}

static void activity_unmount(void *context)
{
  smart_band_activity_context_t *activity = context;

  if (activity == NULL)
    {
      return;
    }

  if (activity->page != NULL && lv_obj_is_valid(activity->page))
    {
      lv_obj_del(activity->page);
    }

  memset(activity, 0, sizeof(*activity));
}

static int activity_mount(void *context, lv_obj_t *parent,
                          const smart_band_watch_face_config_t *config)
{
  smart_band_activity_context_t *activity = context;
  smart_band_ui_components_t *ui;
  lv_coord_t margin;
  lv_coord_t content_width;
  lv_obj_t *label;

  if (activity == NULL || parent == NULL || config == NULL ||
      config->screen_width <= 0 || config->screen_height <= 0)
    {
      return -1;
    }

  memset(activity, 0, sizeof(*activity));
  smart_band_ui_components_init(&activity->ui, config->screen_width,
                                config->screen_height);
  ui = &activity->ui;
  margin = sx(24);
  content_width = ui->screen_w - margin * 2;

  activity->page = lv_obj_create(parent);
  if (activity->page == NULL)
    {
      return -1;
    }

  strip_obj(activity->page);
  lv_obj_set_pos(activity->page, 0, 0);
  lv_obj_set_size(activity->page, ui->screen_w, ui->screen_h);
  lv_obj_set_style_bg_color(activity->page, lv_color_hex(0x111619), 0);
  lv_obj_set_style_bg_opa(activity->page, LV_OPA_COVER, 0);

  label = create_label(activity->page, "ACTIVITY", font_12(),
                       lv_color_hex(0xa7b2b8), LV_TEXT_ALIGN_LEFT);
  activity->time = create_label(activity->page, "--:--", font_32(),
                                lv_color_hex(0xf4f7f8),
                                LV_TEXT_ALIGN_LEFT);
  activity->date = create_label(activity->page, "----/--/--", font_14(),
                                lv_color_hex(0x7ed5ca),
                                LV_TEXT_ALIGN_RIGHT);
  if (label == NULL || activity->time == NULL || activity->date == NULL)
    {
      activity_unmount(activity);
      return -1;
    }

  place_label(label, margin, sy(25), content_width, sy(18));
  place_label(activity->time, margin, sy(48), sx(172), sy(52));
  place_label(activity->date, sx(180), sy(66), ui->screen_w - sx(204),
              sy(24));

  label = create_label(activity->page, "MOVE", font_12(),
                       lv_color_hex(0xff786f), LV_TEXT_ALIGN_LEFT);
  activity->calories = create_label(activity->page, "-- kcal", font_20(),
                                    lv_color_hex(0xf4f7f8),
                                    LV_TEXT_ALIGN_RIGHT);
  activity->move_bar = create_progress(ui, activity->page, margin, sy(154),
                                       content_width,
                                       lv_color_hex(0xff6258));
  if (label == NULL || activity->calories == NULL ||
      activity->move_bar == NULL)
    {
      activity_unmount(activity);
      return -1;
    }

  place_label(label, margin, sy(116), sx(88), sy(22));
  place_label(activity->calories, sx(112), sy(112),
              ui->screen_w - margin - sx(112), sy(30));

  label = create_label(activity->page, "STEPS", font_12(),
                       lv_color_hex(0x8fe36c), LV_TEXT_ALIGN_LEFT);
  activity->steps = create_label(activity->page, "-- / 8000", font_20(),
                                 lv_color_hex(0xf4f7f8),
                                 LV_TEXT_ALIGN_RIGHT);
  activity->steps_bar = create_progress(ui, activity->page, margin, sy(254),
                                        content_width,
                                        lv_color_hex(0x78d957));
  if (label == NULL || activity->steps == NULL ||
      activity->steps_bar == NULL)
    {
      activity_unmount(activity);
      return -1;
    }

  place_label(label, margin, sy(216), sx(88), sy(22));
  place_label(activity->steps, sx(104), sy(212),
              ui->screen_w - margin - sx(104), sy(30));

  label = create_label(activity->page, "HEART", font_12(),
                       lv_color_hex(0x72d5ee), LV_TEXT_ALIGN_LEFT);
  activity->heart = create_label(activity->page, "-- bpm", font_20(),
                                 lv_color_hex(0xf4f7f8),
                                 LV_TEXT_ALIGN_RIGHT);
  activity->heart_bar = create_progress(ui, activity->page, margin, sy(354),
                                        content_width,
                                        lv_color_hex(0x50c8e6));
  activity->heart_zone = create_label(activity->page, "Unavailable",
                                      font_12(), lv_color_hex(0x9da9ae),
                                      LV_TEXT_ALIGN_LEFT);
  if (label == NULL || activity->heart == NULL ||
      activity->heart_bar == NULL || activity->heart_zone == NULL)
    {
      activity_unmount(activity);
      return -1;
    }

  place_label(label, margin, sy(316), sx(88), sy(22));
  place_label(activity->heart, sx(112), sy(312),
              ui->screen_w - margin - sx(112), sy(30));
  place_label(activity->heart_zone, margin, sy(374), content_width, sy(22));

  activity->battery = create_label(activity->page, "BATTERY --%", font_14(),
                                   lv_color_hex(0xf3cf5b),
                                   LV_TEXT_ALIGN_CENTER);
  if (activity->battery == NULL)
    {
      activity_unmount(activity);
      return -1;
    }

  place_label(activity->battery, margin, sy(548), content_width, sy(26));
  return 0;
}

static void activity_render(void *context, const smart_band_state_t *model)
{
  smart_band_activity_context_t *activity = context;
  char buffer[40];
  int progress;
  int calories;
  int heart_progress;
  const char *zone;

  if (activity == NULL || activity->page == NULL || model == NULL)
    {
      return;
    }

  set_label_text(activity->time, model->time_text);
  smart_band_ui_format_watch_date(model, buffer, sizeof(buffer));
  set_label_text(activity->date, buffer);

  if (smart_band_ui_metric_available(model, SMART_BAND_METRIC_STEPS))
    {
      snprintf(buffer, sizeof(buffer), "%d / %d", model->steps,
               model->step_goal);
      calories = (model->steps * 4) / 100;
      progress = smart_band_step_progress(model);
      set_label_text(activity->steps, buffer);
      snprintf(buffer, sizeof(buffer), "%d kcal", calories);
      set_label_text(activity->calories, buffer);
      lv_bar_set_value(activity->steps_bar, progress, 0);
      lv_bar_set_value(activity->move_bar, progress, 0);
    }
  else
    {
      set_label_text(activity->steps, "-- / --");
      set_label_text(activity->calories, "-- kcal");
      lv_bar_set_value(activity->steps_bar, 0, 0);
      lv_bar_set_value(activity->move_bar, 0, 0);
    }

  if (smart_band_ui_metric_available(model, SMART_BAND_METRIC_HEART_RATE))
    {
      snprintf(buffer, sizeof(buffer), "%d bpm", model->heart_rate);
      set_label_text(activity->heart, buffer);
      if (model->heart_rate < 90)
        {
          zone = "Rest";
        }
      else if (model->heart_rate < 120)
        {
          zone = "Warm";
        }
      else if (model->heart_rate < 150)
        {
          zone = "Cardio";
        }
      else
        {
          zone = "Peak";
        }

      set_label_text(activity->heart_zone, zone);
      heart_progress = model->heart_rate > 180 ? 100 :
                       (model->heart_rate * 100) / 180;
      lv_bar_set_value(activity->heart_bar, heart_progress, 0);
    }
  else
    {
      set_label_text(activity->heart, "-- bpm");
      set_label_text(activity->heart_zone, "Unavailable");
      lv_bar_set_value(activity->heart_bar, 0, 0);
    }

  if (smart_band_ui_metric_available(model, SMART_BAND_METRIC_BATTERY))
    {
      snprintf(buffer, sizeof(buffer), "BATTERY %d%%%s",
               model->battery_percent,
               model->battery_charging ? " +" : "");
      set_label_text(activity->battery, buffer);
    }
  else
    {
      set_label_text(activity->battery, "BATTERY --%");
    }
}

static void activity_set_visible(void *context, bool visible)
{
  smart_band_activity_context_t *activity = context;

  if (activity == NULL || activity->page == NULL)
    {
      return;
    }

  if (visible)
    {
      lv_obj_clear_flag(activity->page, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_add_flag(activity->page, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *activity_root(void *context)
{
  smart_band_activity_context_t *activity = context;

  return activity == NULL ? NULL : activity->page;
}

static const smart_band_watch_face_ops_t g_activity_ops =
{
  activity_mount,
  activity_render,
  activity_set_visible,
  activity_root,
  activity_unmount
};

const smart_band_watch_face_descriptor_t smart_band_activity_face =
{
  SMART_BAND_WATCH_FACE_ACTIVITY,
  "Activity Rings",
  sizeof(smart_band_activity_context_t),
  &g_activity_ops
};
