#include "minimal_face.h"

#include "../components.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
  smart_band_ui_components_t ui;
  lv_obj_t *page;
  lv_obj_t *time;
  lv_obj_t *date;
  lv_obj_t *battery;
  lv_obj_t *battery_bar;
  lv_obj_t *status;
} smart_band_minimal_context_t;

_Static_assert(sizeof(smart_band_minimal_context_t) <=
               SMART_BAND_WATCH_FACE_CONTEXT_CAPACITY,
               "Minimal context exceeds watch-face storage");

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
#define font_time smart_band_ui_font_time

static void minimal_unmount(void *context)
{
  smart_band_minimal_context_t *minimal = context;

  if (minimal == NULL)
    {
      return;
    }

  if (minimal->page != NULL && lv_obj_is_valid(minimal->page))
    {
      lv_obj_del(minimal->page);
    }

  memset(minimal, 0, sizeof(*minimal));
}

static int minimal_mount(void *context, lv_obj_t *parent,
                         const smart_band_watch_face_config_t *config)
{
  smart_band_minimal_context_t *minimal = context;
  smart_band_ui_components_t *ui;
  lv_coord_t margin;
  lv_coord_t content_width;
  lv_obj_t *label;
  lv_obj_t *accent;

  if (minimal == NULL || parent == NULL || config == NULL ||
      config->screen_width <= 0 || config->screen_height <= 0)
    {
      return -1;
    }

  memset(minimal, 0, sizeof(*minimal));
  smart_band_ui_components_init(&minimal->ui, config->screen_width,
                                config->screen_height);
  ui = &minimal->ui;
  margin = sx(28);
  content_width = ui->screen_w - margin * 2;

  minimal->page = lv_obj_create(parent);
  if (minimal->page == NULL)
    {
      return -1;
    }

  strip_obj(minimal->page);
  lv_obj_set_pos(minimal->page, 0, 0);
  lv_obj_set_size(minimal->page, ui->screen_w, ui->screen_h);
  lv_obj_set_style_bg_color(minimal->page, lv_color_hex(0xf7f9f8), 0);
  lv_obj_set_style_bg_opa(minimal->page, LV_OPA_COVER, 0);

  label = create_label(minimal->page, "MINIMAL", font_12(),
                       lv_color_hex(0x66747a), LV_TEXT_ALIGN_LEFT);
  accent = create_box(minimal->page, margin, sy(54), sx(48), sy(6),
                      lv_color_hex(0xf3cf54), sy(3));
  minimal->time = create_label(minimal->page, "--:--", font_time(),
                               lv_color_hex(0x182225),
                               LV_TEXT_ALIGN_LEFT);
  minimal->date = create_label(minimal->page, "----/--/--", font_20(),
                               lv_color_hex(0x3a6d75),
                               LV_TEXT_ALIGN_LEFT);
  if (label == NULL || accent == NULL || minimal->time == NULL ||
      minimal->date == NULL)
    {
      minimal_unmount(minimal);
      return -1;
    }

  place_label(label, margin, sy(25), content_width, sy(20));
  place_label(minimal->time, margin, sy(116), content_width, sy(82));
  place_label(minimal->date, margin, sy(208), content_width, sy(34));

  label = create_label(minimal->page, "BATTERY", font_12(),
                       lv_color_hex(0x66747a), LV_TEXT_ALIGN_LEFT);
  minimal->battery = create_label(minimal->page, "--%", font_20(),
                                  lv_color_hex(0x182225),
                                  LV_TEXT_ALIGN_RIGHT);
  minimal->battery_bar = lv_bar_create(minimal->page);
  if (label == NULL || minimal->battery == NULL ||
      minimal->battery_bar == NULL)
    {
      minimal_unmount(minimal);
      return -1;
    }

  place_label(label, margin, sy(354), sx(90), sy(22));
  place_label(minimal->battery, sx(196), sy(348),
              ui->screen_w - margin - sx(196), sy(32));
  strip_obj(minimal->battery_bar);
  lv_obj_set_pos(minimal->battery_bar, margin, sy(392));
  lv_obj_set_size(minimal->battery_bar, content_width, sy(14));
  lv_obj_set_style_bg_color(minimal->battery_bar, lv_color_hex(0xdce5e4), 0);
  lv_obj_set_style_bg_opa(minimal->battery_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(minimal->battery_bar, lv_color_hex(0x2caea0),
                            LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(minimal->battery_bar, LV_OPA_COVER,
                          LV_PART_INDICATOR);
  lv_obj_set_style_radius(minimal->battery_bar, sy(7), 0);
  lv_obj_set_style_radius(minimal->battery_bar, sy(7), LV_PART_INDICATOR);
  lv_bar_set_range(minimal->battery_bar, 0, 100);
  lv_bar_set_value(minimal->battery_bar, 0, 0);

  minimal->status = create_label(minimal->page, "Unavailable", font_14(),
                                 lv_color_hex(0x66747a),
                                 LV_TEXT_ALIGN_LEFT);
  if (minimal->status == NULL)
    {
      minimal_unmount(minimal);
      return -1;
    }

  place_label(minimal->status, margin, sy(526), content_width, sy(26));
  return 0;
}

static void minimal_render(void *context, const smart_band_state_t *model)
{
  smart_band_minimal_context_t *minimal = context;
  char buffer[24];

  if (minimal == NULL || minimal->page == NULL || model == NULL)
    {
      return;
    }

  set_label_text(minimal->time, model->time_text);
  smart_band_ui_format_watch_date(model, buffer, sizeof(buffer));
  set_label_text(minimal->date, buffer);
  set_label_text(minimal->status, model->status_text);

  if (smart_band_ui_metric_available(model, SMART_BAND_METRIC_BATTERY))
    {
      snprintf(buffer, sizeof(buffer), "%d%%%s", model->battery_percent,
               model->battery_charging ? " +" : "");
      set_label_text(minimal->battery, buffer);
      lv_bar_set_value(minimal->battery_bar, model->battery_percent, 0);
    }
  else
    {
      set_label_text(minimal->battery, "--%");
      lv_bar_set_value(minimal->battery_bar, 0, 0);
    }
}

static void minimal_set_visible(void *context, bool visible)
{
  smart_band_minimal_context_t *minimal = context;

  if (minimal == NULL || minimal->page == NULL)
    {
      return;
    }

  if (visible)
    {
      lv_obj_clear_flag(minimal->page, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_add_flag(minimal->page, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *minimal_root(void *context)
{
  smart_band_minimal_context_t *minimal = context;

  return minimal == NULL ? NULL : minimal->page;
}

static const smart_band_watch_face_ops_t g_minimal_ops =
{
  minimal_mount,
  minimal_render,
  minimal_set_visible,
  minimal_root,
  minimal_unmount
};

const smart_band_watch_face_descriptor_t smart_band_minimal_face =
{
  SMART_BAND_WATCH_FACE_MINIMAL,
  "Minimal Digital",
  sizeof(smart_band_minimal_context_t),
  &g_minimal_ops
};
