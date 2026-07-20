#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#  define _POSIX_C_SOURCE 200809L
#endif

#include "app_lvgl.h"

#include "icon_assets.h"
#include "smart_band_runtime.h"
#include "ui/lvgl/components.h"
#include "ui/lvgl/watch_pages.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SMART_BAND_DEFAULT_TZ "CST-8"
#define SMART_BAND_SWIPE_CLICK_GUARD_MS 300

typedef struct
{
  lv_obj_t *root;
  lv_obj_t *watch;
  lv_obj_t *screen;
  smart_band_ui_components_t components;
  smart_band_watch_pages_t watch_pages;

  lv_obj_t *dots[SMART_BAND_PAGE_COUNT];

  lv_obj_t *apps_page;
  lv_obj_t *apps_date;
  lv_obj_t *apps_launcher;
  lv_obj_t *app_detail;
  lv_obj_t *app_title;
  lv_obj_t *app_content;
  lv_obj_t *app_back;

  lv_timer_t *timer;
  smart_band_runtime_t runtime;
  lv_coord_t screen_w;
  lv_coord_t screen_h;
  lv_point_t press_point;
  bool press_valid;
  bool page_swipe_consumed;
  uint32_t page_swipe_at;
  bool compact_band;
} smart_band_ui_t;

static smart_band_ui_t g_ui;

static void page_drag_cb(lv_event_t *event);
static void dot_click_cb(lv_event_t *event);
static void enable_touch_navigation(lv_obj_t *obj);
static void enable_touch_navigation_tree(lv_obj_t *obj);
static void set_page_visible(lv_obj_t *page, bool visible);
static void app_icon_cb(lv_event_t *event);
static void app_back_cb(lv_event_t *event);
static void step_goal_cb(lv_event_t *event);
static void render_page(void);
static void render_pending(void);
static lv_obj_t *create_action_button(lv_obj_t *parent, const char *text,
                                      lv_coord_t x, lv_coord_t y,
                                      lv_coord_t w, lv_coord_t h,
                                      lv_color_t color, lv_event_cb_t cb,
                                      uintptr_t data);

static time_t runtime_wall_now(void *context)
{
  (void)context;
  return time(NULL);
}

static uint32_t runtime_monotonic_now(void *context)
{
  (void)context;
  return lv_tick_get();
}

static const lv_font_t *font_12(void) { return smart_band_ui_font_12(); }
static const lv_font_t *font_14(void) { return smart_band_ui_font_14(); }
static const lv_font_t *font_16(void) { return smart_band_ui_font_16(); }
static const lv_font_t *font_20(void) { return smart_band_ui_font_20(); }
static const lv_font_t *font_32(void) { return smart_band_ui_font_32(); }
static const lv_font_t *font_time(void) { return smart_band_ui_font_time(); }
static lv_coord_t sx(int value) { return smart_band_ui_sx(&g_ui.components, value); }
static lv_coord_t sy(int value) { return smart_band_ui_sy(&g_ui.components, value); }
static lv_coord_t min_coord(lv_coord_t a, lv_coord_t b) { return smart_band_ui_min(a, b); }
static lv_coord_t max_coord(lv_coord_t a, lv_coord_t b) { return smart_band_ui_max(a, b); }
static lv_coord_t abs_coord(lv_coord_t value) { return smart_band_ui_abs(value); }
static void strip_obj(lv_obj_t *obj) { smart_band_ui_strip_obj(obj); }
static lv_obj_t *create_box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                            lv_coord_t w, lv_coord_t h, lv_color_t color,
                            lv_coord_t radius)
{
  return smart_band_ui_create_box(parent, x, y, w, h, color, radius);
}
static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, lv_color_t color,
                              lv_text_align_t align)
{
  return smart_band_ui_create_label(parent, text, font, color, align);
}
static void place_label(lv_obj_t *label, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h)
{
  smart_band_ui_place_label(label, x, y, w, h);
}
static void set_label_text(lv_obj_t *label, const char *text)
{
  smart_band_ui_set_label_text(label, text);
}
static void set_label_text_fmt_int(lv_obj_t *label, const char *fmt, int value)
{
  smart_band_ui_set_label_text_fmt_int(label, fmt, value);
}
static lv_obj_t *create_icon_image(lv_obj_t *parent,
                                   const lv_image_dsc_t *src,
                                   lv_coord_t x, lv_coord_t y,
                                   lv_coord_t size)
{
  return smart_band_ui_create_icon_image(parent, src, x, y, size);
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
  smart_band_ui_format_temperature(&g_ui.runtime.model, buffer, size);
}
static void format_duration(char *buffer, size_t size, int seconds)
{
  smart_band_ui_format_duration(buffer, size, seconds);
}
static void format_watch_date(char *buffer, size_t size)
{
  smart_band_ui_format_watch_date(&g_ui.runtime.model, buffer, size);
}
static lv_obj_t *create_page(lv_obj_t *parent)
{
  lv_obj_t *page = lv_obj_create(parent);
  if (page == NULL) return NULL;
  strip_obj(page);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_size(page, g_ui.screen_w, g_ui.screen_h);
  lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
  return page;
}

static int create_face_page(void)
{
  return smart_band_watch_pages_build_face(&g_ui.watch_pages, g_ui.screen,
                                           &g_ui.components,
                                           g_ui.compact_band);
}

static int create_heart_page(void)
{
  return smart_band_watch_pages_build_heart(&g_ui.watch_pages, g_ui.screen,
                                            &g_ui.components);
}

static int create_steps_page(void)
{
  return smart_band_watch_pages_build_steps(&g_ui.watch_pages, g_ui.screen,
                                            &g_ui.components, step_goal_cb);
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

static smart_band_app_host_t make_app_host(void)
{
  smart_band_app_host_t host;

  memset(&host, 0, sizeof(host));
  host.screen_w = g_ui.screen_w;
  host.screen_h = g_ui.screen_h;
  host.model = &g_ui.runtime.model;
  host.monotonic_now = runtime_monotonic_now;
  host.clock_context = g_ui.runtime.clock.source.context;
  host.sx = sx;
  host.sy = sy;
  host.font_12 = font_12;
  host.font_14 = font_14;
  host.font_16 = font_16;
  host.font_20 = font_20;
  host.font_32 = font_32;
  host.font_time = font_time;
  host.create_box = create_box;
  host.create_label = create_label;
  host.create_action_button = create_action_button;
  host.create_app_stat = create_app_stat;
  host.place_label = place_label;
  host.set_label_text = set_label_text;
  host.set_label_text_fmt_int = set_label_text_fmt_int;
  host.format_temperature = format_temperature;
  host.format_duration = format_duration;
  return host;
}

static void update_active_app(void)
{
  smart_band_app_host_t host = make_app_host();

  smart_band_app_render(&g_ui.runtime.apps, &host);
}

static void open_app(smart_band_app_id_t id)
{
  const smart_band_app_def_t *def = smart_band_app_find(id);
  smart_band_app_host_t host;

  if (def == NULL || g_ui.app_content == NULL)
    {
      return;
    }

  host = make_app_host();
  smart_band_app_unmount(&g_ui.runtime.apps);
  lv_obj_clean(g_ui.app_content);
  set_label_text(g_ui.app_title, def->title);

  if (smart_band_app_mount(&g_ui.runtime.apps, id, g_ui.app_content,
                           &host) != 0)
    {
      lv_obj_clean(g_ui.app_content);
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
  lv_obj_set_style_radius(card, sx(16), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0xe6eeee), 0);
  lv_obj_set_style_shadow_width(card, sx(5), 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x314856), 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
  lv_obj_set_style_shadow_offset_y(card, sy(2), 0);
  lv_obj_add_event_cb(card, app_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)def->id);

  icon = create_icon_image(card, def->icon, sx(8), sy(7),
                           sx(48));
  if (icon == NULL)
    {
      return -1;
    }

  lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(icon, app_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)def->id);

  title = create_label(card, def->title, font_12(), lv_color_hex(0x293b53),
                       LV_TEXT_ALIGN_CENTER);
  if (title == NULL)
    {
      return -1;
    }

  lv_obj_add_flag(title, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(title, app_icon_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)def->id);
  place_label(title, sx(60), sy(16), w - sx(66), sy(24));
  return 0;
}

static int create_apps_page(void)
{
  size_t app_count;
  const smart_band_app_def_t *apps = smart_band_apps_catalog(&app_count);
  lv_obj_t *title;
  lv_coord_t margin = sx(18);
  lv_coord_t gap_x = sx(10);
  lv_coord_t gap_y = sy(10);
  lv_coord_t card_w = (g_ui.screen_w - margin * 2 - gap_x) / 2;
  lv_coord_t card_h = sy(62);
  lv_coord_t grid_y = sy(126);

  g_ui.apps_page = create_page(g_ui.screen);
  if (g_ui.apps_page == NULL ||
      smart_band_watch_page_build_header(g_ui.apps_page, &g_ui.components,
                                         &g_ui.apps_date, sy(22), sy(64)) != 0)
    {
      return -1;
    }

  title = create_label(g_ui.apps_page, "Apps", font_20(),
                       lv_color_hex(0x293b53), LV_TEXT_ALIGN_CENTER);
  if (title == NULL)
    {
      return -1;
    }

  place_label(title, sx(22), sy(94), g_ui.screen_w - sx(44), sy(28));

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

  for (size_t i = 0; i < app_count; i++)
    {
      lv_coord_t row = (lv_coord_t)(i / 2u);
      lv_coord_t col = (lv_coord_t)(i % 2u);

      if (create_launcher_card(g_ui.apps_launcher, &apps[i],
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
  bool compact_band;

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

  compact_band = root_w <= 540 && root_h <= 540;
  g_ui.compact_band = compact_band;
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

  if (compact_band)
    {
      g_ui.watch = root;
      g_ui.screen = root;
      g_ui.screen_w = root_w;
      g_ui.screen_h = root_h;
      smart_band_ui_components_init(&g_ui.components, g_ui.screen_w,
                                    g_ui.screen_h);
      lv_obj_set_style_bg_color(root, lv_color_hex(0xffffff), 0);
      lv_obj_set_style_bg_grad_color(root, lv_color_hex(0xfffcf6), 0);
      lv_obj_set_style_bg_grad_dir(root, LV_GRAD_DIR_VER, 0);

      if (create_background_waves() != 0 || create_face_page() != 0 ||
          create_heart_page() != 0 || create_steps_page() != 0 ||
          create_apps_page() != 0 || create_dots() != 0)
        {
          return -1;
        }

      enable_touch_navigation(g_ui.screen);
      enable_touch_navigation_tree(g_ui.watch_pages.face_page);
      enable_touch_navigation_tree(g_ui.watch_pages.heart_page);
      enable_touch_navigation_tree(g_ui.watch_pages.steps_page);
      enable_touch_navigation_tree(g_ui.apps_launcher);
      return 0;
    }

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
  smart_band_ui_components_init(&g_ui.components, g_ui.screen_w,
                                g_ui.screen_h);
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
  enable_touch_navigation_tree(g_ui.watch_pages.face_page);
  enable_touch_navigation_tree(g_ui.watch_pages.heart_page);
  enable_touch_navigation_tree(g_ui.watch_pages.steps_page);
  enable_touch_navigation_tree(g_ui.apps_launcher);
  return 0;
}

static void update_dots(void)
{
  for (int i = 0; i < SMART_BAND_PAGE_COUNT; i++)
    {
      lv_color_t color = i == (int)g_ui.runtime.model.page ?
                         lv_color_hex(0x79c5be) : lv_color_hex(0xc2d3d1);
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
  set_page_visible(g_ui.watch_pages.face_page,
                   g_ui.runtime.model.page == SMART_BAND_PAGE_FACE);
  set_page_visible(g_ui.watch_pages.heart_page,
                   g_ui.runtime.model.page == SMART_BAND_PAGE_HEART);
  set_page_visible(g_ui.watch_pages.steps_page,
                   g_ui.runtime.model.page == SMART_BAND_PAGE_STEPS);
  set_page_visible(g_ui.apps_page,
                   g_ui.runtime.model.page == SMART_BAND_PAGE_APPS);

  update_dots();
}

static void switch_to_page(smart_band_page_t page)
{
  if (page >= SMART_BAND_PAGE_COUNT)
    {
      return;
    }

  g_ui.runtime.model.page = page;
  smart_band_runtime_mark_dirty(&g_ui.runtime, SMART_BAND_DIRTY_PAGE);
  render_pending();
}

static void update_face(void)
{
  smart_band_watch_pages_render_face(&g_ui.watch_pages, &g_ui.components,
                                     &g_ui.runtime.model, g_ui.compact_band);
}

static void update_heart_detail(void)
{
  smart_band_watch_pages_render_heart(&g_ui.watch_pages,
                                      &g_ui.runtime.model);
}

static void update_steps_detail(void)
{
  smart_band_watch_pages_render_steps(&g_ui.watch_pages,
                                      &g_ui.runtime.model);
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
  switch (g_ui.runtime.model.page)
    {
      case SMART_BAND_PAGE_FACE:
        update_face();
        break;
      case SMART_BAND_PAGE_HEART:
        update_heart_detail();
        break;
      case SMART_BAND_PAGE_STEPS:
        update_steps_detail();
        break;
      case SMART_BAND_PAGE_APPS:
        update_apps_page();
        break;
      default:
        break;
    }

  update_page_visibility();
}

static smart_band_dirty_flags_t current_page_dirty_mask(void)
{
  switch (g_ui.runtime.model.page)
    {
      case SMART_BAND_PAGE_FACE:
        return SMART_BAND_DIRTY_TIME | SMART_BAND_DIRTY_HEART |
               SMART_BAND_DIRTY_STEPS | SMART_BAND_DIRTY_BATTERY |
               SMART_BAND_DIRTY_ENVIRONMENT | SMART_BAND_DIRTY_STATUS |
               SMART_BAND_DIRTY_PAGE;
      case SMART_BAND_PAGE_HEART:
        return SMART_BAND_DIRTY_HEART | SMART_BAND_DIRTY_PAGE;
      case SMART_BAND_PAGE_STEPS:
        return SMART_BAND_DIRTY_STEPS | SMART_BAND_DIRTY_PAGE;
      case SMART_BAND_PAGE_APPS:
        return SMART_BAND_DIRTY_TIME | SMART_BAND_DIRTY_APP |
               SMART_BAND_DIRTY_PAGE;
      default:
        return SMART_BAND_DIRTY_PAGE;
    }
}

static void render_pending(void)
{
  smart_band_dirty_flags_t dirty =
    smart_band_runtime_take_dirty(&g_ui.runtime);

  if ((dirty & current_page_dirty_mask()) != 0)
    {
      render_page();
    }
}

static void app_icon_cb(lv_event_t *event)
{
  smart_band_app_id_t id =
    (smart_band_app_id_t)(uintptr_t)lv_event_get_user_data(event);

  if (g_ui.page_swipe_consumed &&
      lv_tick_elaps(g_ui.page_swipe_at) < SMART_BAND_SWIPE_CLICK_GUARD_MS)
    {
      return;
    }

  g_ui.page_swipe_consumed = false;
  open_app(id);
}

static void app_back_cb(lv_event_t *event)
{
  (void)event;

  smart_band_app_unmount(&g_ui.runtime.apps);
  if (g_ui.app_content != NULL)
    {
      lv_obj_clean(g_ui.app_content);
    }

  set_label_text(g_ui.app_title, "Apps");
  set_page_visible(g_ui.app_detail, false);
  set_page_visible(g_ui.apps_launcher, true);
}

static void step_goal_cb(lv_event_t *event)
{
  uintptr_t direction = (uintptr_t)lv_event_get_user_data(event);
  int delta = direction == 0 ? -SMART_BAND_STEP_GOAL_DELTA :
              SMART_BAND_STEP_GOAL_DELTA;

  smart_band_adjust_step_goal(&g_ui.runtime.model, delta);
  smart_band_runtime_mark_dirty(&g_ui.runtime, SMART_BAND_DIRTY_STEPS);
  render_pending();
}

static void timer_cb(lv_timer_t *timer)
{
  (void)timer;
  (void)smart_band_runtime_tick(
    &g_ui.runtime, g_ui.runtime.model.page == SMART_BAND_PAGE_APPS);

  render_pending();
}

static void next_page(void)
{
  smart_band_next_page(&g_ui.runtime.model);
  smart_band_runtime_mark_dirty(&g_ui.runtime, SMART_BAND_DIRTY_PAGE);
  render_pending();
}

static void prev_page(void)
{
  smart_band_prev_page(&g_ui.runtime.model);
  smart_band_runtime_mark_dirty(&g_ui.runtime, SMART_BAND_DIRTY_PAGE);
  render_pending();
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
      g_ui.page_swipe_consumed = false;
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

  g_ui.page_swipe_consumed = true;
  g_ui.page_swipe_at = lv_tick_get();
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
  lv_obj_t *parent_root = parent != NULL ? parent : lv_scr_act();
  lv_obj_t *owned_root;
  lv_coord_t root_w;
  lv_coord_t root_h;
  smart_band_clock_source_t clock_source;

  if (parent_root == NULL)
    {
      return -1;
    }

  if (g_ui.root != NULL || g_ui.timer != NULL || g_ui.runtime.initialized)
    {
      smart_band_lvgl_destroy();
    }

  memset(&g_ui, 0, sizeof(g_ui));
  lv_obj_update_layout(parent_root);
  root_w = lv_obj_get_width(parent_root);
  root_h = lv_obj_get_height(parent_root);
  if (root_w <= 0 || root_h <= 0)
    {
      return -1;
    }

  owned_root = lv_obj_create(parent_root);
  if (owned_root == NULL)
    {
      return -1;
    }

  strip_obj(owned_root);
  lv_obj_set_pos(owned_root, 0, 0);
  lv_obj_set_size(owned_root, root_w, root_h);

  g_ui.root = owned_root;
  configure_local_time();
  memset(&clock_source, 0, sizeof(clock_source));
  clock_source.wall_now = runtime_wall_now;
  clock_source.monotonic_now = runtime_monotonic_now;
  if (smart_band_runtime_init(&g_ui.runtime, &clock_source, NULL) != 0)
    {
      lv_obj_del(owned_root);
      g_ui.root = NULL;
      return -1;
    }

  if (create_ui_tree(owned_root) != 0)
    {
      smart_band_runtime_deinit(&g_ui.runtime);
      lv_obj_del(owned_root);
      g_ui.root = NULL;
      return -1;
    }

  g_ui.timer = lv_timer_create(timer_cb, 1000, NULL);
  if (g_ui.timer == NULL)
    {
      smart_band_runtime_deinit(&g_ui.runtime);
      lv_obj_del(owned_root);
      g_ui.root = NULL;
      return -1;
    }

  (void)smart_band_runtime_refresh_sensors(&g_ui.runtime);
  render_pending();
  return 0;
}

void smart_band_lvgl_destroy(void)
{
  if (g_ui.timer != NULL)
    {
      lv_timer_del(g_ui.timer);
      g_ui.timer = NULL;
    }

  smart_band_runtime_deinit(&g_ui.runtime);

  if (g_ui.root != NULL)
    {
      if (lv_obj_is_valid(g_ui.root))
        {
          lv_obj_del(g_ui.root);
        }

      g_ui.root = NULL;
    }

  g_ui.watch = NULL;
  g_ui.screen = NULL;
}
