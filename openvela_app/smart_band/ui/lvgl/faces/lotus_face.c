#include "lotus_face.h"

#include "icon_assets.h"
#include "../components.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
  smart_band_ui_components_t ui;
  lv_obj_t *page;
  lv_obj_t *date;
  lv_obj_t *hour;
  lv_obj_t *minute;
  lv_obj_t *sleep_value;
  lv_obj_t *heart_value;
  lv_obj_t *stress_value;
  lv_obj_t *weather_value;
  lv_obj_t *battery;
  lv_obj_t *battery_bar;
  lv_obj_t *battery_fill;
  lv_obj_t *battery_cap;
  lv_obj_t *battery_charge;
  bool compact_band;
} smart_band_lotus_context_t;

_Static_assert(sizeof(smart_band_lotus_context_t) <=
               SMART_BAND_WATCH_FACE_CONTEXT_CAPACITY,
               "Lotus context exceeds watch-face storage");

#define sx(value) smart_band_ui_sx(ui, (value))
#define sy(value) smart_band_ui_sy(ui, (value))
#define max_coord(a, b) smart_band_ui_max((a), (b))
#define strip_obj(obj) smart_band_ui_strip_obj((obj))
#define create_box smart_band_ui_create_box
#define create_label smart_band_ui_create_label
#define place_label smart_band_ui_place_label
#define set_label_text smart_band_ui_set_label_text
#define set_label_text_fmt_int smart_band_ui_set_label_text_fmt_int
#define create_icon_image smart_band_ui_create_icon_image
#define font_12 smart_band_ui_font_12
#define font_14 smart_band_ui_font_14
#define font_20 smart_band_ui_font_20
#define font_32 smart_band_ui_font_32
#define font_time smart_band_ui_font_time

static lv_obj_t *create_page(const smart_band_ui_components_t *ui,
                             lv_obj_t *parent)
{
  lv_obj_t *page = lv_obj_create(parent);

  if (page == NULL)
    {
      return NULL;
    }

  strip_obj(page);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_size(page, ui->screen_w, ui->screen_h);
  lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
  return page;
}

static int create_leaf_mark(const smart_band_ui_components_t *ui,
                            lv_obj_t *parent, lv_coord_t y)
{
  lv_coord_t leaf_w = sx(28);
  lv_coord_t leaf_h = sy(18);
  lv_coord_t center = ui->screen_w / 2;
  lv_obj_t *leaf;

  leaf = create_box(parent, center - sx(39), y + sy(10), leaf_w, leaf_h,
                    lv_color_hex(0x79c5be), LV_RADIUS_CIRCLE);
  if (leaf == NULL)
    {
      return -1;
    }

  leaf = create_box(parent, center - sx(11), y, leaf_w, leaf_h,
                    lv_color_hex(0x9dd6d0), LV_RADIUS_CIRCLE);
  if (leaf == NULL)
    {
      return -1;
    }

  leaf = create_box(parent, center + sx(17), y + sy(10), leaf_w, leaf_h,
                    lv_color_hex(0x79c5be), LV_RADIUS_CIRCLE);
  return leaf == NULL ? -1 : 0;
}

static int create_date_row(const smart_band_ui_components_t *ui,
                           lv_obj_t *parent, lv_obj_t **date_label,
                           lv_coord_t y)
{
  lv_coord_t dot = sx(7);
  lv_coord_t text_w = sx(180);
  lv_obj_t *left_dot;
  lv_obj_t *right_dot;

  left_dot = create_box(parent, (ui->screen_w - text_w) / 2 - sx(18),
                        y + sy(7), dot, dot, lv_color_hex(0x77c4bd),
                        LV_RADIUS_CIRCLE);
  right_dot = create_box(parent, (ui->screen_w + text_w) / 2 + sx(11),
                         y + sy(7), dot, dot, lv_color_hex(0x77c4bd),
                         LV_RADIUS_CIRCLE);
  *date_label = create_label(parent, "WED 08 JUL", font_20(),
                             lv_color_hex(0x6f8790), LV_TEXT_ALIGN_CENTER);
  if (left_dot == NULL || right_dot == NULL || *date_label == NULL)
    {
      return -1;
    }

  place_label(*date_label, (ui->screen_w - text_w) / 2, y, text_w, sy(28));
  return 0;
}

static int create_ornament(const smart_band_ui_components_t *ui,
                           lv_obj_t *parent, lv_coord_t y)
{
  lv_coord_t line_w = sx(86);
  lv_coord_t line_h = max_coord(sy(3), 2);
  lv_coord_t center = ui->screen_w / 2;
  lv_obj_t *left;
  lv_obj_t *right;
  lv_obj_t *badge;
  lv_obj_t *icon;

  left = create_box(parent, center - sx(70) - line_w, y + sy(16), line_w,
                    line_h, lv_color_hex(0xb9e0dc), LV_RADIUS_CIRCLE);
  right = create_box(parent, center + sx(70), y + sy(16), line_w, line_h,
                     lv_color_hex(0xb9e0dc), LV_RADIUS_CIRCLE);
  badge = create_box(parent, center - sx(22), y, sx(44), sy(34),
                     lv_color_hex(0xeff9f7), LV_RADIUS_CIRCLE);
  if (left == NULL || right == NULL || badge == NULL)
    {
      return -1;
    }

  icon = create_icon_image(badge, &smart_band_icon_heart, sx(7), sy(2),
                           sx(30));
  return icon == NULL ? -1 : 0;
}

static int create_metric_card(const smart_band_ui_components_t *ui,
                              lv_obj_t *parent, lv_coord_t y,
                              lv_color_t bg_color,
                              const lv_image_dsc_t *icon_src,
                              const char *label_text,
                              lv_color_t label_color, lv_obj_t **value_out)
{
  lv_coord_t margin = sx(22);
  lv_coord_t card_w = ui->screen_w - margin * 2;
  lv_coord_t card_h = sy(72);
  lv_coord_t icon_size = smart_band_ui_min(sx(48), card_h - sy(16));
  lv_coord_t value_x = sx(112);
  lv_obj_t *card;
  lv_obj_t *divider;
  lv_obj_t *icon;
  lv_obj_t *label;

  card = create_box(parent, margin, y, card_w, card_h, bg_color, sx(24));
  if (card == NULL)
    {
      return -1;
    }

  lv_obj_set_style_shadow_width(card, sx(12), 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x374a5b), 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
  lv_obj_set_style_shadow_offset_y(card, sy(6), 0);
  icon = create_icon_image(card, icon_src, sx(18),
                           (card_h - icon_size) / 2, icon_size);
  if (icon == NULL)
    {
      return -1;
    }

  divider = create_box(card, sx(88), (card_h - sy(52)) / 2, 1, sy(52),
                       lv_color_hex(0xd4dde1), 0);
  label = create_label(card, label_text, font_14(), label_color,
                       LV_TEXT_ALIGN_LEFT);
  *value_out = create_label(card, "--", font_20(), lv_color_hex(0x293b53),
                            LV_TEXT_ALIGN_LEFT);
  if (divider == NULL || label == NULL || *value_out == NULL)
    {
      return -1;
    }

  place_label(label, value_x, sy(14), card_w - value_x - sx(16), sy(20));
  place_label(*value_out, value_x, sy(38), card_w - value_x - sx(16),
              sy(28));
  return 0;
}

static void lotus_unmount(void *context)
{
  smart_band_lotus_context_t *lotus = context;

  if (lotus == NULL)
    {
      return;
    }

  if (lotus->page != NULL && lv_obj_is_valid(lotus->page))
    {
      lv_obj_del(lotus->page);
    }

  memset(lotus, 0, sizeof(*lotus));
}

static int lotus_mount(void *context, lv_obj_t *parent,
                       const smart_band_watch_face_config_t *config)
{
  smart_band_lotus_context_t *lotus = context;
  smart_band_ui_components_t *ui;
  bool compact_band;
  const lv_font_t *time_font;
  lv_coord_t time_y;
  lv_coord_t time_h;
  lv_coord_t hour_w;
  lv_coord_t minute_x;
  lv_coord_t cards_y;
  lv_coord_t card_gap;
  lv_coord_t card_h;
  lv_coord_t dot;
  lv_coord_t colon_x;
  lv_obj_t *time_row;
  lv_obj_t *colon_one;
  lv_obj_t *colon_two;
  lv_coord_t battery_x;
  lv_coord_t battery_y;
  lv_coord_t battery_w;
  lv_coord_t battery_h;

  if (lotus == NULL || parent == NULL || config == NULL ||
      config->screen_width <= 0 || config->screen_height <= 0)
    {
      return -1;
    }

  memset(lotus, 0, sizeof(*lotus));
  smart_band_ui_components_init(&lotus->ui, config->screen_width,
                                config->screen_height);
  lotus->compact_band = config->compact_band;
  ui = &lotus->ui;
  compact_band = lotus->compact_band;
  time_font = compact_band ? font_32() : font_time();
  time_y = compact_band ? sy(102) : sy(114);
  time_h = compact_band ? sy(64) : sy(78);
  hour_w = compact_band ? sx(112) : sx(122);
  minute_x = compact_band ? sx(178) : sx(184);
  cards_y = compact_band ? sy(246) : sy(260);
  card_gap = sy(12);
  card_h = sy(72);
  dot = compact_band ? sx(10) : sx(14);

  lotus->page = create_page(ui, parent);
  if (lotus->page == NULL ||
      create_leaf_mark(ui, lotus->page, sy(32)) != 0 ||
      create_date_row(ui, lotus->page, &lotus->date, sy(78)) != 0)
    {
      goto fail;
    }

  time_row = lv_obj_create(lotus->page);
  if (time_row == NULL)
    {
      goto fail;
    }

  strip_obj(time_row);
  lv_obj_set_pos(time_row, sx(22), time_y);
  lv_obj_set_size(time_row, ui->screen_w - sx(44), time_h);
  lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);
  colon_x = (ui->screen_w - sx(44)) / 2 - dot / 2;
  lotus->hour = create_label(time_row, "--", time_font,
                             lv_color_hex(0x00796c), LV_TEXT_ALIGN_RIGHT);
  lotus->minute = create_label(time_row, "--", time_font,
                               lv_color_hex(0x293b53), LV_TEXT_ALIGN_LEFT);
  colon_one = create_box(time_row, colon_x,
                         time_h / 2 - sy(compact_band ? 12 : 18),
                         dot, dot, lv_color_hex(0x79c5be), LV_RADIUS_CIRCLE);
  colon_two = create_box(time_row, colon_x,
                         time_h / 2 + sy(compact_band ? 8 : 8),
                         dot, dot, lv_color_hex(0x79c5be), LV_RADIUS_CIRCLE);
  if (lotus->hour == NULL || lotus->minute == NULL || colon_one == NULL ||
      colon_two == NULL ||
      create_ornament(ui, lotus->page,
                      compact_band ? sy(190) : sy(206)) != 0)
    {
      goto fail;
    }

  place_label(lotus->hour, 0, 0, hour_w, time_h);
  place_label(lotus->minute, minute_x, 0, hour_w, time_h);
  if (create_metric_card(ui, lotus->page, cards_y,
                         lv_color_hex(0xf2f5ff), &smart_band_icon_sleep,
                         "Sleep", lv_color_hex(0x8799cf),
                         &lotus->sleep_value) != 0 ||
      create_metric_card(ui, lotus->page, cards_y + card_h + card_gap,
                         lv_color_hex(0xfff0eb), &smart_band_icon_heart,
                         "Heart Rate", lv_color_hex(0xea7770),
                         &lotus->heart_value) != 0 ||
      create_metric_card(ui, lotus->page,
                         cards_y + (card_h + card_gap) * 2,
                         lv_color_hex(0xeefbf8), &smart_band_icon_stress,
                         "Stress", lv_color_hex(0x43a79e),
                         &lotus->stress_value) != 0 ||
      create_metric_card(ui, lotus->page,
                         cards_y + (card_h + card_gap) * 3,
                         lv_color_hex(0xfff6e2), &smart_band_icon_weather,
                         "Weather", lv_color_hex(0xe8ae46),
                         &lotus->weather_value) != 0)
    {
      goto fail;
    }

  lotus->battery = create_label(lotus->page, "--%", font_14(),
                                lv_color_hex(0x1f3438), LV_TEXT_ALIGN_LEFT);
  lotus->battery_bar =
    create_box(lotus->page, 0, 0, sx(1), sy(1), lv_color_hex(0xffffff),
               sx(4));
  lotus->battery_fill =
    create_box(lotus->battery_bar, 0, 0, sx(1), sy(1),
               lv_color_hex(0x6ccbc0), sx(2));
  lotus->battery_charge =
    create_label(lotus->battery_bar, LV_SYMBOL_CHARGE, font_12(),
                 lv_color_hex(0x1d3a34), LV_TEXT_ALIGN_CENTER);
  if (lotus->battery == NULL || lotus->battery_bar == NULL ||
      lotus->battery_fill == NULL || lotus->battery_charge == NULL)
    {
      goto fail;
    }

  battery_w = sx(compact_band ? 38 : 42);
  battery_h = max_coord(sy(compact_band ? 16 : 18), 14);
  battery_x = ui->screen_w - sx(compact_band ? 100 : 110);
  battery_y = compact_band ? sy(18) : sy(22);
  lv_obj_set_pos(lotus->battery_bar, battery_x, battery_y);
  lv_obj_set_size(lotus->battery_bar, battery_w, battery_h);
  lv_obj_set_style_bg_color(lotus->battery_bar, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(lotus->battery_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(lotus->battery_bar, 2, 0);
  lv_obj_set_style_border_color(lotus->battery_bar,
                                lv_color_hex(0x1f3438), 0);
  lv_obj_set_style_pad_all(lotus->battery_bar, 0, 0);
  lv_obj_set_pos(lotus->battery_fill, sx(3), sy(3));
  lv_obj_set_size(lotus->battery_fill, battery_w - sx(6),
                  battery_h - sy(6));
  place_label(lotus->battery_charge, 0, 0, battery_w, battery_h);
  lotus->battery_cap =
    create_box(lotus->page, battery_x + battery_w + sx(2),
               battery_y + (battery_h - sy(8)) / 2, sx(4), sy(8),
               lv_color_hex(0x1f3438), sx(2));
  if (lotus->battery_cap == NULL)
    {
      goto fail;
    }

  place_label(lotus->battery, battery_x + battery_w + sx(10),
              battery_y - sy(1), sx(58), battery_h + sy(4));
  lv_obj_add_flag(lotus->battery_charge, LV_OBJ_FLAG_HIDDEN);
  return 0;

fail:
  lotus_unmount(lotus);
  return -1;
}

static void lotus_render(void *context, const smart_band_state_t *model)
{
  smart_band_lotus_context_t *lotus = context;
  const smart_band_ui_components_t *ui;
  char hour[3];
  char minute[3];
  char date_text[20];
  char value[32];
  lv_coord_t battery_w;
  lv_coord_t battery_h;
  lv_coord_t battery_pad_x;
  lv_coord_t battery_pad_y;
  lv_coord_t fill_w;
  lv_coord_t fill_h;
  lv_color_t battery_color;
  bool battery_available;

  if (lotus == NULL || lotus->page == NULL || model == NULL)
    {
      return;
    }

  ui = &lotus->ui;
  battery_available = smart_band_ui_metric_available(
    model, SMART_BAND_METRIC_BATTERY);
  smart_band_ui_split_time_text(model, hour, sizeof(hour), minute,
                                sizeof(minute));
  smart_band_ui_format_watch_date(model, date_text, sizeof(date_text));
  set_label_text(lotus->hour, hour);
  set_label_text(lotus->minute, minute);
  set_label_text(lotus->date, date_text);
  set_label_text(lotus->sleep_value, "7h 48m");
  if (smart_band_ui_metric_available(model, SMART_BAND_METRIC_HEART_RATE))
    {
      set_label_text_fmt_int(lotus->heart_value, "%d bpm",
                             model->heart_rate);
    }
  else
    {
      set_label_text(lotus->heart_value, "-- bpm");
    }

  set_label_text(lotus->stress_value, "Low");
  smart_band_ui_set_temperature_label(model, lotus->weather_value);
  if (battery_available)
    {
      snprintf(value, sizeof(value), "%d%%", model->battery_percent);
    }
  else
    {
      snprintf(value, sizeof(value), "--");
    }

  set_label_text(lotus->battery, value);
  battery_w = sx(lotus->compact_band ? 38 : 42);
  battery_h = max_coord(sy(lotus->compact_band ? 16 : 18), 14);
  battery_pad_x = sx(3);
  battery_pad_y = sy(3);
  fill_h = max_coord(battery_h - battery_pad_y * 2, 2);
  fill_w = battery_available ?
           ((battery_w - battery_pad_x * 2) * model->battery_percent) / 100 :
           0;
  if (battery_available && model->battery_percent > 0 && fill_w < 2)
    {
      fill_w = 2;
    }

  battery_color = !battery_available ?
                  lv_color_hex(0xc5d0d3) :
                  (model->battery_percent <= 20 ?
                   lv_color_hex(0xea7770) :
                   (model->battery_charging ?
                    lv_color_hex(0x6cd66f) : lv_color_hex(0x6ccbc0)));
  lv_obj_set_size(lotus->battery_fill, fill_w, fill_h);
  lv_obj_set_style_bg_color(lotus->battery_fill, battery_color, 0);
  if (battery_available && model->battery_charging)
    {
      lv_obj_clear_flag(lotus->battery_charge, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_add_flag(lotus->battery_charge, LV_OBJ_FLAG_HIDDEN);
    }
}

static void lotus_set_visible(void *context, bool visible)
{
  smart_band_lotus_context_t *lotus = context;

  if (lotus == NULL || lotus->page == NULL)
    {
      return;
    }

  if (visible)
    {
      lv_obj_clear_flag(lotus->page, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_add_flag(lotus->page, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *lotus_root(void *context)
{
  smart_band_lotus_context_t *lotus = context;

  return lotus == NULL ? NULL : lotus->page;
}

static const smart_band_watch_face_ops_t g_lotus_ops =
{
  lotus_mount,
  lotus_render,
  lotus_set_visible,
  lotus_root,
  lotus_unmount
};

const smart_band_watch_face_descriptor_t smart_band_lotus_face =
{
  SMART_BAND_WATCH_FACE_LOTUS,
  "Lotus",
  sizeof(smart_band_lotus_context_t),
  &g_lotus_ops
};
