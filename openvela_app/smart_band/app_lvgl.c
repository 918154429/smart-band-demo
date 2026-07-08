#include "app_lvgl.h"

#include "sensor_bridge.h"
#include "watch_model.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DESIGN_W 330
#define DESIGN_H 626
#define CARD_COUNT 4
#define SMART_BAND_DEFAULT_TZ "CST-8"

typedef struct
{
  lv_obj_t *root;
  lv_obj_t *watch;
  lv_obj_t *screen;
  lv_obj_t *face_page;
  lv_obj_t *heart_page;
  lv_obj_t *steps_page;

  lv_obj_t *dots[SMART_BAND_PAGE_COUNT];

  lv_obj_t *face_date;
  lv_obj_t *face_hour;
  lv_obj_t *face_minute;
  lv_obj_t *face_sleep_value;
  lv_obj_t *face_heart_value;
  lv_obj_t *face_stress_value;
  lv_obj_t *face_weather_value;
  lv_obj_t *face_battery;

  lv_obj_t *heart_date;
  lv_obj_t *heart_value;
  lv_obj_t *heart_progress;
  lv_obj_t *heart_status;
  lv_obj_t *heart_battery;
  lv_obj_t *heart_source;
  lv_obj_t *heart_stress;

  lv_obj_t *steps_date;
  lv_obj_t *steps_value;
  lv_obj_t *steps_progress;
  lv_obj_t *steps_goal;
  lv_obj_t *steps_percent;
  lv_obj_t *steps_source;
  lv_obj_t *steps_weather;

  lv_timer_t *timer;
  smart_band_state_t model;
  smart_band_sensor_bridge_t sensors;
  lv_coord_t screen_w;
  lv_coord_t screen_h;
  lv_point_t press_point;
  bool press_valid;
} smart_band_ui_t;

static smart_band_ui_t g_ui;

static void page_drag_cb(lv_event_t *event);
static void dot_click_cb(lv_event_t *event);
static void enable_touch_navigation(lv_obj_t *obj);

static const lv_font_t *font_12(void)
{
#if LV_FONT_MONTSERRAT_12
  return &lv_font_montserrat_12;
#else
  return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t *font_14(void)
{
#if LV_FONT_MONTSERRAT_14
  return &lv_font_montserrat_14;
#else
  return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t *font_16(void)
{
#if LV_FONT_MONTSERRAT_16
  return &lv_font_montserrat_16;
#else
  return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t *font_20(void)
{
#if LV_FONT_MONTSERRAT_20
  return &lv_font_montserrat_20;
#elif LV_FONT_MONTSERRAT_16
  return &lv_font_montserrat_16;
#else
  return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t *font_32(void)
{
#if LV_FONT_MONTSERRAT_32
  return &lv_font_montserrat_32;
#elif LV_FONT_MONTSERRAT_20
  return &lv_font_montserrat_20;
#else
  return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t *font_time(void)
{
#if LV_FONT_MONTSERRAT_48
  return &lv_font_montserrat_48;
#elif LV_FONT_MONTSERRAT_32
  return &lv_font_montserrat_32;
#else
  return LV_FONT_DEFAULT;
#endif
}

static lv_coord_t sx(int value)
{
  return (lv_coord_t)((value * (int)g_ui.screen_w) / DESIGN_W);
}

static lv_coord_t sy(int value)
{
  return (lv_coord_t)((value * (int)g_ui.screen_h) / DESIGN_H);
}

static lv_coord_t min_coord(lv_coord_t a, lv_coord_t b)
{
  return a < b ? a : b;
}

static lv_coord_t max_coord(lv_coord_t a, lv_coord_t b)
{
  return a > b ? a : b;
}

static lv_coord_t abs_coord(lv_coord_t value)
{
  return value < 0 ? -value : value;
}

static void strip_obj(lv_obj_t *obj)
{
  lv_obj_remove_style_all(obj);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *create_box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                            lv_coord_t w, lv_coord_t h, lv_color_t color,
                            lv_coord_t radius)
{
  lv_obj_t *box = lv_obj_create(parent);
  if (box == NULL)
    {
      return NULL;
    }

  strip_obj(box);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_size(box, w, h);
  lv_obj_set_style_bg_color(box, color, 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(box, radius, 0);
  return box;
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, lv_color_t color,
                              lv_text_align_t align)
{
  lv_obj_t *label = lv_label_create(parent);
  if (label == NULL)
    {
      return NULL;
    }

  strip_obj(label);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, align, 0);
  return label;
}

static void place_label(lv_obj_t *label, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h)
{
  lv_obj_set_pos(label, x, y);
  lv_obj_set_size(label, w, h);
}

static void set_label_text(lv_obj_t *label, const char *text)
{
  if (label != NULL && text != NULL)
    {
      lv_label_set_text(label, text);
    }
}

static void set_label_text_fmt_int(lv_obj_t *label, const char *fmt, int value)
{
  char buffer[32];
  snprintf(buffer, sizeof(buffer), fmt, value);
  set_label_text(label, buffer);
}

static void configure_local_time(void)
{
#if defined(__NuttX__)
  setenv("TZ", SMART_BAND_DEFAULT_TZ, 1);
#endif

  tzset();
}

static void set_temperature_label(lv_obj_t *label)
{
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%dC%s", g_ui.model.temperature_c,
           g_ui.model.temperature_sensor_active ? "" : " sim");
  set_label_text(label, buffer);
}

static void format_watch_date(char *buffer, size_t size)
{
  static const char *const weekdays[] =
  {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
  };
  static const char *const months[] =
  {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
  };

  struct tm local_now;
  struct tm *tm_result = NULL;
  time_t now = time(NULL);

#if defined(_POSIX_VERSION) || defined(__NuttX__)
  tm_result = localtime_r(&now, &local_now);
#else
  tm_result = localtime(&now);
  if (tm_result != NULL)
    {
      local_now = *tm_result;
      tm_result = &local_now;
    }
#endif

  if (tm_result == NULL)
    {
      snprintf(buffer, size, "%s", g_ui.model.date_text);
      return;
    }

  snprintf(buffer, size, "%s %02d %s", weekdays[local_now.tm_wday],
           local_now.tm_mday, months[local_now.tm_mon]);
}

static void split_time_text(char *hour, size_t hour_size, char *minute,
                            size_t minute_size)
{
  if (strlen(g_ui.model.time_text) >= 5 && g_ui.model.time_text[2] == ':')
    {
      snprintf(hour, hour_size, "%c%c", g_ui.model.time_text[0],
               g_ui.model.time_text[1]);
      snprintf(minute, minute_size, "%c%c", g_ui.model.time_text[3],
               g_ui.model.time_text[4]);
      return;
    }

  snprintf(hour, hour_size, "--");
  snprintf(minute, minute_size, "--");
}

static lv_obj_t *create_page(lv_obj_t *parent)
{
  lv_obj_t *page = lv_obj_create(parent);
  if (page == NULL)
    {
      return NULL;
    }

  strip_obj(page);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_size(page, g_ui.screen_w, g_ui.screen_h);
  lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
  return page;
}

static int create_leaf_mark(lv_obj_t *parent, lv_coord_t y)
{
  lv_coord_t leaf_w = sx(28);
  lv_coord_t leaf_h = sy(18);
  lv_coord_t center = g_ui.screen_w / 2;
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

static int create_date_row(lv_obj_t *parent, lv_obj_t **date_label,
                           lv_coord_t y)
{
  lv_coord_t dot = sx(7);
  lv_coord_t text_w = sx(180);
  lv_obj_t *left_dot;
  lv_obj_t *right_dot;

  left_dot = create_box(parent, (g_ui.screen_w - text_w) / 2 - sx(18),
                        y + sy(7), dot, dot, lv_color_hex(0x77c4bd),
                        LV_RADIUS_CIRCLE);
  right_dot = create_box(parent, (g_ui.screen_w + text_w) / 2 + sx(11),
                         y + sy(7), dot, dot, lv_color_hex(0x77c4bd),
                         LV_RADIUS_CIRCLE);
  *date_label = create_label(parent, "WED 08 JUL", font_20(),
                             lv_color_hex(0x6f8790), LV_TEXT_ALIGN_CENTER);

  if (left_dot == NULL || right_dot == NULL || *date_label == NULL)
    {
      return -1;
    }

  place_label(*date_label, (g_ui.screen_w - text_w) / 2, y, text_w, sy(28));
  return 0;
}

static int create_ornament(lv_obj_t *parent, lv_coord_t y)
{
  lv_coord_t line_w = sx(86);
  lv_coord_t line_h = max_coord(sy(3), 2);
  lv_coord_t center = g_ui.screen_w / 2;
  lv_obj_t *left;
  lv_obj_t *right;
  lv_obj_t *badge;
  lv_obj_t *label;

  left = create_box(parent, center - sx(70) - line_w, y + sy(16), line_w,
                    line_h, lv_color_hex(0xb9e0dc), LV_RADIUS_CIRCLE);
  right = create_box(parent, center + sx(70), y + sy(16), line_w, line_h,
                     lv_color_hex(0xb9e0dc), LV_RADIUS_CIRCLE);
  badge = create_box(parent, center - sx(22), y, sx(44), sy(34),
                     lv_color_hex(0xeff9f7), LV_RADIUS_CIRCLE);
  label = create_label(badge, "HR", font_16(), lv_color_hex(0x79c5be),
                       LV_TEXT_ALIGN_CENTER);

  if (left == NULL || right == NULL || badge == NULL || label == NULL)
    {
      return -1;
    }

  place_label(label, 0, sy(7), sx(44), sy(20));
  return 0;
}

static lv_obj_t *create_orb(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                            lv_coord_t size, lv_color_t color,
                            const char *text)
{
  lv_obj_t *orb = create_box(parent, x, y, size, size, color,
                             LV_RADIUS_CIRCLE);
  lv_obj_t *label;

  if (orb == NULL)
    {
      return NULL;
    }

  label = create_label(orb, text, font_16(), lv_color_hex(0xffffff),
                       LV_TEXT_ALIGN_CENTER);
  if (label == NULL)
    {
      return NULL;
    }

  lv_obj_set_style_text_font(label, font_20(), 0);
  place_label(label, 0, (size - sy(24)) / 2, size, sy(26));
  return orb;
}

static int create_metric_card(lv_obj_t *parent, lv_coord_t y,
                              lv_color_t bg_color, lv_color_t orb_color,
                              const char *orb_text, const char *label_text,
                              lv_color_t label_color, lv_obj_t **value_out)
{
  lv_coord_t margin = sx(22);
  lv_coord_t card_w = g_ui.screen_w - margin * 2;
  lv_coord_t card_h = sy(72);
  lv_coord_t orb_size = min_coord(sx(54), card_h - sy(18));
  lv_coord_t value_x = sx(112);
  lv_obj_t *card;
  lv_obj_t *divider;
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

  if (create_orb(card, sx(16), (card_h - orb_size) / 2, orb_size,
                 orb_color, orb_text) == NULL)
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

static int create_face_page(void)
{
  lv_coord_t cards_y = sy(260);
  lv_coord_t card_gap = sy(12);
  lv_coord_t card_h = sy(72);
  lv_obj_t *time_row;
  lv_obj_t *colon_one;
  lv_obj_t *colon_two;

  g_ui.face_page = create_page(g_ui.screen);
  if (g_ui.face_page == NULL || create_leaf_mark(g_ui.face_page, sy(32)) != 0 ||
      create_date_row(g_ui.face_page, &g_ui.face_date, sy(78)) != 0)
    {
      return -1;
    }

  time_row = lv_obj_create(g_ui.face_page);
  if (time_row == NULL)
    {
      return -1;
    }

  strip_obj(time_row);
  lv_obj_set_pos(time_row, sx(22), sy(114));
  lv_obj_set_size(time_row, g_ui.screen_w - sx(44), sy(78));
  lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);

  g_ui.face_hour = create_label(time_row, "--", font_time(),
                                lv_color_hex(0x00796c), LV_TEXT_ALIGN_RIGHT);
  g_ui.face_minute = create_label(time_row, "--", font_time(),
                                  lv_color_hex(0x293b53), LV_TEXT_ALIGN_LEFT);
  colon_one = create_box(time_row, (g_ui.screen_w - sx(44)) / 2 - sx(7),
                         sy(22), sx(14), sx(14), lv_color_hex(0x79c5be),
                         LV_RADIUS_CIRCLE);
  colon_two = create_box(time_row, (g_ui.screen_w - sx(44)) / 2 - sx(7),
                         sy(48), sx(14), sx(14), lv_color_hex(0x79c5be),
                         LV_RADIUS_CIRCLE);

  if (g_ui.face_hour == NULL || g_ui.face_minute == NULL ||
      colon_one == NULL || colon_two == NULL ||
      create_ornament(g_ui.face_page, sy(206)) != 0)
    {
      return -1;
    }

  place_label(g_ui.face_hour, 0, 0, sx(122), sy(76));
  place_label(g_ui.face_minute, sx(184), 0, sx(122), sy(76));

  if (create_metric_card(g_ui.face_page, cards_y,
                         lv_color_hex(0xf2f5ff), lv_color_hex(0x9caddc),
                         "Zz", "Sleep", lv_color_hex(0x8799cf),
                         &g_ui.face_sleep_value) != 0 ||
      create_metric_card(g_ui.face_page, cards_y + card_h + card_gap,
                         lv_color_hex(0xfff0eb), lv_color_hex(0xf08d88),
                         "HR", "Heart Rate", lv_color_hex(0xea7770),
                         &g_ui.face_heart_value) != 0 ||
      create_metric_card(g_ui.face_page, cards_y + (card_h + card_gap) * 2,
                         lv_color_hex(0xeefbf8), lv_color_hex(0x80cbc3),
                         "OK", "Stress", lv_color_hex(0x43a79e),
                         &g_ui.face_stress_value) != 0 ||
      create_metric_card(g_ui.face_page, cards_y + (card_h + card_gap) * 3,
                         lv_color_hex(0xfff6e2), lv_color_hex(0xf5c66e),
                         "C", "Weather", lv_color_hex(0xe8ae46),
                         &g_ui.face_weather_value) != 0)
    {
      return -1;
    }

  g_ui.face_battery = create_label(g_ui.face_page, "BAT --%", font_12(),
                                   lv_color_hex(0x6f8790),
                                   LV_TEXT_ALIGN_CENTER);
  if (g_ui.face_battery == NULL)
    {
      return -1;
    }

  place_label(g_ui.face_battery, g_ui.screen_w - sx(96), sy(22), sx(74),
              sy(20));
  return 0;
}

static lv_obj_t *create_detail_hero(lv_obj_t *page, lv_color_t hero_bg,
                                    lv_color_t orb_bg, const char *orb_text,
                                    lv_obj_t **value_out,
                                    lv_obj_t **progress_out)
{
  lv_coord_t margin = sx(22);
  lv_coord_t hero_y = sy(154);
  lv_coord_t hero_w = g_ui.screen_w - margin * 2;
  lv_coord_t hero_h = sy(190);
  lv_coord_t orb_size = sx(82);
  lv_obj_t *hero;

  hero = create_box(page, margin, hero_y, hero_w, hero_h, hero_bg, sx(32));
  if (hero == NULL)
    {
      return NULL;
    }

  lv_obj_set_style_shadow_width(hero, sx(18), 0);
  lv_obj_set_style_shadow_color(hero, lv_color_hex(0x374a5b), 0);
  lv_obj_set_style_shadow_opa(hero, LV_OPA_20, 0);
  lv_obj_set_style_shadow_offset_y(hero, sy(10), 0);

  if (create_orb(hero, (hero_w - orb_size) / 2, sy(20), orb_size, orb_bg,
                 orb_text) == NULL)
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

static int create_mini_card(lv_obj_t *page, int col, int row,
                            const char *title, lv_obj_t **value_out)
{
  lv_coord_t margin = sx(22);
  lv_coord_t gap = sx(12);
  lv_coord_t card_w = (g_ui.screen_w - margin * 2 - gap) / 2;
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

static int create_heart_page(void)
{
  lv_obj_t *title;

  g_ui.heart_page = create_page(g_ui.screen);
  if (g_ui.heart_page == NULL ||
      create_leaf_mark(g_ui.heart_page, sy(32)) != 0 ||
      create_date_row(g_ui.heart_page, &g_ui.heart_date, sy(78)) != 0)
    {
      return -1;
    }

  title = create_label(g_ui.heart_page, "Heart Rate", font_20(),
                       lv_color_hex(0x5a7680), LV_TEXT_ALIGN_CENTER);
  if (title == NULL)
    {
      return -1;
    }

  place_label(title, sx(22), sy(112), g_ui.screen_w - sx(44), sy(28));

  if (create_detail_hero(g_ui.heart_page, lv_color_hex(0xfff0eb),
                         lv_color_hex(0xf08d88), "HR",
                         &g_ui.heart_value,
                         &g_ui.heart_progress) == NULL ||
      create_mini_card(g_ui.heart_page, 0, 0, "Resting",
                       &g_ui.heart_status) != 0 ||
      create_mini_card(g_ui.heart_page, 1, 0, "Status",
                       &g_ui.heart_source) != 0 ||
      create_mini_card(g_ui.heart_page, 0, 1, "Battery",
                       &g_ui.heart_battery) != 0 ||
      create_mini_card(g_ui.heart_page, 1, 1, "Stress",
                       &g_ui.heart_stress) != 0)
    {
      return -1;
    }

  return 0;
}

static int create_steps_page(void)
{
  lv_obj_t *title;

  g_ui.steps_page = create_page(g_ui.screen);
  if (g_ui.steps_page == NULL ||
      create_leaf_mark(g_ui.steps_page, sy(32)) != 0 ||
      create_date_row(g_ui.steps_page, &g_ui.steps_date, sy(78)) != 0)
    {
      return -1;
    }

  title = create_label(g_ui.steps_page, "Activity", font_20(),
                       lv_color_hex(0x5a7680), LV_TEXT_ALIGN_CENTER);
  if (title == NULL)
    {
      return -1;
    }

  place_label(title, sx(22), sy(112), g_ui.screen_w - sx(44), sy(28));

  if (create_detail_hero(g_ui.steps_page, lv_color_hex(0xeefbf8),
                         lv_color_hex(0x80cbc3), "ST",
                         &g_ui.steps_value,
                         &g_ui.steps_progress) == NULL ||
      create_mini_card(g_ui.steps_page, 0, 0, "Goal",
                       &g_ui.steps_goal) != 0 ||
      create_mini_card(g_ui.steps_page, 1, 0, "Progress",
                       &g_ui.steps_percent) != 0 ||
      create_mini_card(g_ui.steps_page, 0, 1, "Source",
                       &g_ui.steps_source) != 0 ||
      create_mini_card(g_ui.steps_page, 1, 1, "Weather",
                       &g_ui.steps_weather) != 0)
    {
      return -1;
    }

  return 0;
}

static int create_dots(void)
{
  lv_coord_t dot = sx(8);
  lv_coord_t gap = sx(12);
  lv_coord_t row_w = dot * SMART_BAND_PAGE_COUNT +
                     gap * (SMART_BAND_PAGE_COUNT - 1);
  lv_obj_t *row = lv_obj_create(g_ui.screen);

  if (row == NULL)
    {
      return -1;
    }

  strip_obj(row);
  lv_obj_set_size(row, row_w, sy(16));
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -sy(22));
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);

  for (int i = 0; i < SMART_BAND_PAGE_COUNT; i++)
    {
      g_ui.dots[i] = create_box(row, i * (dot + gap), sy(4), dot, dot,
                                lv_color_hex(0xc2d3d1), LV_RADIUS_CIRCLE);
      if (g_ui.dots[i] == NULL)
        {
          return -1;
        }

      lv_obj_add_flag(g_ui.dots[i], LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(g_ui.dots[i], dot_click_cb, LV_EVENT_CLICKED,
                          (void *)(uintptr_t)i);
    }

  return 0;
}

static int create_background_waves(void)
{
  lv_obj_t *wave_one;
  lv_obj_t *wave_two;

  wave_one = create_box(g_ui.screen, -sx(40), g_ui.screen_h - sy(104),
                        g_ui.screen_w + sx(80), sy(156),
                        lv_color_hex(0xe5f7f4), LV_RADIUS_CIRCLE);
  wave_two = create_box(g_ui.screen, sx(84), g_ui.screen_h - sy(82),
                        g_ui.screen_w, sy(124), lv_color_hex(0xfff4dd),
                        LV_RADIUS_CIRCLE);
  if (wave_one == NULL || wave_two == NULL)
    {
      return -1;
    }

  lv_obj_set_style_bg_opa(wave_one, LV_OPA_50, 0);
  lv_obj_set_style_bg_opa(wave_two, LV_OPA_40, 0);
  return 0;
}

static int create_ui_tree(lv_obj_t *root)
{
  lv_coord_t root_w;
  lv_coord_t root_h;
  lv_coord_t watch_w;
  lv_coord_t watch_h;
  lv_coord_t frame_radius;

  lv_obj_update_layout(root);
  root_w = lv_obj_get_width(root);
  root_h = lv_obj_get_height(root);

  if (root_w <= 0)
    {
      root_w = 320;
    }

  if (root_h <= 0)
    {
      root_h = 480;
    }

  watch_h = min_coord(root_h - 48, 720);
  watch_h = max_coord(watch_h, 360);
  watch_w = (watch_h * 194) / 368;

  if (watch_w > root_w - 48)
    {
      watch_w = root_w - 48;
      watch_h = (watch_w * 368) / 194;
    }

  frame_radius = max_coord((watch_w * 54) / 330, 34);

  lv_obj_clean(root);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(root, lv_color_hex(0xeef4f3), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(root, 0, 0);

  g_ui.watch = lv_obj_create(root);
  if (g_ui.watch == NULL)
    {
      return -1;
    }

  strip_obj(g_ui.watch);
  lv_obj_set_size(g_ui.watch, watch_w, watch_h);
  lv_obj_center(g_ui.watch);
  lv_obj_set_style_bg_color(g_ui.watch, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_grad_color(g_ui.watch, lv_color_hex(0xf9fbf8), 0);
  lv_obj_set_style_bg_grad_dir(g_ui.watch, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(g_ui.watch, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_ui.watch, frame_radius, 0);
  lv_obj_set_style_border_width(g_ui.watch, 1, 0);
  lv_obj_set_style_border_color(g_ui.watch, lv_color_hex(0xb7c2c7), 0);
  lv_obj_set_style_shadow_width(g_ui.watch, 32, 0);
  lv_obj_set_style_shadow_color(g_ui.watch, lv_color_hex(0x1c3040), 0);
  lv_obj_set_style_shadow_opa(g_ui.watch, LV_OPA_30, 0);
  lv_obj_set_style_shadow_offset_y(g_ui.watch, 18, 0);

  g_ui.screen = lv_obj_create(g_ui.watch);
  if (g_ui.screen == NULL)
    {
      return -1;
    }

  strip_obj(g_ui.screen);
  g_ui.screen_w = watch_w - 6;
  g_ui.screen_h = watch_h - 6;
  lv_obj_set_size(g_ui.screen, g_ui.screen_w, g_ui.screen_h);
  lv_obj_center(g_ui.screen);
  lv_obj_set_style_bg_color(g_ui.screen, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_grad_color(g_ui.screen, lv_color_hex(0xfffcf6), 0);
  lv_obj_set_style_bg_grad_dir(g_ui.screen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(g_ui.screen, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_ui.screen, frame_radius - 2, 0);
  lv_obj_set_style_clip_corner(g_ui.screen, true, 0);
  lv_obj_add_flag(g_ui.screen, LV_OBJ_FLAG_CLICKABLE);

  if (create_background_waves() != 0 || create_face_page() != 0 ||
      create_heart_page() != 0 || create_steps_page() != 0 ||
      create_dots() != 0)
    {
      return -1;
    }

  enable_touch_navigation(g_ui.screen);
  return 0;
}

static void update_dots(void)
{
  for (int i = 0; i < SMART_BAND_PAGE_COUNT; i++)
    {
      lv_color_t color = i == (int)g_ui.model.page ? lv_color_hex(0x79c5be) :
                                                       lv_color_hex(0xc2d3d1);
      lv_obj_set_style_bg_color(g_ui.dots[i], color, 0);
      lv_obj_set_size(g_ui.dots[i], i == (int)g_ui.model.page ? sx(18) : sx(8),
                      sx(8));
    }
}

static void set_page_visible(lv_obj_t *page, bool visible)
{
  if (visible)
    {
      lv_obj_clear_flag(page, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_page_visibility(void)
{
  set_page_visible(g_ui.face_page, g_ui.model.page == SMART_BAND_PAGE_FACE);
  set_page_visible(g_ui.heart_page, g_ui.model.page == SMART_BAND_PAGE_HEART);
  set_page_visible(g_ui.steps_page, g_ui.model.page == SMART_BAND_PAGE_STEPS);
  update_dots();
}

static void switch_to_page(smart_band_page_t page)
{
  if (page >= SMART_BAND_PAGE_COUNT)
    {
      return;
    }

  g_ui.model.page = page;
  update_page_visibility();
}

static void update_face(void)
{
  char hour[3];
  char minute[3];
  char date_text[20];
  char value[32];

  split_time_text(hour, sizeof(hour), minute, sizeof(minute));
  format_watch_date(date_text, sizeof(date_text));

  set_label_text(g_ui.face_hour, hour);
  set_label_text(g_ui.face_minute, minute);
  set_label_text(g_ui.face_date, date_text);
  set_label_text(g_ui.face_sleep_value, "7h 48m");
  set_label_text_fmt_int(g_ui.face_heart_value, "%d bpm", g_ui.model.heart_rate);
  set_label_text(g_ui.face_stress_value, "Low");
  set_temperature_label(g_ui.face_weather_value);

  snprintf(value, sizeof(value), "BAT %d%%%s", g_ui.model.battery_percent,
           g_ui.model.battery_sensor_active ? "" : " sim");
  set_label_text(g_ui.face_battery, value);
}

static void update_heart_detail(void)
{
  char date_text[20];
  char value[32];
  int progress = (g_ui.model.heart_rate * 100) / 135;

  if (progress > 100)
    {
      progress = 100;
    }

  format_watch_date(date_text, sizeof(date_text));
  snprintf(value, sizeof(value), "%d bpm", g_ui.model.heart_rate);

  set_label_text(g_ui.heart_date, date_text);
  set_label_text(g_ui.heart_value, value);
  lv_bar_set_value(g_ui.heart_progress, progress, LV_ANIM_ON);
  set_label_text(g_ui.heart_status, "62");
  set_label_text(g_ui.heart_source,
                 g_ui.model.heart_sensor_active ? "Sensor" :
                 (g_ui.model.heart_rate > 110 ? "High" : "Good"));
  set_label_text_fmt_int(g_ui.heart_battery, "%d%%", g_ui.model.battery_percent);
  set_label_text(g_ui.heart_stress, "Low");
}

static void update_steps_detail(void)
{
  char date_text[20];
  char value[32];
  int progress = smart_band_step_progress(&g_ui.model);

  format_watch_date(date_text, sizeof(date_text));
  snprintf(value, sizeof(value), "%d", g_ui.model.steps);

  set_label_text(g_ui.steps_date, date_text);
  set_label_text(g_ui.steps_value, value);
  lv_bar_set_value(g_ui.steps_progress, progress, LV_ANIM_ON);
  set_label_text(g_ui.steps_goal, "8000");
  set_label_text_fmt_int(g_ui.steps_percent, "%d%%", progress);
  set_label_text(g_ui.steps_source,
                 g_ui.model.step_sensor_active ? "Sensor" : "Model");
  set_temperature_label(g_ui.steps_weather);
}

static void render_page(void)
{
  update_face();
  update_heart_detail();
  update_steps_detail();
  update_page_visibility();
}

static void timer_cb(lv_timer_t *timer)
{
  (void)timer;
  smart_band_state_tick(&g_ui.model, time(NULL));
  smart_band_sensor_bridge_update(&g_ui.sensors, &g_ui.model);
  render_page();
}

static void next_page(void)
{
  smart_band_next_page(&g_ui.model);
  update_page_visibility();
}

static void prev_page(void)
{
  smart_band_prev_page(&g_ui.model);
  update_page_visibility();
}

static void page_drag_cb(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);
  lv_indev_t *indev = lv_indev_get_act();
  lv_point_t point;
  lv_coord_t dx;
  lv_coord_t dy;
  lv_coord_t threshold = max_coord(sx(36), 28);

  if (indev == NULL)
    {
      return;
    }

  if (code == LV_EVENT_PRESSED)
    {
      lv_indev_get_point(indev, &g_ui.press_point);
      g_ui.press_valid = true;
      return;
    }

  if (code != LV_EVENT_RELEASED || !g_ui.press_valid)
    {
      return;
    }

  g_ui.press_valid = false;
  lv_indev_get_point(indev, &point);
  dx = point.x - g_ui.press_point.x;
  dy = point.y - g_ui.press_point.y;

  if (abs_coord(dx) < threshold || abs_coord(dx) <= abs_coord(dy))
    {
      return;
    }

  if (dx < 0)
    {
      next_page();
    }
  else
    {
      prev_page();
    }
}

static void dot_click_cb(lv_event_t *event)
{
  smart_band_page_t page =
    (smart_band_page_t)(uintptr_t)lv_event_get_user_data(event);

  switch_to_page(page);
}

static void enable_touch_navigation(lv_obj_t *obj)
{
  uint32_t child_count;

  if (obj == NULL)
    {
      return;
    }

  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj, page_drag_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(obj, page_drag_cb, LV_EVENT_RELEASED, NULL);

  child_count = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < child_count; i++)
    {
      enable_touch_navigation(lv_obj_get_child(obj, i));
    }
}

int smart_band_lvgl_create(lv_obj_t *parent)
{
  lv_obj_t *root = parent != NULL ? parent : lv_scr_act();

  if (root == NULL)
    {
      return -1;
    }

  memset(&g_ui, 0, sizeof(g_ui));
  g_ui.root = root;
  configure_local_time();
  smart_band_state_init(&g_ui.model, time(NULL));
  smart_band_sensor_bridge_init(&g_ui.sensors);

  if (create_ui_tree(root) != 0)
    {
      smart_band_sensor_bridge_deinit(&g_ui.sensors);
      return -1;
    }

  g_ui.timer = lv_timer_create(timer_cb, 1000, NULL);
  if (g_ui.timer == NULL)
    {
      smart_band_sensor_bridge_deinit(&g_ui.sensors);
      return -1;
    }

  smart_band_sensor_bridge_update(&g_ui.sensors, &g_ui.model);
  render_page();
  return 0;
}

void smart_band_lvgl_destroy(void)
{
  if (g_ui.timer != NULL)
    {
      lv_timer_del(g_ui.timer);
      g_ui.timer = NULL;
    }

  smart_band_sensor_bridge_deinit(&g_ui.sensors);

  if (g_ui.root != NULL)
    {
      lv_obj_clean(g_ui.root);
      g_ui.root = NULL;
    }
}
