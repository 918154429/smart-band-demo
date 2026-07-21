#include "watch_pages.h"

#include "icon_assets.h"

#include <stdio.h>

#define sx(value) smart_band_ui_sx(ui, (value))
#define sy(value) smart_band_ui_sy(ui, (value))
#define min_coord(a, b) smart_band_ui_min((a), (b))
#define max_coord(a, b) smart_band_ui_max((a), (b))
#define strip_obj(obj) smart_band_ui_strip_obj((obj))
#define create_box smart_band_ui_create_box
#define create_label smart_band_ui_create_label
#define place_label smart_band_ui_place_label
#define set_label_text smart_band_ui_set_label_text
#define set_label_text_fmt_int smart_band_ui_set_label_text_fmt_int
#define create_icon_image smart_band_ui_create_icon_image
#define create_action_button(parent, text, x, y, w, h, color, cb, data) \
  smart_band_ui_create_action_button(ui, parent, text, x, y, w, h, color, \
                                     cb, data)
#define font_12 smart_band_ui_font_12
#define font_14 smart_band_ui_font_14
#define font_16 smart_band_ui_font_16
#define font_20 smart_band_ui_font_20
#define font_32 smart_band_ui_font_32
#define font_time smart_band_ui_font_time
#define metric_available(metric) smart_band_ui_metric_available(model, (metric))
#define metric_source_text(metric) smart_band_ui_metric_source_text(model, (metric))
#define set_temperature_label(label) smart_band_ui_set_temperature_label(model, (label))
#define format_watch_date(buffer, size) smart_band_ui_format_watch_date(model, (buffer), (size))
#define split_time_text(hour, hs, minute, ms) smart_band_ui_split_time_text(model, (hour), (hs), (minute), (ms))

static lv_obj_t *create_page(const smart_band_ui_components_t *ui, lv_obj_t *parent)
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

static int create_leaf_mark(const smart_band_ui_components_t *ui, lv_obj_t *parent, lv_coord_t y)
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

static int create_date_row(const smart_band_ui_components_t *ui, lv_obj_t *parent, lv_obj_t **date_label,
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

int smart_band_watch_page_build_header(lv_obj_t *parent,
                                       const smart_band_ui_components_t *ui,
                                       lv_obj_t **date_label,
                                       lv_coord_t leaf_y,
                                       lv_coord_t date_y)
{
  if (parent == NULL || ui == NULL || date_label == NULL)
    {
      return -1;
    }

  return create_leaf_mark(ui, parent, leaf_y) == 0 &&
         create_date_row(ui, parent, date_label, date_y) == 0 ? 0 : -1;
}

static lv_obj_t *create_detail_hero(const smart_band_ui_components_t *ui, lv_obj_t *page, lv_color_t hero_bg,
                                    const lv_image_dsc_t *icon_src,
                                    lv_obj_t **value_out,
                                    lv_obj_t **progress_out)
{
  lv_coord_t margin = sx(22);
  lv_coord_t hero_y = sy(154);
  lv_coord_t hero_w = ui->screen_w - margin * 2;
  lv_coord_t hero_h = sy(190);
  lv_coord_t icon_size = sx(68);
  lv_obj_t *hero;
  lv_obj_t *icon;

  hero = create_box(page, margin, hero_y, hero_w, hero_h, hero_bg, sx(32));
  if (hero == NULL)
    {
      return NULL;
    }

  lv_obj_set_style_shadow_width(hero, sx(18), 0);
  lv_obj_set_style_shadow_color(hero, lv_color_hex(0x374a5b), 0);
  lv_obj_set_style_shadow_opa(hero, LV_OPA_20, 0);
  lv_obj_set_style_shadow_offset_y(hero, sy(10), 0);

  icon = create_icon_image(hero, icon_src, (hero_w - icon_size) / 2,
                           sy(18), icon_size);
  if (icon == NULL)
    {
      return NULL;
    }

  *value_out = create_label(hero, "--", font_32(), lv_color_hex(0x293b53),
                            LV_TEXT_ALIGN_CENTER);
  *progress_out = lv_bar_create(hero);
  if (*value_out == NULL || *progress_out == NULL)
    {
      return NULL;
    }

  strip_obj(*progress_out);
  place_label(*value_out, sx(18), sy(106), hero_w - sx(36), sy(42));
  lv_obj_set_pos(*progress_out, sx(52), sy(158));
  lv_obj_set_size(*progress_out, hero_w - sx(104), sy(8));
  lv_obj_set_style_radius(*progress_out, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(*progress_out, lv_color_hex(0xdbeeea), 0);
  lv_obj_set_style_bg_opa(*progress_out, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(*progress_out, lv_color_hex(0x79c5be),
                            LV_PART_INDICATOR);
  lv_obj_set_style_radius(*progress_out, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_bar_set_range(*progress_out, 0, 100);
  return hero;
}

static int create_mini_card(const smart_band_ui_components_t *ui, lv_obj_t *page, int col, int row,
                            const char *title, lv_obj_t **value_out)
{
  lv_coord_t margin = sx(22);
  lv_coord_t gap = sx(12);
  lv_coord_t card_w = (ui->screen_w - margin * 2 - gap) / 2;
  lv_coord_t card_h = sy(76);
  lv_coord_t x = margin + col * (card_w + gap);
  lv_coord_t y = sy(374) + row * (card_h + sy(12));
  lv_obj_t *card;
  lv_obj_t *title_label;

  card = create_box(page, x, y, card_w, card_h, lv_color_hex(0xffffff),
                    sx(20));
  if (card == NULL)
    {
      return -1;
    }

  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0xe7eff0), 0);

  title_label = create_label(card, title, font_12(), lv_color_hex(0x7d9298),
                             LV_TEXT_ALIGN_LEFT);
  *value_out = create_label(card, "--", font_20(), lv_color_hex(0x293b53),
                            LV_TEXT_ALIGN_LEFT);
  if (title_label == NULL || *value_out == NULL)
    {
      return -1;
    }

  place_label(title_label, sx(14), sy(12), card_w - sx(28), sy(18));
  place_label(*value_out, sx(14), sy(36), card_w - sx(28), sy(28));
  return 0;
}

int smart_band_watch_pages_build_heart(smart_band_watch_pages_t *pages, lv_obj_t *parent, const smart_band_ui_components_t *ui)
{
  lv_obj_t *title;

  pages->heart_page = create_page(ui, parent);
  if (pages->heart_page == NULL ||
      create_leaf_mark(ui, pages->heart_page, sy(32)) != 0 ||
      create_date_row(ui, pages->heart_page, &pages->heart_date, sy(78)) != 0)
    {
      return -1;
    }

  title = create_label(pages->heart_page, "Heart Rate", font_20(),
                       lv_color_hex(0x5a7680), LV_TEXT_ALIGN_CENTER);
  if (title == NULL)
    {
      return -1;
    }

  place_label(title, sx(22), sy(112), ui->screen_w - sx(44), sy(28));

  if (create_detail_hero(ui, pages->heart_page, lv_color_hex(0xfff0eb),
                         &smart_band_icon_heart,
                         &pages->heart_value,
                         &pages->heart_progress) == NULL ||
      create_mini_card(ui, pages->heart_page, 0, 0, "Resting",
                       &pages->heart_status) != 0 ||
       create_mini_card(ui, pages->heart_page, 1, 0, "Source",
                       &pages->heart_source) != 0 ||
      create_mini_card(ui, pages->heart_page, 0, 1, "Battery",
                       &pages->heart_battery) != 0 ||
      create_mini_card(ui, pages->heart_page, 1, 1, "Stress",
                       &pages->heart_stress) != 0)
    {
      return -1;
    }

  return 0;
}

int smart_band_watch_pages_build_steps(smart_band_watch_pages_t *pages, lv_obj_t *parent, const smart_band_ui_components_t *ui, lv_event_cb_t step_goal_cb)
{
  lv_obj_t *title;

  pages->steps_page = create_page(ui, parent);
  if (pages->steps_page == NULL ||
      create_leaf_mark(ui, pages->steps_page, sy(32)) != 0 ||
      create_date_row(ui, pages->steps_page, &pages->steps_date, sy(78)) != 0)
    {
      return -1;
    }

  title = create_label(pages->steps_page, "Activity", font_20(),
                       lv_color_hex(0x5a7680), LV_TEXT_ALIGN_CENTER);
  if (title == NULL)
    {
      return -1;
    }

  place_label(title, sx(22), sy(112), ui->screen_w - sx(44), sy(28));

  if (create_detail_hero(ui, pages->steps_page, lv_color_hex(0xeefbf8),
                         &smart_band_icon_steps,
                         &pages->steps_value,
                         &pages->steps_progress) == NULL ||
      create_mini_card(ui, pages->steps_page, 0, 0, "Goal",
                       &pages->steps_goal) != 0 ||
      create_mini_card(ui, pages->steps_page, 1, 0, "Progress",
                       &pages->steps_percent) != 0 ||
      create_mini_card(ui, pages->steps_page, 0, 1, "Source",
                       &pages->steps_source) != 0 ||
      create_mini_card(ui, pages->steps_page, 1, 1, "Weather",
                       &pages->steps_weather) != 0)
    {
      return -1;
    }

  pages->steps_goal_down =
    create_action_button(pages->steps_page, "-", sx(104), sy(346), sx(42),
                         sy(24), lv_color_hex(0x8eb6d8), step_goal_cb, 0);
  pages->steps_goal_up =
    create_action_button(pages->steps_page, "+", ui->screen_w - sx(146),
                         sy(346), sx(42), sy(24), lv_color_hex(0x80cbc3),
                         step_goal_cb, 1);
  if (pages->steps_goal_down == NULL || pages->steps_goal_up == NULL)
    {
      return -1;
    }

  return 0;
}



void smart_band_watch_pages_render_heart(smart_band_watch_pages_t *pages, const smart_band_state_t *model)
{
  char date_text[20];
  char value[32];
  bool heart_available = metric_available(SMART_BAND_METRIC_HEART_RATE);
  int progress = heart_available ? (model->heart_rate * 100) / 135 : 0;

  if (progress > 100)
    {
      progress = 100;
    }

  format_watch_date(date_text, sizeof(date_text));
  if (heart_available)
    {
      snprintf(value, sizeof(value), "%d bpm", model->heart_rate);
    }
  else
    {
      snprintf(value, sizeof(value), "-- bpm");
    }

  set_label_text(pages->heart_date, date_text);
  set_label_text(pages->heart_value, value);
  lv_bar_set_value(pages->heart_progress, progress, LV_ANIM_ON);
  set_label_text(pages->heart_status, heart_available ? "62" : "--");
  set_label_text(pages->heart_source,
                 metric_source_text(SMART_BAND_METRIC_HEART_RATE));
  if (metric_available(SMART_BAND_METRIC_BATTERY))
    {
      set_label_text_fmt_int(pages->heart_battery, "%d%%",
                             model->battery_percent);
    }
  else
    {
      set_label_text(pages->heart_battery, "--");
    }
  set_label_text(pages->heart_stress, "Low");
}

void smart_band_watch_pages_render_steps(smart_band_watch_pages_t *pages, const smart_band_state_t *model)
{
  char date_text[20];
  char value[32];
  bool steps_available = metric_available(SMART_BAND_METRIC_STEPS);
  int progress = steps_available ? smart_band_step_progress(model) : 0;

  format_watch_date(date_text, sizeof(date_text));
  if (steps_available)
    {
      snprintf(value, sizeof(value), "%d", model->steps);
    }
  else
    {
      snprintf(value, sizeof(value), "--");
    }

  set_label_text(pages->steps_date, date_text);
  set_label_text(pages->steps_value, value);
  lv_bar_set_value(pages->steps_progress, progress, LV_ANIM_ON);
  set_label_text_fmt_int(pages->steps_goal, "%d", model->step_goal);
  if (steps_available)
    {
      set_label_text_fmt_int(pages->steps_percent, "%d%%", progress);
    }
  else
    {
      set_label_text(pages->steps_percent, "--");
    }
  set_label_text(pages->steps_source,
                 metric_source_text(SMART_BAND_METRIC_STEPS));
  set_temperature_label(pages->steps_weather);
}
