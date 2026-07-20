#include "game_2048_model.h"
#include "smart_band_apps.h"

#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
  GAME_ACTION_UP = 0,
  GAME_ACTION_LEFT,
  GAME_ACTION_RIGHT,
  GAME_ACTION_DOWN,
  GAME_ACTION_NEW
} game_action_t;

#define GAME_ACTION_COUNT 5

typedef struct
{
  lv_obj_t *cells[GAME_2048_CELLS];
  lv_obj_t *cell_labels[GAME_2048_CELLS];
  lv_obj_t *score_label;
  lv_obj_t *status_label;
  lv_obj_t *board_obj;
} game_2048_view_t;

typedef struct
{
  game_2048_model_t model;
  game_2048_view_t view;
  bool press_valid;
  lv_point_t press_point;
  smart_band_app_event_binding_t action_bindings[GAME_ACTION_COUNT];
  smart_band_app_event_binding_t swipe_binding;
} game_2048_context_t;

_Static_assert(sizeof(game_2048_context_t) <=
               SMART_BAND_APP_CONTEXT_CAPACITY,
               "2048 app context exceeds runtime capacity");

static uint32_t tile_color(uint32_t value)
{
  switch (value)
    {
      case 2:
        return 0xeef8f6;
      case 4:
        return 0xdaf1ee;
      case 8:
        return 0xb9e5de;
      case 16:
        return 0x95d6cd;
      case 32:
        return 0x72c4bc;
      case 64:
        return 0x55afa7;
      case 128:
        return 0xf3d78c;
      case 256:
        return 0xf0c669;
      case 512:
        return 0xeaa84f;
      case 1024:
        return 0xe58f52;
      case 2048:
        return 0xf08d88;
      default:
        return value == 0 ? 0xf2f7f8 : 0x6f8790;
    }
}

static uint32_t tile_text_color(uint32_t value)
{
  return value <= 4 ? 0x293b53 : 0xffffff;
}

static void game_2048_render(void *context,
                             const smart_band_app_host_t *host)
{
  game_2048_context_t *ctx = context;
  char buffer[24];

  (void)host;
  if (ctx == NULL)
    {
      return;
    }

  for (size_t r = 0; r < GAME_2048_SIZE; r++)
    {
      for (size_t c = 0; c < GAME_2048_SIZE; c++)
        {
          size_t index = r * GAME_2048_SIZE + c;
          uint32_t value = game_2048_model_cell(&ctx->model, r, c);

          if (ctx->view.cells[index] != NULL)
            {
              lv_obj_set_style_bg_color(ctx->view.cells[index],
                                        lv_color_hex(tile_color(value)), 0);
            }

          if (ctx->view.cell_labels[index] != NULL)
            {
              if (value == 0)
                {
                  lv_label_set_text(ctx->view.cell_labels[index], "");
                }
              else
                {
                  snprintf(buffer, sizeof(buffer), "%" PRIu32, value);
                  lv_label_set_text(ctx->view.cell_labels[index], buffer);
                }

              lv_obj_set_style_text_color(ctx->view.cell_labels[index],
                                          lv_color_hex(tile_text_color(value)),
                                          0);
            }
        }
    }

  if (ctx->view.score_label != NULL)
    {
      snprintf(buffer, sizeof(buffer), "%" PRIu64,
               game_2048_model_score(&ctx->model));
      lv_label_set_text(ctx->view.score_label, buffer);
    }

  if (ctx->view.status_label != NULL)
    {
      if (game_2048_model_game_over(&ctx->model))
        {
          lv_label_set_text(ctx->view.status_label, "Game over");
        }
      else if (game_2048_model_won(&ctx->model))
        {
          lv_label_set_text(ctx->view.status_label, "2048 reached");
        }
      else
        {
          lv_label_set_text(ctx->view.status_label, "Swipe or tap arrows");
        }
    }

  if (ctx->view.board_obj != NULL)
    {
      lv_obj_invalidate(ctx->view.board_obj);
    }
}

static void move_cb(lv_event_t *event)
{
  smart_band_app_event_binding_t *binding = lv_event_get_user_data(event);
  game_2048_context_t *ctx;
  game_action_t action;

  if (lv_event_get_code(event) != LV_EVENT_CLICKED || binding == NULL ||
      binding->context == NULL)
    {
      return;
    }

  ctx = binding->context;
  action = (game_action_t)binding->action;
  if (action == GAME_ACTION_NEW)
    {
      game_2048_model_new_game(&ctx->model);
    }
  else if (action >= GAME_ACTION_UP && action <= GAME_ACTION_DOWN)
    {
      game_2048_model_move(&ctx->model, (game_2048_move_t)action);
    }

  game_2048_render(ctx, NULL);
}

static lv_coord_t abs_coord(lv_coord_t value)
{
  return value < 0 ? -value : value;
}

static void board_swipe_cb(lv_event_t *event)
{
  smart_band_app_event_binding_t *binding = lv_event_get_user_data(event);
  game_2048_context_t *ctx;
  lv_event_code_t code = lv_event_get_code(event);
  lv_indev_t *indev = lv_indev_get_act();
  lv_point_t point;
  lv_coord_t dx;
  lv_coord_t dy;
  game_2048_move_t direction;

  if (indev == NULL || binding == NULL || binding->context == NULL)
    {
      return;
    }

  ctx = binding->context;
  if (code == LV_EVENT_PRESSED)
    {
      lv_indev_get_point(indev, &ctx->press_point);
      ctx->press_valid = true;
      return;
    }

  if (code != LV_EVENT_RELEASED || !ctx->press_valid ||
      game_2048_model_game_over(&ctx->model))
    {
      return;
    }

  ctx->press_valid = false;
  lv_indev_get_point(indev, &point);
  dx = point.x - ctx->press_point.x;
  dy = point.y - ctx->press_point.y;

  if (abs_coord(dx) < 24 && abs_coord(dy) < 24)
    {
      return;
    }

  if (abs_coord(dx) > abs_coord(dy))
    {
      direction = dx > 0 ? GAME_2048_MOVE_RIGHT : GAME_2048_MOVE_LEFT;
    }
  else
    {
      direction = dy > 0 ? GAME_2048_MOVE_DOWN : GAME_2048_MOVE_UP;
    }

  game_2048_model_move(&ctx->model, direction);
  game_2048_render(ctx, NULL);
}

static lv_obj_t *create_text(lv_obj_t *parent, const char *text,
                             const smart_band_app_host_t *host,
                             const lv_font_t *font, uint32_t color,
                             lv_text_align_t align,
                             lv_coord_t x, lv_coord_t y,
                             lv_coord_t w, lv_coord_t h)
{
  lv_obj_t *label = lv_label_create(parent);

  (void)host;
  if (label == NULL)
    {
      return NULL;
    }

  lv_obj_remove_style_all(label);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(label, align, 0);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_size(label, w, h);
  return label;
}

static lv_obj_t *create_tile(game_2048_context_t *ctx, lv_obj_t *parent,
                             const smart_band_app_host_t *host,
                             lv_coord_t x, lv_coord_t y,
                             lv_coord_t size, int index)
{
  lv_obj_t *tile = lv_obj_create(parent);
  lv_obj_t *label;

  if (tile == NULL)
    {
      return NULL;
    }

  lv_obj_remove_style_all(tile);
  lv_obj_set_pos(tile, x, y);
  lv_obj_set_size(tile, size, size);
  lv_obj_set_style_bg_color(tile, lv_color_hex(0xf2f7f8), 0);
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(tile, host->sx(8), 0);
  lv_obj_set_style_border_width(tile, 0, 0);

  label = create_text(tile, "", host, host->font_14(), 0x293b53,
                      LV_TEXT_ALIGN_CENTER, 0, (size - host->sy(20)) / 2,
                      size, host->sy(22));
  if (label == NULL)
    {
      return NULL;
    }

  ctx->view.cells[index] = tile;
  ctx->view.cell_labels[index] = label;
  return tile;
}

static void game_2048_unmount(void *context)
{
  game_2048_context_t *ctx = context;

  if (ctx == NULL)
    {
      return;
    }

  memset(&ctx->view, 0, sizeof(ctx->view));
  ctx->press_valid = false;
}

static int game_2048_mount(void *context, lv_obj_t *parent,
                           const smart_band_app_host_t *host)
{
  static const game_action_t actions[GAME_ACTION_COUNT] =
  {
    GAME_ACTION_LEFT, GAME_ACTION_UP, GAME_ACTION_DOWN, GAME_ACTION_RIGHT,
    GAME_ACTION_NEW
  };
  game_2048_context_t *ctx = context;
  lv_coord_t margin = host->sx(18);
  lv_coord_t board_size = host->sx(190);
  lv_coord_t gap = host->sx(6);
  lv_coord_t tile_size = (board_size - gap * 5) / GAME_2048_SIZE;
  lv_coord_t board_x = (host->screen_w - board_size) / 2;
  lv_coord_t board_y = host->sy(62);
  lv_coord_t controls_y = board_y + board_size + host->sy(12);
  lv_coord_t button_w = host->sx(62);
  lv_coord_t button_h = host->sy(42);

  if (ctx == NULL)
    {
      return -1;
    }

  game_2048_unmount(ctx);
  for (int i = 0; i < GAME_ACTION_COUNT; i++)
    {
      ctx->action_bindings[i].context = ctx;
      ctx->action_bindings[i].action = (uintptr_t)actions[i];
    }

  ctx->swipe_binding.context = ctx;
  ctx->swipe_binding.action = 0;

  if (create_text(parent, "2048", host, host->font_20(), 0x293b53,
                  LV_TEXT_ALIGN_LEFT, margin, host->sy(4),
                  host->sx(92), host->sy(30)) == NULL ||
      create_text(parent, "Score", host, host->font_12(), 0x81939a,
                  LV_TEXT_ALIGN_RIGHT, host->screen_w - host->sx(112),
                  host->sy(4), host->sx(44), host->sy(18)) == NULL)
    {
      return -1;
    }

  ctx->view.score_label = create_text(parent, "0", host, host->font_20(),
                                      0x293b53, LV_TEXT_ALIGN_RIGHT,
                                      host->screen_w - host->sx(68),
                                      host->sy(2), host->sx(50),
                                      host->sy(30));
  ctx->view.status_label = create_text(parent, "", host, host->font_12(),
                                       0x6f8790, LV_TEXT_ALIGN_LEFT, margin,
                                       host->sy(34),
                                       host->screen_w - margin * 2,
                                       host->sy(22));
  if (ctx->view.score_label == NULL || ctx->view.status_label == NULL)
    {
      return -1;
    }

  ctx->view.board_obj = lv_obj_create(parent);
  if (ctx->view.board_obj == NULL)
    {
      return -1;
    }

  lv_obj_remove_style_all(ctx->view.board_obj);
  lv_obj_add_flag(ctx->view.board_obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ctx->view.board_obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(ctx->view.board_obj, board_x, board_y);
  lv_obj_set_size(ctx->view.board_obj, board_size, board_size);
  lv_obj_set_style_bg_color(ctx->view.board_obj, lv_color_hex(0xdce9e8), 0);
  lv_obj_set_style_bg_opa(ctx->view.board_obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ctx->view.board_obj, host->sx(14), 0);
  lv_obj_add_event_cb(ctx->view.board_obj, board_swipe_cb, LV_EVENT_PRESSED,
                      &ctx->swipe_binding);
  lv_obj_add_event_cb(ctx->view.board_obj, board_swipe_cb, LV_EVENT_RELEASED,
                      &ctx->swipe_binding);

  for (int r = 0; r < GAME_2048_SIZE; r++)
    {
      for (int c = 0; c < GAME_2048_SIZE; c++)
        {
          int index = r * GAME_2048_SIZE + c;
          if (create_tile(ctx, ctx->view.board_obj, host,
                          gap + c * (tile_size + gap),
                          gap + r * (tile_size + gap), tile_size,
                          index) == NULL)
            {
              return -1;
            }
        }
    }

  if (host->create_action_button(parent, "<", margin, controls_y,
                                 button_w, button_h, lv_color_hex(0x6f8790),
                                 move_cb,
                                 (uintptr_t)&ctx->action_bindings[0]) == NULL ||
      host->create_action_button(parent, "^",
                                 margin + button_w + host->sx(8),
                                 controls_y, button_w, button_h,
                                 lv_color_hex(0x80cbc3), move_cb,
                                 (uintptr_t)&ctx->action_bindings[1]) == NULL ||
      host->create_action_button(parent, "v",
                                 margin + (button_w + host->sx(8)) * 2,
                                 controls_y, button_w, button_h,
                                 lv_color_hex(0x80cbc3), move_cb,
                                 (uintptr_t)&ctx->action_bindings[2]) == NULL ||
      host->create_action_button(parent, ">",
                                 margin + (button_w + host->sx(8)) * 3,
                                 controls_y, button_w, button_h,
                                 lv_color_hex(0x6f8790), move_cb,
                                 (uintptr_t)&ctx->action_bindings[3]) == NULL ||
      host->create_action_button(parent, "New",
                                 host->screen_w - host->sx(90),
                                 host->sy(34), host->sx(72), host->sy(36),
                                 lv_color_hex(0xf08d88), move_cb,
                                 (uintptr_t)&ctx->action_bindings[4]) == NULL)
    {
      return -1;
    }

  game_2048_model_new_game(&ctx->model);
  game_2048_render(ctx, host);
  return 0;
}

static int game_2048_init(void *context)
{
  game_2048_context_t *ctx = context;

  if (ctx == NULL)
    {
      return -1;
    }

  memset(ctx, 0, sizeof(*ctx));
  game_2048_model_init(&ctx->model, 0x20482048u);
  return 0;
}

const smart_band_app_ops_t smart_band_2048_app_ops =
{
  .context_size = sizeof(game_2048_context_t),
  .init = game_2048_init,
  .mount = game_2048_mount,
  .unmount = game_2048_unmount,
  .tick = NULL,
  .render = game_2048_render
};
