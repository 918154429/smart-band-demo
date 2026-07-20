#include "smart_band_apps.h"
#include "calculator_model.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
  const char *label;
  char code;
  uint32_t color;
  uint8_t col;
  uint8_t row;
  uint8_t col_span;
  uint8_t row_span;
} calc_key_t;

#define CALC_KEY_COUNT 19

typedef struct
{
  lv_obj_t *display;
  lv_obj_t *expression;
  lv_obj_t *display_box;
} calculator_view_t;

typedef struct
{
  calculator_model_t model;
  calculator_view_t view;
  smart_band_app_event_binding_t key_bindings[CALC_KEY_COUNT];
} calculator_context_t;

static const calc_key_t g_keys[CALC_KEY_COUNT] =
{
  { "C",   'C', 0xee8582, 0, 0, 1, 1 },
  { "DEL", 'B', 0x8298cb, 1, 0, 1, 1 },
  { "/",   '/', 0x72c4bc, 2, 0, 1, 1 },
  { "*",   '*', 0x72c4bc, 3, 0, 1, 1 },
  { "7",   '7', 0x667f89, 0, 1, 1, 1 },
  { "8",   '8', 0x667f89, 1, 1, 1, 1 },
  { "9",   '9', 0x667f89, 2, 1, 1, 1 },
  { "-",   '-', 0x72c4bc, 3, 1, 1, 1 },
  { "4",   '4', 0x667f89, 0, 2, 1, 1 },
  { "5",   '5', 0x667f89, 1, 2, 1, 1 },
  { "6",   '6', 0x667f89, 2, 2, 1, 1 },
  { "+",   '+', 0x72c4bc, 3, 2, 1, 1 },
  { "1",   '1', 0x667f89, 0, 3, 1, 1 },
  { "2",   '2', 0x667f89, 1, 3, 1, 1 },
  { "3",   '3', 0x667f89, 2, 3, 1, 1 },
  { "=",   '=', 0xf0bc62, 3, 3, 1, 2 },
  { "+/-", 'S', 0x667f89, 0, 4, 1, 1 },
  { "0",   '0', 0x667f89, 1, 4, 1, 1 },
  { ".",   '.', 0x667f89, 2, 4, 1, 1 }
};

_Static_assert(sizeof(calculator_context_t) <=
               SMART_BAND_APP_CONTEXT_CAPACITY,
               "calculator app context exceeds runtime capacity");

static lv_coord_t calc_min_coord(lv_coord_t a, lv_coord_t b)
{
  return a < b ? a : b;
}

static lv_coord_t calc_max_coord(lv_coord_t a, lv_coord_t b)
{
  return a > b ? a : b;
}

static lv_coord_t calc_content_height(lv_obj_t *parent,
                                      const smart_band_app_host_t *host)
{
  lv_coord_t parent_h = lv_obj_get_height(parent);
  lv_coord_t visible_h = host->screen_h - lv_obj_get_y(parent) - 4;

  if (parent_h <= 0)
    {
      parent_h = visible_h;
    }

  if (visible_h > 0)
    {
      return calc_min_coord(parent_h, visible_h);
    }

  return parent_h;
}

static void calculator_render(void *context,
                              const smart_band_app_host_t *host)
{
  calculator_context_t *ctx = context;

  (void)host;
  if (ctx == NULL)
    {
      return;
    }

  if (ctx->view.display != NULL)
    {
      lv_label_set_text(ctx->view.display,
                        calculator_model_display(&ctx->model));
      lv_obj_invalidate(ctx->view.display);
    }

  if (ctx->view.expression != NULL)
    {
      lv_label_set_text(ctx->view.expression,
                        calculator_model_expression(&ctx->model));
      lv_obj_invalidate(ctx->view.expression);
    }

  if (ctx->view.display_box != NULL)
    {
      lv_obj_invalidate(ctx->view.display_box);
      lv_obj_move_foreground(ctx->view.display_box);
    }

  if (ctx->view.expression != NULL)
    {
      lv_obj_move_foreground(ctx->view.expression);
    }

  if (ctx->view.display != NULL)
    {
      lv_obj_move_foreground(ctx->view.display);
    }

  lv_refr_now(NULL);
}

static void calc_press(calculator_context_t *ctx, char key)
{
  printf("smart_band: calculator key %c\n", key);
  (void)calculator_model_press(&ctx->model, key);
  calculator_render(ctx, NULL);
}

static void calc_key_cb(lv_event_t *event)
{
  smart_band_app_event_binding_t *binding = lv_event_get_user_data(event);

  if (lv_event_get_code(event) == LV_EVENT_CLICKED && binding != NULL &&
      binding->context != NULL)
    {
      calc_press(binding->context, (char)binding->action);
    }
}

static lv_obj_t *calc_make_key(lv_obj_t *parent,
                               const smart_band_app_host_t *host,
                               const calc_key_t *key,
                               smart_band_app_event_binding_t *binding,
                               lv_coord_t x, lv_coord_t y,
                               lv_coord_t w, lv_coord_t h)
{
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_t *label;

  if (button == NULL || key == NULL || binding == NULL)
    {
      return NULL;
    }

  lv_obj_remove_style_all(button);
  lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, w, h);
  lv_obj_set_style_bg_color(button, lv_color_hex(key->color), 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x243647), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(button, calc_min_coord(host->sx(12), h / 3), 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_shadow_width(button, host->sx(3), 0);
  lv_obj_set_style_shadow_color(button, lv_color_hex(0x314856), 0);
  lv_obj_set_style_shadow_opa(button, LV_OPA_20, 0);
  lv_obj_set_style_shadow_offset_y(button, 2, 0);
  lv_obj_add_event_cb(button, calc_key_cb, LV_EVENT_CLICKED,
                      binding);

  label = lv_label_create(button);
  if (label == NULL)
    {
      return NULL;
    }

  lv_obj_remove_style_all(label);
  lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_text(label, key->label);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(label, host->font_14(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(label, 2, (h - host->sy(20)) / 2);
  lv_obj_set_size(label, w - 4, host->sy(22));
  lv_obj_add_event_cb(label, calc_key_cb, LV_EVENT_CLICKED,
                      binding);
  return button;
}

static void calculator_unmount(void *context)
{
  calculator_context_t *ctx = context;

  if (ctx == NULL)
    {
      return;
    }

  ctx->view.display = NULL;
  ctx->view.expression = NULL;
  ctx->view.display_box = NULL;
}

static int calculator_mount(void *context, lv_obj_t *parent,
                            const smart_band_app_host_t *host)
{
  calculator_context_t *ctx = context;
  lv_coord_t page_h = calc_content_height(parent, host);
  lv_coord_t margin = calc_max_coord(host->sx(10), 8);
  lv_coord_t gap_x = calc_max_coord(host->sx(6), 5);
  lv_coord_t gap_y = calc_max_coord(host->sy(6), 4);
  lv_coord_t display_h = calc_max_coord(host->sy(58), 42);
  lv_coord_t grid_y = display_h + gap_y + 4;
  lv_coord_t usable_h = page_h - grid_y - 4;
  lv_coord_t key_w;
  lv_coord_t key_h;
  lv_obj_t *display_box;

  if (usable_h < 5 * 34 + 4 * gap_y)
    {
      display_h = calc_max_coord(host->sy(44), 34);
      grid_y = display_h + gap_y + 2;
      usable_h = page_h - grid_y - 2;
    }

  key_w = (host->screen_w - margin * 2 - gap_x * 3) / 4;
  key_h = (usable_h - gap_y * 4) / 5;
  if (key_h < 28)
    {
      key_h = 28;
    }

  if (ctx == NULL)
    {
      return -1;
    }

  calculator_unmount(ctx);
  calculator_model_init(&ctx->model);

  display_box = host->create_box(parent, margin, 0,
                                 host->screen_w - margin * 2, display_h,
                                 lv_color_hex(0xf4f8fb), host->sx(14));
  if (display_box == NULL)
    {
      return -1;
    }

  lv_obj_set_style_border_width(display_box, 1, 0);
  lv_obj_set_style_border_color(display_box, lv_color_hex(0xe1edf1), 0);
  ctx->view.display_box = display_box;

  ctx->view.expression = lv_label_create(parent);
  ctx->view.display = lv_label_create(parent);
  if (ctx->view.expression == NULL || ctx->view.display == NULL)
    {
      return -1;
    }

  lv_obj_remove_style_all(ctx->view.expression);
  lv_obj_remove_style_all(ctx->view.display);
  lv_obj_clear_flag(ctx->view.expression, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ctx->view.display, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ctx->view.expression, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(ctx->view.display, LV_OBJ_FLAG_SCROLLABLE);

  lv_label_set_text(ctx->view.expression, "");
  lv_label_set_text(ctx->view.display,
                    calculator_model_display(&ctx->model));
  lv_label_set_long_mode(ctx->view.expression, LV_LABEL_LONG_CLIP);
  lv_label_set_long_mode(ctx->view.display, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(ctx->view.expression, host->font_12(), 0);
  lv_obj_set_style_text_font(ctx->view.display, host->font_20(), 0);
  lv_obj_set_style_text_color(ctx->view.expression, lv_color_hex(0x6f8790), 0);
  lv_obj_set_style_text_color(ctx->view.display, lv_color_hex(0x102a3a), 0);
  lv_obj_set_style_text_align(ctx->view.expression, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_align(ctx->view.display, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(ctx->view.expression, margin + host->sx(8), 3);
  lv_obj_set_size(ctx->view.expression,
                  host->screen_w - margin * 2 - host->sx(16),
                  calc_min_coord(host->sy(17), display_h / 3));
  lv_obj_set_pos(ctx->view.display, margin + host->sx(8), display_h / 3);
  lv_obj_set_size(ctx->view.display,
                  host->screen_w - margin * 2 - host->sx(16),
                  display_h - display_h / 3 - 3);

  for (size_t i = 0; i < CALC_KEY_COUNT; i++)
    {
      const calc_key_t *key = &g_keys[i];
      lv_coord_t x = margin + key->col * (key_w + gap_x);
      lv_coord_t y = grid_y + key->row * (key_h + gap_y);
      lv_coord_t w = key_w * key->col_span + gap_x * (key->col_span - 1);
      lv_coord_t h = key_h * key->row_span + gap_y * (key->row_span - 1);

      ctx->key_bindings[i].context = ctx;
      ctx->key_bindings[i].action = (uintptr_t)key->code;
      if (calc_make_key(parent, host, key, &ctx->key_bindings[i],
                        x, y, w, h) == NULL)
        {
          return -1;
        }
    }

  calculator_render(ctx, host);
  return 0;
}

static int calculator_init(void *context)
{
  calculator_context_t *ctx = context;

  if (ctx == NULL)
    {
      return -1;
    }

  memset(ctx, 0, sizeof(*ctx));
  calculator_model_init(&ctx->model);
  return 0;
}

const smart_band_app_ops_t smart_band_calculator_app_ops =
{
  .context_size = sizeof(calculator_context_t),
  .init = calculator_init,
  .mount = calculator_mount,
  .unmount = calculator_unmount,
  .tick = NULL,
  .render = calculator_render
};
