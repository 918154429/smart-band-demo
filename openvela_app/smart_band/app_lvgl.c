#include "app_lvgl.h"

#include "watch_model.h"

#include <stdio.h>
#include <time.h>

typedef struct
{
  lv_obj_t *screen;
  lv_obj_t *title;
  lv_obj_t *clock;
  lv_obj_t *date;
  lv_obj_t *metric;
  lv_obj_t *unit;
  lv_obj_t *status;
  lv_obj_t *progress;
  lv_obj_t *battery;
  lv_obj_t *dots[SMART_BAND_PAGE_COUNT];
  lv_timer_t *timer;
  smart_band_state_t model;
} smart_band_ui_t;

static smart_band_ui_t g_ui;

static void render_page(void);

static void set_label_text(lv_obj_t *label, const char *text)
{
  if (label != NULL && text != NULL)
    {
      lv_label_set_text(label, text);
    }
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, lv_coord_t y,
                              lv_coord_t height, lv_color_t color)
{
  lv_obj_t *label = lv_label_create(parent);
  if (label == NULL)
    {
      return NULL;
    }

  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(label, lv_pct(90));
  lv_obj_set_height(label, height);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
  return label;
}

static void update_dots(void)
{
  for (int i = 0; i < SMART_BAND_PAGE_COUNT; i++)
    {
      if (g_ui.dots[i] == NULL)
        {
          continue;
        }

      lv_color_t color = i == (int)g_ui.model.page ? lv_color_hex(0x2dd4bf) :
                                                       lv_color_hex(0x475569);
      lv_obj_set_style_bg_color(g_ui.dots[i], color, 0);
      lv_obj_set_style_opa(g_ui.dots[i], LV_OPA_COVER, 0);
    }
}

static void render_face_page(void)
{
  char battery_text[24];
  snprintf(battery_text, sizeof(battery_text), "Battery %d%%",
           g_ui.model.battery_percent);

  set_label_text(g_ui.title, smart_band_page_title(g_ui.model.page));
  set_label_text(g_ui.clock, g_ui.model.time_text);
  set_label_text(g_ui.date, g_ui.model.date_text);
  set_label_text(g_ui.metric, g_ui.model.status_text);
  set_label_text(g_ui.unit, "Swipe left/right");
  set_label_text(g_ui.status, battery_text);
  lv_bar_set_range(g_ui.progress, 0, 100);
  lv_bar_set_value(g_ui.progress, g_ui.model.battery_percent, LV_ANIM_ON);
}

static void render_heart_page(void)
{
  char metric_text[16];
  snprintf(metric_text, sizeof(metric_text), "%d", g_ui.model.heart_rate);

  set_label_text(g_ui.title, smart_band_page_title(g_ui.model.page));
  set_label_text(g_ui.clock, "Heart Rate");
  set_label_text(g_ui.date, g_ui.model.time_text);
  set_label_text(g_ui.metric, metric_text);
  set_label_text(g_ui.unit, "BPM");
  set_label_text(g_ui.status, g_ui.model.heart_rate > 110 ? "High intensity" :
                                                           "Heart normal");
  lv_bar_set_range(g_ui.progress, 0, 100);
  lv_bar_set_value(g_ui.progress, (g_ui.model.heart_rate * 100) / 135, LV_ANIM_ON);
}

static void render_steps_page(void)
{
  char metric_text[16];
  char status_text[32];
  int progress = smart_band_step_progress(&g_ui.model);

  snprintf(metric_text, sizeof(metric_text), "%d", g_ui.model.steps);
  snprintf(status_text, sizeof(status_text), "Goal %d steps - %d%%",
           SMART_BAND_STEP_GOAL,
           progress);

  set_label_text(g_ui.title, smart_band_page_title(g_ui.model.page));
  set_label_text(g_ui.clock, "Today Steps");
  set_label_text(g_ui.date, g_ui.model.date_text);
  set_label_text(g_ui.metric, metric_text);
  set_label_text(g_ui.unit, "STEPS");
  set_label_text(g_ui.status, status_text);
  lv_bar_set_range(g_ui.progress, 0, 100);
  lv_bar_set_value(g_ui.progress, progress, LV_ANIM_ON);
}

static void render_page(void)
{
  if (g_ui.screen == NULL)
    {
      return;
    }

  switch (g_ui.model.page)
    {
      case SMART_BAND_PAGE_FACE:
        render_face_page();
        break;
      case SMART_BAND_PAGE_HEART:
        render_heart_page();
        break;
      case SMART_BAND_PAGE_STEPS:
        render_steps_page();
        break;
      default:
        set_label_text(g_ui.title, "Error");
        set_label_text(g_ui.clock, "--");
        set_label_text(g_ui.date, "Invalid page");
        set_label_text(g_ui.metric, "0");
        set_label_text(g_ui.unit, "");
        set_label_text(g_ui.status, "Restart app");
        break;
    }

  update_dots();
}

static void timer_cb(lv_timer_t *timer)
{
  (void)timer;
  smart_band_state_tick(&g_ui.model, time(NULL));
  render_page();
}

static void gesture_cb(lv_event_t *event)
{
  (void)event;

  lv_indev_t *indev = lv_indev_get_act();
  if (indev == NULL)
    {
      return;
    }

  lv_dir_t dir = lv_indev_get_gesture_dir(indev);
  if (dir == LV_DIR_LEFT)
    {
      smart_band_next_page(&g_ui.model);
      render_page();
    }
  else if (dir == LV_DIR_RIGHT)
    {
      smart_band_prev_page(&g_ui.model);
      render_page();
    }
}

static int create_dots(lv_obj_t *parent)
{
  lv_obj_t *row = lv_obj_create(parent);
  if (row == NULL)
    {
      return -1;
    }

  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, 58, 12);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 8, 0);

  for (int i = 0; i < SMART_BAND_PAGE_COUNT; i++)
    {
      g_ui.dots[i] = lv_obj_create(row);
      if (g_ui.dots[i] == NULL)
        {
          return -1;
        }

      lv_obj_remove_style_all(g_ui.dots[i]);
      lv_obj_set_size(g_ui.dots[i], 8, 8);
      lv_obj_set_style_radius(g_ui.dots[i], LV_RADIUS_CIRCLE, 0);
    }

  return 0;
}

int smart_band_lvgl_create(lv_obj_t *parent)
{
  lv_obj_t *root = parent != NULL ? parent : lv_scr_act();
  if (root == NULL)
    {
      return -1;
    }

  smart_band_state_init(&g_ui.model, time(NULL));
  g_ui.screen = root;

  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(root, gesture_cb, LV_EVENT_GESTURE, NULL);
  lv_obj_set_style_bg_color(root, lv_color_hex(0x07111f), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(root, 0, 0);

  g_ui.title = create_label(root, "Face", 14, 24, lv_color_hex(0x94a3b8));
  g_ui.clock = create_label(root, "--:--", 46, 36, lv_color_hex(0xf8fafc));
  g_ui.date = create_label(root, "----/--/--", 92, 24, lv_color_hex(0xcbd5e1));
  g_ui.metric = create_label(root, "0", 124, 34, lv_color_hex(0x2dd4bf));
  g_ui.unit = create_label(root, "", 164, 22, lv_color_hex(0x99f6e4));
  g_ui.status = create_label(root, "", 192, 24, lv_color_hex(0xe2e8f0));

  g_ui.progress = lv_bar_create(root);
  if (g_ui.title == NULL || g_ui.clock == NULL || g_ui.date == NULL ||
      g_ui.metric == NULL || g_ui.unit == NULL || g_ui.status == NULL ||
      g_ui.progress == NULL || create_dots(root) != 0)
    {
      return -1;
    }

  lv_obj_set_size(g_ui.progress, 132, 8);
  lv_obj_align(g_ui.progress, LV_ALIGN_TOP_MID, 0, 218);
  lv_obj_set_style_bg_color(g_ui.progress, lv_color_hex(0x1e293b), 0);
  lv_obj_set_style_bg_color(g_ui.progress, lv_color_hex(0x14b8a6), LV_PART_INDICATOR);
  lv_bar_set_range(g_ui.progress, 0, 100);

  g_ui.timer = lv_timer_create(timer_cb, 1000, NULL);
  if (g_ui.timer == NULL)
    {
      return -1;
    }

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
}
