#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static lv_obj_t *g_stage;
static lv_obj_t *g_count;
static lv_obj_t *g_hint;
static lv_obj_t *g_reset_note;
static const lv_font_t *g_float_font;
static lv_coord_t g_float_start_y;
static int g_merit;
static uint32_t g_last_tap_tick;

static void wooden_fish_update_count(void)
{
  char value[32];

  snprintf(value, sizeof(value), "功德 %d", g_merit);
  if (g_count != NULL)
    {
      lv_label_set_text(g_count, value);
    }
}

void smart_band_wooden_fish_app_update(const smart_band_app_host_t *host)
{
  (void)host;
  wooden_fish_update_count();
}

static void merit_anim_y_cb(void *obj, int32_t value)
{
  lv_obj_set_y((lv_obj_t *)obj, value);
}

static void merit_anim_opa_cb(void *obj, int32_t value)
{
  lv_obj_set_style_text_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
}

static void merit_anim_ready_cb(lv_anim_t *anim)
{
  lv_obj_t *obj = (lv_obj_t *)anim->var;

  if (obj != NULL && lv_obj_is_valid(obj))
    {
      lv_obj_del(obj);
    }
}

static void show_merit_animation(void)
{
  lv_obj_t *label;
  lv_anim_t move_anim;
  lv_anim_t fade_anim;

  if (g_stage == NULL || g_float_font == NULL)
    {
      return;
    }

  label = lv_label_create(g_stage);
  if (label == NULL)
    {
      return;
    }

  lv_label_set_text(label, "功德+1");
  lv_obj_set_style_text_font(label, g_float_font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xd99a32), 0);
  lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(label, 0, 0);
  lv_obj_set_pos(label, 0, g_float_start_y);
  lv_obj_set_size(label, lv_obj_get_width(g_stage), 28);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

  lv_anim_init(&move_anim);
  lv_anim_set_var(&move_anim, label);
  lv_anim_set_values(&move_anim, g_float_start_y, g_float_start_y - 42);
  lv_anim_set_exec_cb(&move_anim, merit_anim_y_cb);
  lv_anim_set_time(&move_anim, 760);
  lv_anim_set_path_cb(&move_anim, lv_anim_path_ease_out);

  lv_anim_init(&fade_anim);
  lv_anim_set_var(&fade_anim, label);
  lv_anim_set_values(&fade_anim, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_exec_cb(&fade_anim, merit_anim_opa_cb);
  lv_anim_set_time(&fade_anim, 760);
  lv_anim_set_path_cb(&fade_anim, lv_anim_path_ease_out);
  lv_anim_set_ready_cb(&fade_anim, merit_anim_ready_cb);

  lv_anim_start(&move_anim);
  lv_anim_start(&fade_anim);
}

void smart_band_wooden_fish_app_tick(const smart_band_app_host_t *host)
{
  (void)host;

  if (g_hint != NULL && g_last_tap_tick != 0 &&
      lv_tick_get() - g_last_tap_tick > 1200)
    {
      lv_label_set_text(g_hint, "速度 平稳");
    }
}

static void update_speed_hint(uint32_t now)
{
  char text[40];
  uint32_t delta;

  if (g_hint == NULL)
    {
      return;
    }

  if (g_last_tap_tick == 0)
    {
      lv_label_set_text(g_hint, "速度 已开始");
      return;
    }

  delta = now - g_last_tap_tick;
  if (delta < 220)
    {
      lv_label_set_text(g_hint, "慢一点  过快");
      return;
    }

  snprintf(text, sizeof(text), "速度 %lu/min",
           (unsigned long)(60000u / delta));
  lv_label_set_text(g_hint, text);
}

static void fish_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);
  uint32_t now = lv_tick_get();

  if (action == 1)
    {
      g_merit++;
      update_speed_hint(now);
      g_last_tap_tick = now;
      if (g_reset_note != NULL)
        {
          lv_label_set_text(g_reset_note, "");
        }

      wooden_fish_update_count();
      show_merit_animation();
      return;
    }

  g_merit = 0;
  g_last_tap_tick = 0;
  if (g_hint != NULL)
    {
      lv_label_set_text(g_hint, "速度 准备好");
    }

  if (g_reset_note != NULL)
    {
      lv_label_set_text(g_reset_note, "功德化成您的好运");
    }

  wooden_fish_update_count();
}

static int add_tappable(lv_obj_t *obj)
{
  if (obj == NULL)
    {
      return -1;
    }

  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj, fish_cb, LV_EVENT_CLICKED, (void *)1);
  return 0;
}

int smart_band_wooden_fish_app_build(lv_obj_t *parent,
                                     const smart_band_app_host_t *host)
{
  lv_coord_t center = host->screen_w / 2;
  lv_obj_t *body;
  lv_obj_t *belly;
  lv_obj_t *head;
  lv_obj_t *left_ear;
  lv_obj_t *right_ear;
  lv_obj_t *snout;
  lv_obj_t *reset;

  g_stage = parent;
  g_count = NULL;
  g_hint = NULL;
  g_reset_note = NULL;
  g_float_font = host->font_20();
  g_float_start_y = host->sy(58);
  g_last_tap_tick = 0;

  g_count = host->create_label(parent, "功德 0", host->font_20(),
                               lv_color_hex(0x293b53),
                               LV_TEXT_ALIGN_CENTER);
  g_hint = host->create_label(parent, "速度 准备好", host->font_12(),
                              lv_color_hex(0x6f8790),
                              LV_TEXT_ALIGN_CENTER);
  g_reset_note = host->create_label(parent, "", host->font_12(),
                                    lv_color_hex(0xd99a32),
                                    LV_TEXT_ALIGN_CENTER);
  if (g_count == NULL || g_hint == NULL || g_reset_note == NULL)
    {
      return -1;
    }

  host->place_label(g_count, host->sx(18), host->sy(2),
                    host->screen_w - host->sx(36), host->sy(28));
  host->place_label(g_hint, host->sx(18), host->sy(30),
                    host->screen_w - host->sx(36), host->sy(22));

  if (host->create_box(parent, center - host->sx(73), host->sy(238),
                       host->sx(146), host->sy(24), lv_color_hex(0xf3ead7),
                       LV_RADIUS_CIRCLE) == NULL ||
      host->create_box(parent, center - host->sx(82), host->sy(218),
                       host->sx(66), host->sy(42), lv_color_hex(0xf5b4c8),
                       host->sx(24)) == NULL ||
      host->create_box(parent, center + host->sx(16), host->sy(218),
                       host->sx(66), host->sy(42), lv_color_hex(0xf5b4c8),
                       host->sx(24)) == NULL ||
      host->create_box(parent, center - host->sx(38), host->sy(210),
                       host->sx(76), host->sy(52), lv_color_hex(0xf7c7d5),
                       host->sx(28)) == NULL ||
      host->create_box(parent, center - host->sx(96), host->sy(250),
                       host->sx(192), host->sy(30), lv_color_hex(0xaad9c8),
                       LV_RADIUS_CIRCLE) == NULL)
    {
      return -1;
    }

  body = host->create_box(parent, center - host->sx(58), host->sy(132),
                          host->sx(116), host->sy(104),
                          lv_color_hex(0xb98358), host->sx(42));
  belly = host->create_box(parent, center - host->sx(39), host->sy(158),
                           host->sx(78), host->sy(64),
                           lv_color_hex(0xd9b28a), host->sx(32));
  left_ear = host->create_box(parent, center - host->sx(56), host->sy(72),
                              host->sx(35), host->sx(35),
                              lv_color_hex(0x9d6a49), LV_RADIUS_CIRCLE);
  right_ear = host->create_box(parent, center + host->sx(21), host->sy(72),
                               host->sx(35), host->sx(35),
                               lv_color_hex(0x9d6a49), LV_RADIUS_CIRCLE);
  head = host->create_box(parent, center - host->sx(63), host->sy(76),
                          host->sx(126), host->sy(92),
                          lv_color_hex(0xb98358), host->sx(42));
  snout = host->create_box(parent, center - host->sx(35), host->sy(120),
                           host->sx(70), host->sy(30),
                           lv_color_hex(0xd9b28a), LV_RADIUS_CIRCLE);
  if (body == NULL || belly == NULL || left_ear == NULL ||
      right_ear == NULL || head == NULL || snout == NULL ||
      add_tappable(head) != 0)
    {
      return -1;
    }

  lv_obj_set_style_border_width(head, 1, 0);
  lv_obj_set_style_border_color(head, lv_color_hex(0x8d5d42), 0);

  if (host->create_box(parent, center - host->sx(32), host->sy(110),
                       host->sx(10), host->sx(10), lv_color_hex(0x293b53),
                       LV_RADIUS_CIRCLE) == NULL ||
      host->create_box(parent, center + host->sx(22), host->sy(110),
                       host->sx(10), host->sx(10), lv_color_hex(0x293b53),
                       LV_RADIUS_CIRCLE) == NULL ||
      host->create_box(parent, center - host->sx(8), host->sy(131),
                       host->sx(16), host->sy(9), lv_color_hex(0x6b4632),
                       LV_RADIUS_CIRCLE) == NULL ||
      host->create_box(parent, center - host->sx(18), host->sy(150),
                       host->sx(36), host->sy(4), lv_color_hex(0x6b4632),
                       LV_RADIUS_CIRCLE) == NULL)
    {
      return -1;
    }

  host->place_label(g_reset_note, host->sx(18), host->sy(286),
                    host->screen_w - host->sx(36), host->sy(22));
  reset = host->create_action_button(parent, "Reset",
                                     center - host->sx(52), host->sy(314),
                                     host->sx(104), host->sy(36),
                                     lv_color_hex(0x6f8790), fish_cb, 2);
  if (reset == NULL)
    {
      return -1;
    }

  wooden_fish_update_count();
  return 0;
}
