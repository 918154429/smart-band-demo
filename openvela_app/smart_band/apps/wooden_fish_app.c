#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WOODEN_FISH_ACTION_KNOCK 1u
#define WOODEN_FISH_ACTION_RESET 2u
#define WOODEN_FISH_ACTION_COUNT 2

typedef enum
{
  WOODEN_FISH_HINT_READY = 0,
  WOODEN_FISH_HINT_FIRST,
  WOODEN_FISH_HINT_TOO_FAST,
  WOODEN_FISH_HINT_SPEED,
  WOODEN_FISH_HINT_STEADY
} wooden_fish_hint_t;

typedef struct
{
  int merit;
  uint32_t last_tap_ms;
  bool has_last_tap;
  uint32_t speed_per_minute;
  wooden_fish_hint_t hint;
  bool show_reset_note;
  bool animate_merit;

  lv_obj_t *stage;
  lv_obj_t *count;
  lv_obj_t *hint_label;
  lv_obj_t *reset_note;
  const lv_font_t *float_font;
  lv_coord_t float_start_y;
  bool mounted;
  smart_band_app_monotonic_now_fn monotonic_now;
  void *clock_context;
  smart_band_app_event_binding_t bindings[WOODEN_FISH_ACTION_COUNT];
} wooden_fish_context_t;

_Static_assert(sizeof(wooden_fish_context_t) <=
               SMART_BAND_APP_CONTEXT_CAPACITY,
               "wooden fish app context exceeds runtime capacity");

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

static void show_merit_animation(wooden_fish_context_t *context)
{
  lv_obj_t *label;
  lv_anim_t move_anim;
  lv_anim_t fade_anim;

  if (context->stage == NULL || context->float_font == NULL)
    {
      return;
    }

  label = lv_label_create(context->stage);
  if (label == NULL)
    {
      return;
    }

  lv_label_set_text(label, "Merit +1");
  lv_obj_set_style_text_font(label, context->float_font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xd99a32), 0);
  lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(label, 0, 0);
  lv_obj_set_pos(label, 0, context->float_start_y);
  lv_obj_set_size(label, lv_obj_get_width(context->stage), 28);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

  lv_anim_init(&move_anim);
  lv_anim_set_var(&move_anim, label);
  lv_anim_set_values(&move_anim, context->float_start_y,
                     context->float_start_y - 42);
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

static const char *wooden_fish_hint_text(const wooden_fish_context_t *context,
                                         char *buffer, size_t size)
{
  switch (context->hint)
    {
      case WOODEN_FISH_HINT_FIRST:
        return "First knock";

      case WOODEN_FISH_HINT_TOO_FAST:
        return "Too fast";

      case WOODEN_FISH_HINT_SPEED:
        snprintf(buffer, size, "Speed %lu/min",
                 (unsigned long)context->speed_per_minute);
        return buffer;

      case WOODEN_FISH_HINT_STEADY:
        return "Speed steady";

      case WOODEN_FISH_HINT_READY:
      default:
        return "Ready to knock";
    }
}

static void wooden_fish_render(void *opaque,
                               const smart_band_app_host_t *host)
{
  wooden_fish_context_t *context = opaque;
  char count[32];
  char hint[40];

  (void)host;
  if (context == NULL || !context->mounted)
    {
      return;
    }

  snprintf(count, sizeof(count), "Merit %d", context->merit);
  if (context->count != NULL)
    {
      lv_label_set_text(context->count, count);
    }

  if (context->hint_label != NULL)
    {
      lv_label_set_text(context->hint_label,
                        wooden_fish_hint_text(context, hint, sizeof(hint)));
    }

  if (context->reset_note != NULL)
    {
      lv_label_set_text(context->reset_note,
                        context->show_reset_note ?
                        "Merit becomes your luck" : "");
    }

  if (context->animate_merit)
    {
      context->animate_merit = false;
      show_merit_animation(context);
    }
}

static void wooden_fish_update_speed(wooden_fish_context_t *context,
                                     uint32_t now_ms)
{
  uint32_t delta;

  if (!context->has_last_tap)
    {
      context->hint = WOODEN_FISH_HINT_FIRST;
      context->speed_per_minute = 0;
      return;
    }

  delta = now_ms - context->last_tap_ms;
  if (delta < 220u)
    {
      context->hint = WOODEN_FISH_HINT_TOO_FAST;
      context->speed_per_minute = 0;
      return;
    }

  context->hint = WOODEN_FISH_HINT_SPEED;
  context->speed_per_minute = 60000u / delta;
}

static void fish_cb(lv_event_t *event)
{
  smart_band_app_event_binding_t *binding =
    (smart_band_app_event_binding_t *)lv_event_get_user_data(event);
  wooden_fish_context_t *context;
  uint32_t now_ms;

  if (binding == NULL || binding->context == NULL)
    {
      return;
    }

  context = binding->context;
  if (binding->action == WOODEN_FISH_ACTION_KNOCK)
    {
      now_ms = context->monotonic_now == NULL ? lv_tick_get() :
               context->monotonic_now(context->clock_context);
      context->merit++;
      wooden_fish_update_speed(context, now_ms);
      context->last_tap_ms = now_ms;
      context->show_reset_note = false;
      context->animate_merit = true;
      context->has_last_tap = true;
    }
  else if (binding->action == WOODEN_FISH_ACTION_RESET)
    {
      context->merit = 0;
      context->last_tap_ms = 0;
      context->has_last_tap = false;
      context->speed_per_minute = 0;
      context->hint = WOODEN_FISH_HINT_READY;
      context->show_reset_note = true;
      context->animate_merit = false;
    }
  else
    {
      return;
    }

  wooden_fish_render(context, NULL);
}

static int add_tappable(lv_obj_t *obj,
                        smart_band_app_event_binding_t *binding)
{
  if (obj == NULL || binding == NULL)
    {
      return -1;
    }

  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj, fish_cb, LV_EVENT_CLICKED, binding);
  return 0;
}

static lv_obj_t *create_part(const smart_band_app_host_t *host,
                             lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                             lv_coord_t w, lv_coord_t h, uint32_t color,
                             lv_coord_t radius)
{
  lv_obj_t *part = host->create_box(parent, x, y, w, h,
                                    lv_color_hex(color), radius);

  if (part != NULL)
    {
      lv_obj_set_style_shadow_width(part, 0, 0);
    }

  return part;
}

static int wooden_fish_init(void *opaque)
{
  wooden_fish_context_t *context = opaque;

  if (context == NULL)
    {
      return -1;
    }

  memset(context, 0, sizeof(*context));
  context->hint = WOODEN_FISH_HINT_READY;
  return 0;
}

static void wooden_fish_unmount(void *opaque)
{
  wooden_fish_context_t *context = opaque;

  if (context == NULL)
    {
      return;
    }

  context->stage = NULL;
  context->count = NULL;
  context->hint_label = NULL;
  context->reset_note = NULL;
  context->float_font = NULL;
  context->float_start_y = 0;
  context->mounted = false;
  context->animate_merit = false;
  memset(context->bindings, 0, sizeof(context->bindings));
}

static int wooden_fish_mount(void *opaque, lv_obj_t *parent,
                             const smart_band_app_host_t *host)
{
  wooden_fish_context_t *context = opaque;
  lv_coord_t center;
  lv_obj_t *head;
  lv_obj_t *body;
  lv_obj_t *belly;
  lv_obj_t *snout;
  lv_obj_t *hammer_handle;
  lv_obj_t *hammer_head;
  lv_obj_t *knock;
  lv_obj_t *reset;

  if (context == NULL || parent == NULL || host == NULL)
    {
      return -1;
    }

  wooden_fish_unmount(context);
  center = host->screen_w / 2;
  context->stage = parent;
  context->monotonic_now = host->monotonic_now;
  context->clock_context = host->clock_context;
  context->float_font = host->font_20();
  context->float_start_y = host->sy(56);
  context->bindings[0].context = context;
  context->bindings[0].action = WOODEN_FISH_ACTION_KNOCK;
  context->bindings[1].context = context;
  context->bindings[1].action = WOODEN_FISH_ACTION_RESET;

  context->count = host->create_label(parent, "Merit 0", host->font_20(),
                                      lv_color_hex(0x293b53),
                                      LV_TEXT_ALIGN_CENTER);
  context->hint_label =
    host->create_label(parent, "Ready to knock", host->font_12(),
                       lv_color_hex(0x6f8790), LV_TEXT_ALIGN_CENTER);
  context->reset_note = host->create_label(parent, "", host->font_12(),
                                           lv_color_hex(0xd99a32),
                                           LV_TEXT_ALIGN_CENTER);
  if (context->count == NULL || context->hint_label == NULL ||
      context->reset_note == NULL)
    {
      wooden_fish_unmount(context);
      return -1;
    }

  host->place_label(context->count, host->sx(18), host->sy(2),
                    host->screen_w - host->sx(36), host->sy(28));
  host->place_label(context->hint_label, host->sx(18), host->sy(30),
                    host->screen_w - host->sx(36), host->sy(20));

  if (create_part(host, parent, center - host->sx(94), host->sy(243),
                  host->sx(188), host->sy(28), 0xaad9c8,
                  LV_RADIUS_CIRCLE) == NULL ||
      create_part(host, parent, center - host->sx(76), host->sy(221),
                  host->sx(62), host->sy(42), 0xf5a9c3,
                  host->sx(24)) == NULL ||
      create_part(host, parent, center + host->sx(14), host->sy(221),
                  host->sx(62), host->sy(42), 0xf5a9c3,
                  host->sx(24)) == NULL ||
      create_part(host, parent, center - host->sx(42), host->sy(210),
                  host->sx(84), host->sy(56), 0xf7c4d5,
                  host->sx(28)) == NULL ||
      create_part(host, parent, center - host->sx(66), host->sy(252),
                  host->sx(132), host->sy(18), 0xf3ead7,
                  LV_RADIUS_CIRCLE) == NULL)
    {
      wooden_fish_unmount(context);
      return -1;
    }

  body = create_part(host, parent, center - host->sx(70), host->sy(122),
                     host->sx(140), host->sy(108), 0xba8257,
                     host->sx(48));
  belly = create_part(host, parent, center - host->sx(38), host->sy(150),
                      host->sx(76), host->sy(60), 0xdbb083,
                      host->sx(30));
  if (body == NULL || belly == NULL)
    {
      wooden_fish_unmount(context);
      return -1;
    }

  if (create_part(host, parent, center - host->sx(60), host->sy(76),
                  host->sx(34), host->sx(34), 0x9b6947,
                  LV_RADIUS_CIRCLE) == NULL ||
      create_part(host, parent, center + host->sx(26), host->sy(76),
                  host->sx(34), host->sx(34), 0x9b6947,
                  LV_RADIUS_CIRCLE) == NULL)
    {
      wooden_fish_unmount(context);
      return -1;
    }

  head = create_part(host, parent, center - host->sx(62), host->sy(82),
                     host->sx(124), host->sy(92), 0xbe875c,
                     host->sx(42));
  snout = create_part(host, parent, center - host->sx(34), host->sy(126),
                      host->sx(68), host->sy(30), 0xe0b98f,
                      LV_RADIUS_CIRCLE);
  if (head == NULL || snout == NULL ||
      add_tappable(head, &context->bindings[0]) != 0 ||
      add_tappable(body, &context->bindings[0]) != 0)
    {
      wooden_fish_unmount(context);
      return -1;
    }

  lv_obj_set_style_border_width(head, 1, 0);
  lv_obj_set_style_border_color(head, lv_color_hex(0x8d5d42), 0);

  if (create_part(host, parent, center - host->sx(30), host->sy(116),
                  host->sx(10), host->sx(10), 0x293b53,
                  LV_RADIUS_CIRCLE) == NULL ||
      create_part(host, parent, center + host->sx(20), host->sy(116),
                  host->sx(10), host->sx(10), 0x293b53,
                  LV_RADIUS_CIRCLE) == NULL ||
      create_part(host, parent, center - host->sx(7), host->sy(137),
                  host->sx(14), host->sy(8), 0x6b4632,
                  LV_RADIUS_CIRCLE) == NULL ||
      create_part(host, parent, center - host->sx(18), host->sy(154),
                  host->sx(36), host->sy(4), 0x6b4632,
                  LV_RADIUS_CIRCLE) == NULL ||
      create_part(host, parent, center - host->sx(88), host->sy(206),
                  host->sx(32), host->sy(18), 0x8f6043,
                  LV_RADIUS_CIRCLE) == NULL ||
      create_part(host, parent, center + host->sx(56), host->sy(206),
                  host->sx(32), host->sy(18), 0x8f6043,
                  LV_RADIUS_CIRCLE) == NULL)
    {
      wooden_fish_unmount(context);
      return -1;
    }

  hammer_handle = create_part(host, parent, center + host->sx(70),
                              host->sy(70), host->sx(15), host->sy(90),
                              0x7b5a3e, host->sx(7));
  hammer_head = create_part(host, parent, center + host->sx(47),
                            host->sy(58), host->sx(58), host->sy(24),
                            0xc58d5c, host->sx(12));
  if (hammer_handle == NULL || hammer_head == NULL)
    {
      wooden_fish_unmount(context);
      return -1;
    }

  lv_obj_set_style_transform_rotation(hammer_handle, -280, 0);
  lv_obj_set_style_transform_rotation(hammer_head, -280, 0);
  lv_obj_set_style_border_width(hammer_head, 1, 0);
  lv_obj_set_style_border_color(hammer_head, lv_color_hex(0x8d5d42), 0);

  host->place_label(context->reset_note, host->sx(18), host->sy(278),
                    host->screen_w - host->sx(36), host->sy(20));

  knock = host->create_action_button(
    parent, "Knock", center - host->sx(112), host->sy(308), host->sx(96),
    host->sy(38), lv_color_hex(0xf5c66e), fish_cb,
    (uintptr_t)&context->bindings[0]);
  reset = host->create_action_button(
    parent, "Reset", center + host->sx(16), host->sy(308), host->sx(96),
    host->sy(38), lv_color_hex(0x6f8790), fish_cb,
    (uintptr_t)&context->bindings[1]);
  if (knock == NULL || reset == NULL)
    {
      wooden_fish_unmount(context);
      return -1;
    }

  context->mounted = true;
  return 0;
}

static bool wooden_fish_tick(void *opaque, uint32_t now_ms)
{
  wooden_fish_context_t *context = opaque;

  if (context == NULL || !context->has_last_tap ||
      context->hint == WOODEN_FISH_HINT_STEADY ||
      now_ms - context->last_tap_ms <= 1200u)
    {
      return false;
    }

  context->hint = WOODEN_FISH_HINT_STEADY;
  context->speed_per_minute = 0;
  return true;
}

const smart_band_app_ops_t smart_band_wooden_fish_app_ops =
{
  .context_size = sizeof(wooden_fish_context_t),
  .init = wooden_fish_init,
  .mount = wooden_fish_mount,
  .unmount = wooden_fish_unmount,
  .tick = wooden_fish_tick,
  .render = wooden_fish_render
};
