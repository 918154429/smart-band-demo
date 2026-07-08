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
#define SMART_BAND_APP_COUNT 10
#define SMART_BAND_MINE_SIZE 4
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
  SMART_BAND_APP_SETTINGS,
  SMART_BAND_APP_STOPWATCH,
  SMART_BAND_APP_FLASHLIGHT,
  SMART_BAND_APP_MINES,
  SMART_BAND_APP_TETRIS,
  SMART_BAND_APP_WOODEN_FISH
} smart_band_app_id_t;

typedef struct
{
  smart_band_app_id_t id;
  const char *title;
  const char *icon;
  uint32_t color;
} smart_band_app_def_t;

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

  lv_obj_t *apps_page;
  lv_obj_t *apps_date;
  lv_obj_t *apps_launcher;
  lv_obj_t *app_detail;
  lv_obj_t *app_title;
  lv_obj_t *app_content;
  lv_obj_t *app_back;

  smart_band_app_id_t active_app;

  lv_obj_t *weather_temp;
  lv_obj_t *weather_source;
  lv_obj_t *weather_sky;
  lv_obj_t *weather_range;
  lv_obj_t *weather_humidity;
  lv_obj_t *weather_wind;

  lv_obj_t *calc_display;
  char calc_text[24];
  int calc_lhs;
  char calc_op;
  bool calc_has_lhs;
  bool calc_reset_next;

  lv_obj_t *timer_display;
  lv_obj_t *timer_status;
  int timer_seconds;
  bool timer_running;

  lv_obj_t *music_title;
  lv_obj_t *music_status;
  lv_obj_t *music_progress;
  int music_track;
  int music_volume;
  bool music_playing;

  lv_obj_t *setting_brightness;
  lv_obj_t *setting_dnd;
  int brightness_percent;
  bool dnd_enabled;

  lv_obj_t *stopwatch_display;
  lv_obj_t *stopwatch_status;
  int stopwatch_seconds;
  bool stopwatch_running;

  lv_obj_t *flashlight_panel;
  lv_obj_t *flashlight_status;
  bool flashlight_on;

  lv_obj_t *mine_status;
  lv_obj_t *mine_cells[SMART_BAND_MINE_CELLS];
  uint16_t mine_revealed;
  uint16_t mine_mines;
  bool mine_over;

  lv_obj_t *tetris_status;
  lv_obj_t *tetris_cells[SMART_BAND_TETRIS_ROWS * SMART_BAND_TETRIS_COLS];
  uint8_t tetris_rows[SMART_BAND_TETRIS_ROWS];
  int tetris_x;
  int tetris_y;
  int tetris_score;
  bool tetris_running;

  lv_obj_t *fish_count;
  lv_obj_t *fish_mode;
  int fish_merit;
  bool fish_auto;

  lv_timer_t *timer;
  smart_band_state_t model;
  smart_band_sensor_bridge_t sensors;
  lv_coord_t screen_w;
  lv_coord_t screen_h;
  lv_point_t press_point;
  bool press_valid;
} smart_band_ui_t;

static smart_band_ui_t g_ui;

static const smart_band_app_def_t g_app_defs[SMART_BAND_APP_COUNT] =
{
  {SMART_BAND_APP_WEATHER, "Weather", "WX", 0xf5c66e},
  {SMART_BAND_APP_CALCULATOR, "Calculator", "12", 0x80cbc3},
  {SMART_BAND_APP_TIMER, "Timer", "TM", 0xa98bd6},
  {SMART_BAND_APP_MUSIC, "Music", "MU", 0xf08d88},
  {SMART_BAND_APP_SETTINGS, "Settings", "SE", 0x6f8790},
  {SMART_BAND_APP_STOPWATCH, "Stopwatch", "SW", 0x73a1d6},
  {SMART_BAND_APP_FLASHLIGHT, "Flashlight", "FL", 0xf5d36e},
  {SMART_BAND_APP_MINES, "Mines", "MI", 0x8aa8d8},
  {SMART_BAND_APP_TETRIS, "Tetris", "TE", 0x62bfb6},
  {SMART_BAND_APP_WOODEN_FISH, "Wooden Fish", "WF", 0xd9a85f}
};

static void page_drag_cb(lv_event_t *event);
static void dot_click_cb(lv_event_t *event);
static void enable_touch_navigation(lv_obj_t *obj);
static void enable_touch_navigation_tree(lv_obj_t *obj);
static void set_page_visible(lv_obj_t *page, bool visible);
static void app_icon_cb(lv_event_t *event);
static void app_back_cb(lv_event_t *event);
static void calc_cb(lv_event_t *event);
static void timer_app_cb(lv_event_t *event);
static void music_app_cb(lv_event_t *event);
static void settings_app_cb(lv_event_t *event);
static void stopwatch_app_cb(lv_event_t *event);
static void flashlight_app_cb(lv_event_t *event);
static void mine_cell_cb(lv_event_t *event);
static void mine_new_cb(lv_event_t *event);
static void tetris_app_cb(lv_event_t *event);
static void fish_app_cb(lv_event_t *event);

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

static void format_temperature(char *buffer, size_t size)
{
  snprintf(buffer, size, "%d%s%s", g_ui.model.temperature_c,
           "\xC2\xB0" "C",
           g_ui.model.temperature_sensor_active ? "" : " sim");
}

static void format_duration(char *buffer, size_t size, int seconds)
{
  if (seconds < 0)
    {
      seconds = 0;
    }

  snprintf(buffer, size, "%02d:%02d", seconds / 60, seconds % 60);
}

static void set_temperature_label(lv_obj_t *label)
{
  char buffer[32];
  format_temperature(buffer, sizeof(buffer));
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
  time_t now = time(NULL);

  if (!smart_band_display_time(now, &local_now))
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

static lv_obj_t *create_plain_layer(lv_obj_t *parent, lv_coord_t x,
                                    lv_coord_t y, lv_coord_t w,
                                    lv_coord_t h)
{
  lv_obj_t *layer = lv_obj_create(parent);
  if (layer == NULL)
    {
      return NULL;
    }

  strip_obj(layer);
  lv_obj_set_pos(layer, x, y);
  lv_obj_set_size(layer, w, h);
  lv_obj_set_style_bg_opa(layer, LV_OPA_TRANSP, 0);
  return layer;
}

static lv_obj_t *create_action_button(lv_obj_t *parent, const char *text,
                                      lv_coord_t x, lv_coord_t y,
                                      lv_coord_t w, lv_coord_t h,
                                      lv_color_t color, lv_event_cb_t cb,
                                      uintptr_t data)
{
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_t *label;

  if (button == NULL)
    {
      return NULL;
    }

  strip_obj(button);
  lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, w, h);
  lv_obj_set_style_bg_color(button, color, 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(button, sx(16), 0);
  lv_obj_set_style_shadow_width(button, sx(8), 0);
  lv_obj_set_style_shadow_color(button, lv_color_hex(0x314856), 0);
  lv_obj_set_style_shadow_opa(button, LV_OPA_20, 0);
  lv_obj_set_style_shadow_offset_y(button, sy(4), 0);
  lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, (void *)data);

  label = create_label(button, text, font_14(), lv_color_hex(0xffffff),
                       LV_TEXT_ALIGN_CENTER);
  if (label == NULL)
    {
      return NULL;
    }

  lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(label, cb, LV_EVENT_CLICKED, (void *)data);
  place_label(label, sx(4), (h - sy(20)) / 2, w - sx(8), sy(22));
  return button;
}

static int create_app_stat(lv_obj_t *parent, int col, int row,
                           const char *title, lv_obj_t **value_out)
{
  lv_coord_t margin = sx(22);
  lv_coord_t gap = sx(10);
  lv_coord_t card_w = (g_ui.screen_w - margin * 2 - gap) / 2;
  lv_coord_t card_h = sy(66);
  lv_coord_t x = margin + col * (card_w + gap);
  lv_coord_t y = sy(150) + row * (card_h + sy(10));
  lv_obj_t *card;
  lv_obj_t *title_label;

  card = create_box(parent, x, y, card_w, card_h, lv_color_hex(0xffffff),
                    sx(18));
  if (card == NULL)
    {
      return -1;
    }

  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0xe6eeee), 0);
  title_label = create_label(card, title, font_12(), lv_color_hex(0x81939a),
                             LV_TEXT_ALIGN_LEFT);
  *value_out = create_label(card, "--", font_16(), lv_color_hex(0x293b53),
                            LV_TEXT_ALIGN_LEFT);
  if (title_label == NULL || *value_out == NULL)
    {
      return -1;
    }

  place_label(title_label, sx(12), sy(10), card_w - sx(24), sy(18));
  place_label(*value_out, sx(12), sy(34), card_w - sx(24), sy(24));
  return 0;
}

static const smart_band_app_def_t *find_app_def(smart_band_app_id_t id)
{
  for (int i = 0; i < SMART_BAND_APP_COUNT; i++)
    {
      if (g_app_defs[i].id == id)
        {
          return &g_app_defs[i];
        }
    }

  return NULL;
}

static void clear_app_object_refs(void)
{
  g_ui.weather_temp = NULL;
  g_ui.weather_source = NULL;
  g_ui.weather_sky = NULL;
  g_ui.weather_range = NULL;
  g_ui.weather_humidity = NULL;
  g_ui.weather_wind = NULL;
  g_ui.calc_display = NULL;
  g_ui.timer_display = NULL;
  g_ui.timer_status = NULL;
  g_ui.music_title = NULL;
  g_ui.music_status = NULL;
  g_ui.music_progress = NULL;
  g_ui.setting_brightness = NULL;
  g_ui.setting_dnd = NULL;
  g_ui.stopwatch_display = NULL;
  g_ui.stopwatch_status = NULL;
  g_ui.flashlight_panel = NULL;
  g_ui.flashlight_status = NULL;
  g_ui.mine_status = NULL;
  g_ui.tetris_status = NULL;
  g_ui.fish_count = NULL;
  g_ui.fish_mode = NULL;
  memset(g_ui.mine_cells, 0, sizeof(g_ui.mine_cells));
  memset(g_ui.tetris_cells, 0, sizeof(g_ui.tetris_cells));
}

static void update_weather_app(void)
{
  char temp[32];
  char source[40];
  char humidity[16];
  char wind[16];

  format_temperature(temp, sizeof(temp));
  snprintf(source, sizeof(source), "%s",
           g_ui.model.temperature_sensor_active ? "ambient_temp0" :
           "model fallback");
  snprintf(humidity, sizeof(humidity), "%d%%",
           54 + (int)(g_ui.model.ticks % 10u));
  snprintf(wind, sizeof(wind), "E%d", 2 + (int)(g_ui.model.ticks % 3u));

  set_label_text(g_ui.weather_temp, temp);
  set_label_text(g_ui.weather_source, source);
  set_label_text(g_ui.weather_sky,
                 g_ui.model.temperature_c >= 30 ? "Sunny" : "Cloudy");
  set_label_text(g_ui.weather_range,
                 g_ui.model.temperature_c >= 30 ? "32/26" : "28/22");
  set_label_text(g_ui.weather_humidity, humidity);
  set_label_text(g_ui.weather_wind, wind);
}

static void update_calc_display(void)
{
  set_label_text(g_ui.calc_display, g_ui.calc_text);
}

static void update_timer_app(void)
{
  char value[16];

  format_duration(value, sizeof(value), g_ui.timer_seconds);
  set_label_text(g_ui.timer_display, value);
  set_label_text(g_ui.timer_status,
                 g_ui.timer_running ? "Running" :
                 (g_ui.timer_seconds == 0 ? "Done" : "Ready"));
}

static void update_music_app(void)
{
  static const char *const tracks[] =
  {
    "Morning Walk", "Lo-Fi Set", "Night Run"
  };

  char status[32];
  int progress = (int)((g_ui.model.ticks % 180u) * 100u / 180u);

  if (g_ui.music_track < 0 || g_ui.music_track >= 3)
    {
      g_ui.music_track = 0;
    }

  snprintf(status, sizeof(status), "%s  Vol %d%%",
           g_ui.music_playing ? "Playing" : "Paused", g_ui.music_volume);
  set_label_text(g_ui.music_title, tracks[g_ui.music_track]);
  set_label_text(g_ui.music_status, status);
  if (g_ui.music_progress != NULL)
    {
      lv_bar_set_value(g_ui.music_progress,
                       g_ui.music_playing ? progress : 0, LV_ANIM_ON);
    }
}

static void update_settings_app(void)
{
  set_label_text_fmt_int(g_ui.setting_brightness, "%d%%",
                         g_ui.brightness_percent);
  set_label_text(g_ui.setting_dnd, g_ui.dnd_enabled ? "On" : "Off");
}

static void update_stopwatch_app(void)
{
  char value[16];

  format_duration(value, sizeof(value), g_ui.stopwatch_seconds);
  set_label_text(g_ui.stopwatch_display, value);
  set_label_text(g_ui.stopwatch_status,
                 g_ui.stopwatch_running ? "Running" : "Paused");
}

static void update_flashlight_app(void)
{
  if (g_ui.flashlight_panel != NULL)
    {
      lv_obj_set_style_bg_color(g_ui.flashlight_panel,
                                g_ui.flashlight_on ?
                                lv_color_hex(0xffffff) :
                                lv_color_hex(0x263943), 0);
    }

  set_label_text(g_ui.flashlight_status,
                 g_ui.flashlight_on ? "On" : "Off");
}

static int mine_neighbor_count(int index)
{
  int row = index / SMART_BAND_MINE_SIZE;
  int col = index % SMART_BAND_MINE_SIZE;
  int count = 0;

  for (int dy = -1; dy <= 1; dy++)
    {
      for (int dx = -1; dx <= 1; dx++)
        {
          int nr = row + dy;
          int nc = col + dx;
          int ni = nr * SMART_BAND_MINE_SIZE + nc;

          if (dx == 0 && dy == 0)
            {
              continue;
            }

          if (nr >= 0 && nr < SMART_BAND_MINE_SIZE &&
              nc >= 0 && nc < SMART_BAND_MINE_SIZE &&
              (g_ui.mine_mines & (uint16_t)(1u << ni)) != 0)
            {
              count++;
            }
        }
    }

  return count;
}

static void update_mines_app(void)
{
  int safe_cells = SMART_BAND_MINE_CELLS - 3;
  int revealed_safe = 0;

  for (int i = 0; i < SMART_BAND_MINE_CELLS; i++)
    {
      lv_obj_t *cell = g_ui.mine_cells[i];
      bool is_mine = (g_ui.mine_mines & (uint16_t)(1u << i)) != 0;
      bool revealed = (g_ui.mine_revealed & (uint16_t)(1u << i)) != 0;
      lv_obj_t *label;
      char text[4];

      if (cell == NULL)
        {
          continue;
        }

      label = lv_obj_get_child(cell, 0);
      if (revealed)
        {
          if (!is_mine)
            {
              revealed_safe++;
            }

          if (is_mine)
            {
              snprintf(text, sizeof(text), "*");
              lv_obj_set_style_bg_color(cell, lv_color_hex(0xf08d88), 0);
            }
          else
            {
              int count = mine_neighbor_count(i);
              snprintf(text, sizeof(text), count == 0 ? " " : "%d", count);
              lv_obj_set_style_bg_color(cell, lv_color_hex(0xffffff), 0);
            }
        }
      else
        {
          snprintf(text, sizeof(text), " ");
          lv_obj_set_style_bg_color(cell, lv_color_hex(0x80cbc3), 0);
        }

      if (label != NULL)
        {
          set_label_text(label, text);
          lv_obj_set_style_text_color(label, lv_color_hex(0x293b53), 0);
        }
    }

  if (g_ui.mine_over)
    {
      set_label_text(g_ui.mine_status, "Boom. New game?");
    }
  else if (revealed_safe >= safe_cells)
    {
      set_label_text(g_ui.mine_status, "Cleared");
      g_ui.mine_over = true;
    }
  else
    {
      set_label_text_fmt_int(g_ui.mine_status, "%d safe left",
                             safe_cells - revealed_safe);
    }
}

static bool tetris_occupied(int x, int y)
{
  if (x < 0 || x >= SMART_BAND_TETRIS_COLS ||
      y < 0 || y >= SMART_BAND_TETRIS_ROWS)
    {
      return true;
    }

  return (g_ui.tetris_rows[y] & (uint8_t)(1u << x)) != 0;
}

static bool tetris_can_place(int x, int y)
{
  return !tetris_occupied(x, y) && !tetris_occupied(x + 1, y) &&
         !tetris_occupied(x, y + 1) && !tetris_occupied(x + 1, y + 1);
}

static void tetris_clear_lines(void)
{
  uint8_t full = (uint8_t)((1u << SMART_BAND_TETRIS_COLS) - 1u);

  for (int row = SMART_BAND_TETRIS_ROWS - 1; row >= 0; row--)
    {
      if (g_ui.tetris_rows[row] == full)
        {
          for (int move = row; move > 0; move--)
            {
              g_ui.tetris_rows[move] = g_ui.tetris_rows[move - 1];
            }

          g_ui.tetris_rows[0] = 0;
          g_ui.tetris_score += 10;
          row++;
        }
    }
}

static void tetris_spawn(void)
{
  g_ui.tetris_x = 2;
  g_ui.tetris_y = 0;
  if (!tetris_can_place(g_ui.tetris_x, g_ui.tetris_y))
    {
      g_ui.tetris_running = false;
      set_label_text(g_ui.tetris_status, "Game over");
    }
}

static void tetris_lock_piece(void)
{
  for (int dy = 0; dy < 2; dy++)
    {
      for (int dx = 0; dx < 2; dx++)
        {
          int x = g_ui.tetris_x + dx;
          int y = g_ui.tetris_y + dy;

          if (x >= 0 && x < SMART_BAND_TETRIS_COLS &&
              y >= 0 && y < SMART_BAND_TETRIS_ROWS)
            {
              g_ui.tetris_rows[y] |= (uint8_t)(1u << x);
            }
        }
    }

  tetris_clear_lines();
  tetris_spawn();
}

static void tetris_step(void)
{
  if (!g_ui.tetris_running)
    {
      return;
    }

  if (tetris_can_place(g_ui.tetris_x, g_ui.tetris_y + 1))
    {
      g_ui.tetris_y++;
    }
  else
    {
      tetris_lock_piece();
    }
}

static bool tetris_piece_at(int x, int y)
{
  return x >= g_ui.tetris_x && x < g_ui.tetris_x + 2 &&
         y >= g_ui.tetris_y && y < g_ui.tetris_y + 2;
}

static void update_tetris_app(void)
{
  char status[32];

  for (int row = 0; row < SMART_BAND_TETRIS_ROWS; row++)
    {
      for (int col = 0; col < SMART_BAND_TETRIS_COLS; col++)
        {
          int index = row * SMART_BAND_TETRIS_COLS + col;
          bool filled = (g_ui.tetris_rows[row] & (uint8_t)(1u << col)) != 0;
          bool falling = g_ui.tetris_running && tetris_piece_at(col, row);
          lv_obj_t *cell = g_ui.tetris_cells[index];

          if (cell != NULL)
            {
              lv_obj_set_style_bg_color(cell,
                                        falling ? lv_color_hex(0xf5c66e) :
                                        (filled ? lv_color_hex(0x6f8790) :
                                        lv_color_hex(0xeaf4f2)), 0);
            }
        }
    }

  snprintf(status, sizeof(status), "Score %d%s", g_ui.tetris_score,
           g_ui.tetris_running ? "" : "  Paused");
  set_label_text(g_ui.tetris_status, status);
}

static void update_fish_app(void)
{
  set_label_text_fmt_int(g_ui.fish_count, "Merit %d", g_ui.fish_merit);
  set_label_text(g_ui.fish_mode, g_ui.fish_auto ? "Auto on" : "Tap mode");
}

static void update_active_app(void)
{
  switch (g_ui.active_app)
    {
      case SMART_BAND_APP_WEATHER:
        update_weather_app();
        break;
      case SMART_BAND_APP_CALCULATOR:
        update_calc_display();
        break;
      case SMART_BAND_APP_TIMER:
        update_timer_app();
        break;
      case SMART_BAND_APP_MUSIC:
        update_music_app();
        break;
      case SMART_BAND_APP_SETTINGS:
        update_settings_app();
        break;
      case SMART_BAND_APP_STOPWATCH:
        update_stopwatch_app();
        break;
      case SMART_BAND_APP_FLASHLIGHT:
        update_flashlight_app();
        break;
      case SMART_BAND_APP_MINES:
        update_mines_app();
        break;
      case SMART_BAND_APP_TETRIS:
        update_tetris_app();
        break;
      case SMART_BAND_APP_WOODEN_FISH:
        update_fish_app();
        break;
      default:
        break;
    }
}

static int build_weather_app(void)
{
  lv_obj_t *hero;
  lv_obj_t *caption;

  hero = create_box(g_ui.app_content, sx(22), sy(8), g_ui.screen_w - sx(44),
                    sy(128), lv_color_hex(0xfff6e2), sx(24));
  if (hero == NULL)
    {
      return -1;
    }

  g_ui.weather_temp = create_label(hero, "--", font_32(),
                                   lv_color_hex(0x293b53),
                                   LV_TEXT_ALIGN_CENTER);
  caption = create_label(hero, "Temperature sensor", font_12(),
                         lv_color_hex(0x81939a), LV_TEXT_ALIGN_CENTER);
  g_ui.weather_source = create_label(hero, "--", font_14(),
                                     lv_color_hex(0xe4a840),
                                     LV_TEXT_ALIGN_CENTER);
  if (g_ui.weather_temp == NULL || caption == NULL ||
      g_ui.weather_source == NULL)
    {
      return -1;
    }

  place_label(g_ui.weather_temp, sx(12), sy(22), g_ui.screen_w - sx(68),
              sy(44));
  place_label(caption, sx(12), sy(72), g_ui.screen_w - sx(68), sy(20));
  place_label(g_ui.weather_source, sx(12), sy(94), g_ui.screen_w - sx(68),
              sy(22));

  if (create_app_stat(g_ui.app_content, 0, 0, "Sky",
                      &g_ui.weather_sky) != 0 ||
      create_app_stat(g_ui.app_content, 1, 0, "Range",
                      &g_ui.weather_range) != 0 ||
      create_app_stat(g_ui.app_content, 0, 1, "Humidity",
                      &g_ui.weather_humidity) != 0 ||
      create_app_stat(g_ui.app_content, 1, 1, "Wind",
                      &g_ui.weather_wind) != 0)
    {
      return -1;
    }

  update_weather_app();
  return 0;
}

static int build_calculator_app(void)
{
  static const char *const keys[] =
  {
    "7", "8", "9", "+",
    "4", "5", "6", "-",
    "1", "2", "3", "=",
    "C", "0", "<", "="
  };

  lv_coord_t margin = sx(22);
  lv_coord_t gap = sx(8);
  lv_coord_t display_h = sy(78);
  lv_coord_t key_w = (g_ui.screen_w - margin * 2 - gap * 3) / 4;
  lv_coord_t key_h = sy(54);
  lv_obj_t *display_box;

  display_box = create_box(g_ui.app_content, margin, sy(8),
                           g_ui.screen_w - margin * 2, display_h,
                           lv_color_hex(0xf3f7fb), sx(20));
  if (display_box == NULL)
    {
      return -1;
    }

  g_ui.calc_display = create_label(display_box, g_ui.calc_text, font_32(),
                                   lv_color_hex(0x293b53),
                                   LV_TEXT_ALIGN_RIGHT);
  if (g_ui.calc_display == NULL)
    {
      return -1;
    }

  place_label(g_ui.calc_display, sx(12), sy(18),
              lv_obj_get_width(display_box) - sx(24), sy(44));

  for (int i = 0; i < 16; i++)
    {
      int row = i / 4;
      int col = i % 4;
      lv_color_t color = (col == 3 || i >= 12) ? lv_color_hex(0x80cbc3) :
                                                 lv_color_hex(0x6f8790);

      if (create_action_button(g_ui.app_content, keys[i],
                               margin + col * (key_w + gap),
                               sy(102) + row * (key_h + sy(8)),
                               key_w, key_h, color, calc_cb,
                               (uintptr_t)keys[i][0]) == NULL)
        {
          return -1;
        }
    }

  update_calc_display();
  return 0;
}

static int build_timer_app(void)
{
  lv_obj_t *panel;

  panel = create_box(g_ui.app_content, sx(22), sy(14), g_ui.screen_w - sx(44),
                     sy(180), lv_color_hex(0xf2f5ff), sx(28));
  if (panel == NULL)
    {
      return -1;
    }

  g_ui.timer_display = create_label(panel, "05:00", font_time(),
                                    lv_color_hex(0x293b53),
                                    LV_TEXT_ALIGN_CENTER);
  g_ui.timer_status = create_label(panel, "Ready", font_14(),
                                   lv_color_hex(0x81939a),
                                   LV_TEXT_ALIGN_CENTER);
  if (g_ui.timer_display == NULL || g_ui.timer_status == NULL)
    {
      return -1;
    }

  place_label(g_ui.timer_display, sx(12), sy(38), g_ui.screen_w - sx(68),
              sy(70));
  place_label(g_ui.timer_status, sx(12), sy(116), g_ui.screen_w - sx(68),
              sy(26));

  if (create_action_button(g_ui.app_content, "+1m", sx(22), sy(220),
                           sx(84), sy(54), lv_color_hex(0xa98bd6),
                           timer_app_cb, 1) == NULL ||
      create_action_button(g_ui.app_content, "Start", sx(124), sy(220),
                           sx(84), sy(54), lv_color_hex(0x80cbc3),
                           timer_app_cb, 2) == NULL ||
      create_action_button(g_ui.app_content, "Reset", sx(226), sy(220),
                           sx(82), sy(54), lv_color_hex(0x6f8790),
                           timer_app_cb, 3) == NULL)
    {
      return -1;
    }

  update_timer_app();
  return 0;
}

static int build_music_app(void)
{
  lv_obj_t *panel;

  panel = create_box(g_ui.app_content, sx(22), sy(14), g_ui.screen_w - sx(44),
                     sy(170), lv_color_hex(0xfff0eb), sx(28));
  if (panel == NULL)
    {
      return -1;
    }

  g_ui.music_title = create_label(panel, "--", font_20(),
                                  lv_color_hex(0x293b53),
                                  LV_TEXT_ALIGN_CENTER);
  g_ui.music_status = create_label(panel, "--", font_14(),
                                   lv_color_hex(0x81939a),
                                   LV_TEXT_ALIGN_CENTER);
  g_ui.music_progress = lv_bar_create(panel);
  if (g_ui.music_title == NULL || g_ui.music_status == NULL ||
      g_ui.music_progress == NULL)
    {
      return -1;
    }

  strip_obj(g_ui.music_progress);
  place_label(g_ui.music_title, sx(14), sy(38), g_ui.screen_w - sx(72),
              sy(32));
  place_label(g_ui.music_status, sx(14), sy(74), g_ui.screen_w - sx(72),
              sy(24));
  lv_obj_set_pos(g_ui.music_progress, sx(34), sy(118));
  lv_obj_set_size(g_ui.music_progress, g_ui.screen_w - sx(112), sy(8));
  lv_obj_set_style_radius(g_ui.music_progress, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(g_ui.music_progress, lv_color_hex(0xf9d9d4), 0);
  lv_obj_set_style_bg_opa(g_ui.music_progress, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(g_ui.music_progress, lv_color_hex(0xf08d88),
                            LV_PART_INDICATOR);
  lv_obj_set_style_radius(g_ui.music_progress, LV_RADIUS_CIRCLE,
                          LV_PART_INDICATOR);
  lv_bar_set_range(g_ui.music_progress, 0, 100);

  if (create_action_button(g_ui.app_content, "Prev", sx(22), sy(212),
                           sx(64), sy(54), lv_color_hex(0x6f8790),
                           music_app_cb, 1) == NULL ||
      create_action_button(g_ui.app_content, "Play", sx(96), sy(212),
                           sx(74), sy(54), lv_color_hex(0xf08d88),
                           music_app_cb, 2) == NULL ||
      create_action_button(g_ui.app_content, "Next", sx(180), sy(212),
                           sx(64), sy(54), lv_color_hex(0x6f8790),
                           music_app_cb, 3) == NULL ||
      create_action_button(g_ui.app_content, "Vol", sx(254), sy(212),
                           sx(54), sy(54), lv_color_hex(0x80cbc3),
                           music_app_cb, 4) == NULL)
    {
      return -1;
    }

  update_music_app();
  return 0;
}

static int build_settings_app(void)
{
  lv_obj_t *bright_label;
  lv_obj_t *dnd_label;

  bright_label = create_label(g_ui.app_content, "Brightness", font_16(),
                              lv_color_hex(0x6f8790), LV_TEXT_ALIGN_LEFT);
  g_ui.setting_brightness = create_label(g_ui.app_content, "--", font_32(),
                                         lv_color_hex(0x293b53),
                                         LV_TEXT_ALIGN_LEFT);
  dnd_label = create_label(g_ui.app_content, "Do Not Disturb", font_16(),
                           lv_color_hex(0x6f8790), LV_TEXT_ALIGN_LEFT);
  g_ui.setting_dnd = create_label(g_ui.app_content, "--", font_32(),
                                  lv_color_hex(0x293b53),
                                  LV_TEXT_ALIGN_LEFT);
  if (bright_label == NULL || g_ui.setting_brightness == NULL ||
      dnd_label == NULL || g_ui.setting_dnd == NULL)
    {
      return -1;
    }

  place_label(bright_label, sx(28), sy(24), sx(160), sy(24));
  place_label(g_ui.setting_brightness, sx(28), sy(52), sx(120), sy(44));
  place_label(dnd_label, sx(28), sy(144), sx(180), sy(24));
  place_label(g_ui.setting_dnd, sx(28), sy(172), sx(120), sy(44));

  if (create_action_button(g_ui.app_content, "-", sx(180), sy(50),
                           sx(52), sy(52), lv_color_hex(0x6f8790),
                           settings_app_cb, 1) == NULL ||
      create_action_button(g_ui.app_content, "+", sx(246), sy(50),
                           sx(52), sy(52), lv_color_hex(0x80cbc3),
                           settings_app_cb, 2) == NULL ||
      create_action_button(g_ui.app_content, "DND", sx(180), sy(168),
                           sx(118), sy(54), lv_color_hex(0xa98bd6),
                           settings_app_cb, 3) == NULL)
    {
      return -1;
    }

  update_settings_app();
  return 0;
}

static int build_stopwatch_app(void)
{
  lv_obj_t *panel;

  panel = create_box(g_ui.app_content, sx(22), sy(18), g_ui.screen_w - sx(44),
                     sy(174), lv_color_hex(0xeef7ff), sx(28));
  if (panel == NULL)
    {
      return -1;
    }

  g_ui.stopwatch_display = create_label(panel, "00:00", font_time(),
                                        lv_color_hex(0x293b53),
                                        LV_TEXT_ALIGN_CENTER);
  g_ui.stopwatch_status = create_label(panel, "Paused", font_14(),
                                       lv_color_hex(0x6f8790),
                                       LV_TEXT_ALIGN_CENTER);
  if (g_ui.stopwatch_display == NULL || g_ui.stopwatch_status == NULL)
    {
      return -1;
    }

  place_label(g_ui.stopwatch_display, sx(12), sy(40),
              g_ui.screen_w - sx(68), sy(66));
  place_label(g_ui.stopwatch_status, sx(12), sy(116),
              g_ui.screen_w - sx(68), sy(24));

  if (create_action_button(g_ui.app_content, "Start", sx(54), sy(224),
                           sx(96), sy(54), lv_color_hex(0x73a1d6),
                           stopwatch_app_cb, 1) == NULL ||
      create_action_button(g_ui.app_content, "Reset", sx(180), sy(224),
                           sx(96), sy(54), lv_color_hex(0x6f8790),
                           stopwatch_app_cb, 2) == NULL)
    {
      return -1;
    }

  update_stopwatch_app();
  return 0;
}

static int build_flashlight_app(void)
{
  lv_obj_t *caption;

  g_ui.flashlight_panel = create_box(g_ui.app_content, sx(38), sy(20),
                                     g_ui.screen_w - sx(76), sy(220),
                                     lv_color_hex(0x263943), sx(34));
  g_ui.flashlight_status = create_label(g_ui.app_content, "Off", font_32(),
                                        lv_color_hex(0x293b53),
                                        LV_TEXT_ALIGN_CENTER);
  caption = create_label(g_ui.app_content, "Tap to toggle bright screen",
                         font_12(), lv_color_hex(0x81939a),
                         LV_TEXT_ALIGN_CENTER);
  if (g_ui.flashlight_panel == NULL || g_ui.flashlight_status == NULL ||
      caption == NULL)
    {
      return -1;
    }

  place_label(g_ui.flashlight_status, sx(20), sy(262),
              g_ui.screen_w - sx(40), sy(44));
  place_label(caption, sx(20), sy(308), g_ui.screen_w - sx(40), sy(20));

  if (create_action_button(g_ui.app_content, "Toggle", sx(106), sy(342),
                           sx(118), sy(54), lv_color_hex(0xf5c66e),
                           flashlight_app_cb, 1) == NULL)
    {
      return -1;
    }

  update_flashlight_app();
  return 0;
}

static int build_mines_app(void)
{
  lv_coord_t cell = sy(48);
  lv_coord_t gap = sx(8);
  lv_coord_t grid_w = cell * SMART_BAND_MINE_SIZE +
                      gap * (SMART_BAND_MINE_SIZE - 1);
  lv_coord_t start_x = (g_ui.screen_w - grid_w) / 2;
  lv_coord_t start_y = sy(48);

  g_ui.mine_mines = (uint16_t)((1u << 1) | (1u << 6) | (1u << 11));
  g_ui.mine_revealed = 0;
  g_ui.mine_over = false;

  g_ui.mine_status = create_label(g_ui.app_content, "Find the safe tiles",
                                  font_16(), lv_color_hex(0x293b53),
                                  LV_TEXT_ALIGN_CENTER);
  if (g_ui.mine_status == NULL)
    {
      return -1;
    }

  place_label(g_ui.mine_status, sx(18), sy(8), g_ui.screen_w - sx(36),
              sy(26));

  for (int i = 0; i < SMART_BAND_MINE_CELLS; i++)
    {
      int row = i / SMART_BAND_MINE_SIZE;
      int col = i % SMART_BAND_MINE_SIZE;

      g_ui.mine_cells[i] = create_action_button(g_ui.app_content, " ",
                                                start_x + col * (cell + gap),
                                                start_y + row * (cell + gap),
                                                cell, cell,
                                                lv_color_hex(0x80cbc3),
                                                mine_cell_cb,
                                                (uintptr_t)i);
      if (g_ui.mine_cells[i] == NULL)
        {
          return -1;
        }
    }

  if (create_action_button(g_ui.app_content, "New", sx(110), sy(284),
                           sx(110), sy(54), lv_color_hex(0x8aa8d8),
                           mine_new_cb, 0) == NULL)
    {
      return -1;
    }

  update_mines_app();
  return 0;
}

static int build_tetris_app(void)
{
  lv_coord_t cell = sy(25);
  lv_coord_t gap = sx(4);
  lv_coord_t grid_w = cell * SMART_BAND_TETRIS_COLS +
                      gap * (SMART_BAND_TETRIS_COLS - 1);
  lv_coord_t start_x = (g_ui.screen_w - grid_w) / 2;
  lv_coord_t start_y = sy(6);

  memset(g_ui.tetris_rows, 0, sizeof(g_ui.tetris_rows));
  g_ui.tetris_score = 0;
  g_ui.tetris_running = true;
  tetris_spawn();

  for (int row = 0; row < SMART_BAND_TETRIS_ROWS; row++)
    {
      for (int col = 0; col < SMART_BAND_TETRIS_COLS; col++)
        {
          int index = row * SMART_BAND_TETRIS_COLS + col;

          g_ui.tetris_cells[index] = create_box(g_ui.app_content,
                                                start_x +
                                                col * (cell + gap),
                                                start_y +
                                                row * (cell + gap),
                                                cell, cell,
                                                lv_color_hex(0xeaf4f2),
                                                sx(5));
          if (g_ui.tetris_cells[index] == NULL)
            {
              return -1;
            }
        }
    }

  g_ui.tetris_status = create_label(g_ui.app_content, "Score 0", font_16(),
                                    lv_color_hex(0x293b53),
                                    LV_TEXT_ALIGN_CENTER);
  if (g_ui.tetris_status == NULL)
    {
      return -1;
    }

  place_label(g_ui.tetris_status, sx(20), sy(244), g_ui.screen_w - sx(40),
              sy(26));

  if (create_action_button(g_ui.app_content, "Left", sx(34), sy(288),
                           sx(74), sy(52), lv_color_hex(0x6f8790),
                           tetris_app_cb, 1) == NULL ||
      create_action_button(g_ui.app_content, "Drop", sx(128), sy(288),
                           sx(74), sy(52), lv_color_hex(0x62bfb6),
                           tetris_app_cb, 2) == NULL ||
      create_action_button(g_ui.app_content, "New", sx(222), sy(288),
                           sx(74), sy(52), lv_color_hex(0x8aa8d8),
                           tetris_app_cb, 3) == NULL)
    {
      return -1;
    }

  update_tetris_app();
  return 0;
}

static int build_fish_app(void)
{
  lv_obj_t *fish;
  lv_obj_t *label;

  fish = create_action_button(g_ui.app_content, "KNOCK", sx(68), sy(38),
                              sx(194), sy(150), lv_color_hex(0xd9a85f),
                              fish_app_cb, 1);
  g_ui.fish_count = create_label(g_ui.app_content, "Merit 0", font_32(),
                                 lv_color_hex(0x293b53),
                                 LV_TEXT_ALIGN_CENTER);
  g_ui.fish_mode = create_label(g_ui.app_content, "Tap mode", font_16(),
                                lv_color_hex(0x6f8790),
                                LV_TEXT_ALIGN_CENTER);
  label = create_label(g_ui.app_content, "Each tap updates local state",
                       font_12(), lv_color_hex(0x81939a),
                       LV_TEXT_ALIGN_CENTER);
  if (fish == NULL || g_ui.fish_count == NULL || g_ui.fish_mode == NULL ||
      label == NULL)
    {
      return -1;
    }

  lv_obj_set_style_radius(fish, LV_RADIUS_CIRCLE, 0);
  place_label(g_ui.fish_count, sx(20), sy(210), g_ui.screen_w - sx(40),
              sy(44));
  place_label(g_ui.fish_mode, sx(20), sy(258), g_ui.screen_w - sx(40),
              sy(26));
  place_label(label, sx(20), sy(288), g_ui.screen_w - sx(40), sy(20));

  if (create_action_button(g_ui.app_content, "Auto", sx(54), sy(326),
                           sx(96), sy(50), lv_color_hex(0x80cbc3),
                           fish_app_cb, 2) == NULL ||
      create_action_button(g_ui.app_content, "Reset", sx(180), sy(326),
                           sx(96), sy(50), lv_color_hex(0x6f8790),
                           fish_app_cb, 3) == NULL)
    {
      return -1;
    }

  update_fish_app();
  return 0;
}

static int build_app_content(smart_band_app_id_t id)
{
  switch (id)
    {
      case SMART_BAND_APP_WEATHER:
        return build_weather_app();
      case SMART_BAND_APP_CALCULATOR:
        return build_calculator_app();
      case SMART_BAND_APP_TIMER:
        return build_timer_app();
      case SMART_BAND_APP_MUSIC:
        return build_music_app();
      case SMART_BAND_APP_SETTINGS:
        return build_settings_app();
      case SMART_BAND_APP_STOPWATCH:
        return build_stopwatch_app();
      case SMART_BAND_APP_FLASHLIGHT:
        return build_flashlight_app();
      case SMART_BAND_APP_MINES:
        return build_mines_app();
      case SMART_BAND_APP_TETRIS:
        return build_tetris_app();
      case SMART_BAND_APP_WOODEN_FISH:
        return build_fish_app();
      default:
        return -1;
    }
}

static void open_app(smart_band_app_id_t id)
{
  const smart_band_app_def_t *def = find_app_def(id);

  if (def == NULL || g_ui.app_content == NULL)
    {
      return;
    }

  lv_obj_clean(g_ui.app_content);
  clear_app_object_refs();
  g_ui.active_app = id;
  set_label_text(g_ui.app_title, def->title);

  if (build_app_content(id) != 0)
    {
      lv_obj_clean(g_ui.app_content);
      clear_app_object_refs();
      set_label_text(g_ui.app_title, "App failed");
    }

  set_page_visible(g_ui.apps_launcher, false);
  set_page_visible(g_ui.app_detail, true);
}

static int create_launcher_card(lv_obj_t *parent,
                                const smart_band_app_def_t *def,
                                lv_coord_t x, lv_coord_t y,
                                lv_coord_t w, lv_coord_t h)
{
  lv_obj_t *card = lv_btn_create(parent);
  lv_obj_t *icon;
  lv_obj_t *icon_text;
  lv_obj_t *title;

  if (card == NULL || def == NULL)
    {
      return -1;
    }

  strip_obj(card);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  lv_obj_set_style_bg_color(card, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, sx(18), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0xe6eeee), 0);
  lv_obj_set_style_shadow_width(card, sx(8), 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x314856), 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
  lv_obj_set_style_shadow_offset_y(card, sy(4), 0);
  lv_obj_add_event_cb(card, app_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)def->id);

  icon = create_box(card, (w - sx(42)) / 2, sy(8), sx(42), sx(42),
                    lv_color_hex(def->color), LV_RADIUS_CIRCLE);
  if (icon == NULL)
    {
      return -1;
    }

  lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(icon, app_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)def->id);

  icon_text = create_label(icon, def->icon, font_16(), lv_color_hex(0xffffff),
                           LV_TEXT_ALIGN_CENTER);
  title = create_label(card, def->title, font_12(), lv_color_hex(0x293b53),
                       LV_TEXT_ALIGN_CENTER);
  if (icon_text == NULL || title == NULL)
    {
      return -1;
    }

  lv_obj_add_flag(icon_text, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(icon_text, app_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)def->id);
  lv_obj_add_flag(title, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(title, app_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)def->id);
  place_label(icon_text, 0, sy(11), sx(42), sy(22));
  place_label(title, sx(6), sy(54), w - sx(12), sy(18));
  return 0;
}

static int create_apps_page(void)
{
  lv_obj_t *title;
  lv_coord_t margin = sx(20);
  lv_coord_t gap_x = sx(12);
  lv_coord_t gap_y = sy(8);
  lv_coord_t card_w = (g_ui.screen_w - margin * 2 - gap_x) / 2;
  lv_coord_t card_h = sy(74);
  lv_coord_t grid_y = sy(128);

  g_ui.apps_page = create_page(g_ui.screen);
  if (g_ui.apps_page == NULL ||
      create_leaf_mark(g_ui.apps_page, sy(22)) != 0 ||
      create_date_row(g_ui.apps_page, &g_ui.apps_date, sy(64)) != 0)
    {
      return -1;
    }

  title = create_label(g_ui.apps_page, "Apps", font_20(),
                       lv_color_hex(0x293b53), LV_TEXT_ALIGN_CENTER);
  if (title == NULL)
    {
      return -1;
    }

  place_label(title, sx(22), sy(96), g_ui.screen_w - sx(44), sy(28));

  g_ui.apps_launcher = create_plain_layer(g_ui.apps_page, 0, 0,
                                          g_ui.screen_w, g_ui.screen_h);
  g_ui.app_detail = create_plain_layer(g_ui.apps_page, 0, 0,
                                       g_ui.screen_w, g_ui.screen_h);
  if (g_ui.apps_launcher == NULL || g_ui.app_detail == NULL)
    {
      return -1;
    }

  lv_obj_set_style_bg_color(g_ui.app_detail, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_grad_color(g_ui.app_detail, lv_color_hex(0xfffcf6), 0);
  lv_obj_set_style_bg_grad_dir(g_ui.app_detail, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(g_ui.app_detail, LV_OPA_COVER, 0);

  for (int i = 0; i < SMART_BAND_APP_COUNT; i++)
    {
      int row = i / 2;
      int col = i % 2;

      if (create_launcher_card(g_ui.apps_launcher, &g_app_defs[i],
                               margin + col * (card_w + gap_x),
                               grid_y + row * (card_h + gap_y),
                               card_w, card_h) != 0)
        {
          return -1;
        }
    }

  g_ui.app_back = create_action_button(g_ui.app_detail, "<", sx(20), sy(28),
                                       sx(44), sy(38),
                                       lv_color_hex(0x6f8790), app_back_cb,
                                       0);
  g_ui.app_title = create_label(g_ui.app_detail, "Apps", font_20(),
                                lv_color_hex(0x293b53),
                                LV_TEXT_ALIGN_CENTER);
  g_ui.app_content = create_plain_layer(g_ui.app_detail, 0, sy(94),
                                        g_ui.screen_w, sy(452));
  if (g_ui.app_back == NULL || g_ui.app_title == NULL ||
      g_ui.app_content == NULL)
    {
      return -1;
    }

  place_label(g_ui.app_title, sx(72), sy(34), g_ui.screen_w - sx(144),
              sy(30));
  set_page_visible(g_ui.app_detail, false);
  g_ui.active_app = SMART_BAND_APP_NONE;
  return 0;
}

static int create_dots(void)
{
  lv_coord_t dot = sx(12);
  lv_coord_t gap = sx(10);
  lv_coord_t row_w = dot * SMART_BAND_PAGE_COUNT +
                     gap * (SMART_BAND_PAGE_COUNT - 1);
  lv_obj_t *row = lv_obj_create(g_ui.screen);

  if (row == NULL)
    {
      return -1;
    }

  strip_obj(row);
  lv_obj_set_size(row, row_w, sy(20));
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
      create_apps_page() != 0 || create_dots() != 0)
    {
      return -1;
    }

  enable_touch_navigation(g_ui.screen);
  enable_touch_navigation_tree(g_ui.face_page);
  enable_touch_navigation_tree(g_ui.heart_page);
  enable_touch_navigation_tree(g_ui.steps_page);
  enable_touch_navigation(g_ui.apps_page);
  return 0;
}

static void update_dots(void)
{
  for (int i = 0; i < SMART_BAND_PAGE_COUNT; i++)
    {
      lv_color_t color = i == (int)g_ui.model.page ? lv_color_hex(0x79c5be) :
                                                       lv_color_hex(0xc2d3d1);
      lv_obj_set_style_bg_color(g_ui.dots[i], color, 0);
      lv_obj_set_size(g_ui.dots[i], sx(12), sx(12));
    }
}

static void set_page_visible(lv_obj_t *page, bool visible)
{
  if (page == NULL)
    {
      return;
    }

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
  set_page_visible(g_ui.apps_page, g_ui.model.page == SMART_BAND_PAGE_APPS);

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

static void update_apps_page(void)
{
  char date_text[20];

  format_watch_date(date_text, sizeof(date_text));
  set_label_text(g_ui.apps_date, date_text);
  update_active_app();
}

static void render_page(void)
{
  update_face();
  update_heart_detail();
  update_steps_detail();
  update_apps_page();
  update_page_visibility();
}

static void app_icon_cb(lv_event_t *event)
{
  smart_band_app_id_t id =
    (smart_band_app_id_t)(uintptr_t)lv_event_get_user_data(event);

  open_app(id);
}

static void app_back_cb(lv_event_t *event)
{
  (void)event;

  if (g_ui.app_content != NULL)
    {
      lv_obj_clean(g_ui.app_content);
    }

  clear_app_object_refs();
  g_ui.active_app = SMART_BAND_APP_NONE;
  set_label_text(g_ui.app_title, "Apps");
  set_page_visible(g_ui.app_detail, false);
  set_page_visible(g_ui.apps_launcher, true);
}

static void calc_cb(lv_event_t *event)
{
  char key = (char)(uintptr_t)lv_event_get_user_data(event);
  size_t len;

  if (key >= '0' && key <= '9')
    {
      if (g_ui.calc_reset_next || strcmp(g_ui.calc_text, "0") == 0)
        {
          snprintf(g_ui.calc_text, sizeof(g_ui.calc_text), "%c", key);
          g_ui.calc_reset_next = false;
        }
      else
        {
          len = strlen(g_ui.calc_text);
          if (len + 1 < sizeof(g_ui.calc_text))
            {
              g_ui.calc_text[len] = key;
              g_ui.calc_text[len + 1] = '\0';
            }
        }
    }
  else if (key == '+' || key == '-')
    {
      g_ui.calc_lhs = atoi(g_ui.calc_text);
      g_ui.calc_op = key;
      g_ui.calc_has_lhs = true;
      g_ui.calc_reset_next = true;
    }
  else if (key == '=')
    {
      if (g_ui.calc_has_lhs)
        {
          int rhs = atoi(g_ui.calc_text);
          int result = g_ui.calc_op == '-' ? g_ui.calc_lhs - rhs :
                                             g_ui.calc_lhs + rhs;

          snprintf(g_ui.calc_text, sizeof(g_ui.calc_text), "%d", result);
          g_ui.calc_has_lhs = false;
          g_ui.calc_reset_next = true;
        }
    }
  else if (key == '<')
    {
      len = strlen(g_ui.calc_text);
      if (len > 1)
        {
          g_ui.calc_text[len - 1] = '\0';
        }
      else
        {
          snprintf(g_ui.calc_text, sizeof(g_ui.calc_text), "0");
        }
    }
  else
    {
      snprintf(g_ui.calc_text, sizeof(g_ui.calc_text), "0");
      g_ui.calc_has_lhs = false;
      g_ui.calc_reset_next = false;
    }

  update_calc_display();
}

static void timer_app_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);

  if (action == 1)
    {
      g_ui.timer_seconds += 60;
      if (g_ui.timer_seconds > 99 * 60)
        {
          g_ui.timer_seconds = 99 * 60;
        }
    }
  else if (action == 2)
    {
      if (g_ui.timer_seconds <= 0)
        {
          g_ui.timer_seconds = 5 * 60;
        }

      g_ui.timer_running = !g_ui.timer_running;
    }
  else
    {
      g_ui.timer_seconds = 5 * 60;
      g_ui.timer_running = false;
    }

  update_timer_app();
}

static void music_app_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);

  if (action == 1)
    {
      g_ui.music_track = (g_ui.music_track + 2) % 3;
    }
  else if (action == 2)
    {
      g_ui.music_playing = !g_ui.music_playing;
    }
  else if (action == 3)
    {
      g_ui.music_track = (g_ui.music_track + 1) % 3;
    }
  else
    {
      g_ui.music_volume += 10;
      if (g_ui.music_volume > 100)
        {
          g_ui.music_volume = 40;
        }
    }

  update_music_app();
}

static void settings_app_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);

  if (action == 1)
    {
      g_ui.brightness_percent -= 10;
      if (g_ui.brightness_percent < 10)
        {
          g_ui.brightness_percent = 10;
        }
    }
  else if (action == 2)
    {
      g_ui.brightness_percent += 10;
      if (g_ui.brightness_percent > 100)
        {
          g_ui.brightness_percent = 100;
        }
    }
  else
    {
      g_ui.dnd_enabled = !g_ui.dnd_enabled;
    }

  update_settings_app();
}

static void stopwatch_app_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);

  if (action == 1)
    {
      g_ui.stopwatch_running = !g_ui.stopwatch_running;
    }
  else
    {
      g_ui.stopwatch_seconds = 0;
      g_ui.stopwatch_running = false;
    }

  update_stopwatch_app();
}

static void flashlight_app_cb(lv_event_t *event)
{
  (void)event;

  g_ui.flashlight_on = !g_ui.flashlight_on;
  update_flashlight_app();
}

static void mine_cell_cb(lv_event_t *event)
{
  int index = (int)(uintptr_t)lv_event_get_user_data(event);

  if (index < 0 || index >= SMART_BAND_MINE_CELLS || g_ui.mine_over)
    {
      return;
    }

  g_ui.mine_revealed |= (uint16_t)(1u << index);
  if ((g_ui.mine_mines & (uint16_t)(1u << index)) != 0)
    {
      g_ui.mine_revealed |= g_ui.mine_mines;
      g_ui.mine_over = true;
    }

  update_mines_app();
}

static void mine_new_cb(lv_event_t *event)
{
  (void)event;

  g_ui.mine_mines = (uint16_t)((1u << 1) | (1u << 6) | (1u << 11));
  g_ui.mine_revealed = 0;
  g_ui.mine_over = false;
  update_mines_app();
}

static void tetris_new_game(void)
{
  memset(g_ui.tetris_rows, 0, sizeof(g_ui.tetris_rows));
  g_ui.tetris_score = 0;
  g_ui.tetris_running = true;
  tetris_spawn();
  update_tetris_app();
}

static void tetris_app_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);

  if (action == 1)
    {
      if (g_ui.tetris_running &&
          tetris_can_place(g_ui.tetris_x - 1, g_ui.tetris_y))
        {
          g_ui.tetris_x--;
        }
    }
  else if (action == 2)
    {
      if (!g_ui.tetris_running)
        {
          g_ui.tetris_running = true;
        }

      while (tetris_can_place(g_ui.tetris_x, g_ui.tetris_y + 1))
        {
          g_ui.tetris_y++;
        }

      tetris_step();
    }
  else
    {
      tetris_new_game();
      return;
    }

  update_tetris_app();
}

static void fish_app_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);

  if (action == 1)
    {
      g_ui.fish_merit++;
    }
  else if (action == 2)
    {
      g_ui.fish_auto = !g_ui.fish_auto;
    }
  else
    {
      g_ui.fish_merit = 0;
      g_ui.fish_auto = false;
    }

  update_fish_app();
}

static void timer_cb(lv_timer_t *timer)
{
  (void)timer;
  smart_band_state_tick(&g_ui.model, time(NULL));
  smart_band_sensor_bridge_update(&g_ui.sensors, &g_ui.model);

  if (g_ui.timer_running)
    {
      if (g_ui.timer_seconds > 0)
        {
          g_ui.timer_seconds--;
        }

      if (g_ui.timer_seconds <= 0)
        {
          g_ui.timer_seconds = 0;
          g_ui.timer_running = false;
        }
    }

  if (g_ui.active_app == SMART_BAND_APP_TETRIS &&
      (g_ui.model.ticks % 3u) == 0)
    {
      tetris_step();
    }

  if (g_ui.stopwatch_running)
    {
      g_ui.stopwatch_seconds++;
    }

  if (g_ui.active_app == SMART_BAND_APP_WOODEN_FISH && g_ui.fish_auto)
    {
      g_ui.fish_merit++;
    }

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
  if (obj == NULL)
    {
      return;
    }

  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj, page_drag_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(obj, page_drag_cb, LV_EVENT_RELEASED, NULL);
}

static void enable_touch_navigation_tree(lv_obj_t *obj)
{
  uint32_t child_count;

  if (obj == NULL)
    {
      return;
    }

  enable_touch_navigation(obj);

  child_count = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < child_count; i++)
    {
      enable_touch_navigation_tree(lv_obj_get_child(obj, i));
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
  g_ui.active_app = SMART_BAND_APP_NONE;
  snprintf(g_ui.calc_text, sizeof(g_ui.calc_text), "0");
  g_ui.timer_seconds = 5 * 60;
  g_ui.music_volume = 60;
  g_ui.brightness_percent = 70;
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
