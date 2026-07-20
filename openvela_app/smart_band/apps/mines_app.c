#include "smart_band_apps.h"
#include "mines_model.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
  lv_obj_t *status;
  lv_obj_t *cells[MINES_MODEL_CELLS];
  lv_obj_t *difficulty_buttons[MINES_DIFFICULTY_COUNT];
} mines_view_t;

typedef struct
{
  smart_band_app_event_binding_t cell[MINES_MODEL_CELLS];
  smart_band_app_event_binding_t
    difficulty[MINES_DIFFICULTY_COUNT];
  smart_band_app_event_binding_t new_game;
} mines_bindings_t;

typedef struct
{
  mines_model_t model;
  mines_view_t view;
  mines_bindings_t bindings;
} mines_context_t;

_Static_assert(sizeof(mines_context_t) <= SMART_BAND_APP_CONTEXT_CAPACITY,
               "mines app context exceeds runtime capacity");

static const char *mine_difficulty_name(mines_difficulty_t difficulty)
{
  switch (difficulty)
    {
      case MINES_DIFFICULTY_EASY:
        return "Easy";

      case MINES_DIFFICULTY_HARD:
        return "Hard";

      case MINES_DIFFICULTY_MEDIUM:
      default:
        return "Med";
    }
}

static void mines_render(void *context, const smart_band_app_host_t *host)
{
  mines_context_t *ctx = context;
  int safe_cells;
  int revealed_safe;
  mines_difficulty_t difficulty;
  mines_status_t game_status;
  char status[40];

  (void)host;
  if (ctx == NULL)
    {
      return;
    }

  safe_cells = MINES_MODEL_CELLS - mines_model_mine_count(&ctx->model);
  revealed_safe = mines_model_revealed_safe_count(&ctx->model);
  difficulty = mines_model_difficulty(&ctx->model);
  game_status = mines_model_status(&ctx->model);
  for (int i = 0; i < MINES_MODEL_CELLS; i++)
    {
      lv_obj_t *cell = ctx->view.cells[i];
      bool is_mine = mines_model_is_mine(&ctx->model, i);
      bool revealed = mines_model_is_revealed(&ctx->model, i);
      lv_obj_t *label;
      char text[4];

      if (cell == NULL)
        {
          continue;
        }

      label = lv_obj_get_child(cell, 0);
      if (revealed)
        {
          if (is_mine)
            {
              snprintf(text, sizeof(text), "*");
              lv_obj_set_style_bg_color(cell, lv_color_hex(0xf08d88), 0);
            }
          else
            {
              int count = mines_model_neighbor_count(&ctx->model, i);

              if (count == 0)
                {
                  snprintf(text, sizeof(text), " ");
                }
              else
                {
                  snprintf(text, sizeof(text), "%d", count);
                }

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
          lv_label_set_text(label, text);
          lv_obj_set_style_text_color(label, lv_color_hex(0x293b53), 0);
        }
    }

  for (int i = 0; i < MINES_DIFFICULTY_COUNT; i++)
    {
      if (ctx->view.difficulty_buttons[i] != NULL)
        {
          lv_obj_set_style_bg_color(ctx->view.difficulty_buttons[i],
                                    i == (int)difficulty ?
                                    lv_color_hex(0x00796c) :
                                    lv_color_hex(0x8aa8d8), 0);
        }
    }

  if (game_status == MINES_STATUS_LOST)
    {
      snprintf(status, sizeof(status), "Boom  %s %dM",
               mine_difficulty_name(difficulty),
               mines_model_mine_count(&ctx->model));
    }
  else if (game_status == MINES_STATUS_WON)
    {
      snprintf(status, sizeof(status), "Cleared  %s",
               mine_difficulty_name(difficulty));
    }
  else
    {
      snprintf(status, sizeof(status), "%s  %d left",
               mine_difficulty_name(difficulty),
               safe_cells - revealed_safe);
    }

  if (ctx->view.status != NULL)
    {
      lv_label_set_text(ctx->view.status, status);
    }
}

static void mine_start_game(mines_context_t *ctx,
                            mines_difficulty_t difficulty,
                            uint32_t seed)
{
  (void)mines_model_new_game(&ctx->model, difficulty, seed);
}

static void mine_cell_cb(lv_event_t *event)
{
  smart_band_app_event_binding_t *binding = lv_event_get_user_data(event);
  mines_context_t *ctx;
  int index;

  if (binding == NULL || binding->context == NULL)
    {
      return;
    }

  ctx = binding->context;
  index = (int)binding->action;
  (void)mines_model_reveal(&ctx->model, index);
  mines_render(ctx, NULL);
}

static void mine_new_cb(lv_event_t *event)
{
  smart_band_app_event_binding_t *binding = lv_event_get_user_data(event);

  if (binding != NULL && binding->context != NULL)
    {
      mines_context_t *ctx = binding->context;

      mine_start_game(ctx, mines_model_difficulty(&ctx->model),
                      lv_tick_get() + 17u);
      mines_render(ctx, NULL);
    }
}

static void mine_difficulty_cb(lv_event_t *event)
{
  smart_band_app_event_binding_t *binding = lv_event_get_user_data(event);
  mines_context_t *ctx;
  int difficulty;

  if (binding == NULL || binding->context == NULL)
    {
      return;
    }

  ctx = binding->context;
  difficulty = (int)binding->action;
  if (difficulty >= 0 && difficulty < MINES_DIFFICULTY_COUNT)
    {
      mine_start_game(ctx, (mines_difficulty_t)difficulty,
                      lv_tick_get() + 31u + (uint32_t)difficulty * 97u);
      mines_render(ctx, NULL);
    }
}

static void mines_unmount(void *context)
{
  mines_context_t *ctx = context;

  if (ctx == NULL)
    {
      return;
    }

  ctx->view.status = NULL;
  memset(ctx->view.cells, 0, sizeof(ctx->view.cells));
  memset(ctx->view.difficulty_buttons, 0,
         sizeof(ctx->view.difficulty_buttons));
}

static int mines_mount(void *context, lv_obj_t *parent,
                       const smart_band_app_host_t *host)
{
  mines_context_t *ctx = context;
  lv_coord_t gap = host->sx(5);
  lv_coord_t cell = host->sy(37);
  lv_coord_t max_cell = (host->screen_w - host->sx(40) -
                         gap * (MINES_MODEL_SIZE - 1)) /
                        MINES_MODEL_SIZE;
  lv_coord_t grid_w;
  lv_coord_t start_x;
  lv_coord_t start_y = host->sy(42);
  lv_coord_t controls_y;
  lv_coord_t button_gap = host->sx(6);
  lv_coord_t button_w = (host->screen_w - host->sx(28) * 2 -
                         button_gap * 3) / 4;
  static const char *const labels[MINES_DIFFICULTY_COUNT] =
  {
    "Easy", "Med", "Hard"
  };

  if (ctx == NULL)
    {
      return -1;
    }

  mines_unmount(ctx);
  if (cell > max_cell)
    {
      cell = max_cell;
    }

  grid_w = cell * MINES_MODEL_SIZE +
           gap * (MINES_MODEL_SIZE - 1);
  start_x = (host->screen_w - grid_w) / 2;
  controls_y = start_y + grid_w + host->sy(14);

  mine_start_game(ctx, mines_model_difficulty(&ctx->model),
                  lv_tick_get() + 7u);

  ctx->view.status = host->create_label(parent, "Med  28 left",
                                        host->font_16(),
                                        lv_color_hex(0x293b53),
                                        LV_TEXT_ALIGN_CENTER);
  if (ctx->view.status == NULL)
    {
      return -1;
    }

  host->place_label(ctx->view.status, host->sx(16), host->sy(6),
                    host->screen_w - host->sx(32), host->sy(26));

  for (int i = 0; i < MINES_MODEL_CELLS; i++)
    {
      int row = i / MINES_MODEL_SIZE;
      int col = i % MINES_MODEL_SIZE;

      ctx->bindings.cell[i].context = ctx;
      ctx->bindings.cell[i].action = (uintptr_t)i;
      ctx->view.cells[i] =
        host->create_action_button(parent, " ",
                                   start_x + col * (cell + gap),
                                   start_y + row * (cell + gap),
                                   cell, cell, lv_color_hex(0x80cbc3),
                                   mine_cell_cb,
                                   (uintptr_t)&ctx->bindings.cell[i]);
      if (ctx->view.cells[i] == NULL)
        {
          return -1;
        }

      lv_obj_set_style_radius(ctx->view.cells[i], host->sx(9), 0);
      lv_obj_set_style_shadow_width(ctx->view.cells[i], host->sx(3), 0);
    }

  for (int i = 0; i < MINES_DIFFICULTY_COUNT; i++)
    {
      ctx->bindings.difficulty[i].context = ctx;
      ctx->bindings.difficulty[i].action = (uintptr_t)i;
      ctx->view.difficulty_buttons[i] =
        host->create_action_button(parent, labels[i],
                                   host->sx(28) + i * (button_w + button_gap),
                                   controls_y, button_w, host->sy(36),
                                   lv_color_hex(0x8aa8d8),
                                   mine_difficulty_cb,
                                   (uintptr_t)&ctx->bindings.difficulty[i]);
      if (ctx->view.difficulty_buttons[i] == NULL)
        {
          return -1;
        }
    }

  ctx->bindings.new_game.context = ctx;
  ctx->bindings.new_game.action = 0;
  if (host->create_action_button(parent, "New",
                                 host->sx(28) +
                                 3 * (button_w + button_gap),
                                 controls_y, button_w, host->sy(36),
                                 lv_color_hex(0xf5c66e),
                                 mine_new_cb,
                                 (uintptr_t)&ctx->bindings.new_game) == NULL)
    {
      return -1;
    }

  mines_render(ctx, host);
  return 0;
}

static int mines_init(void *context)
{
  mines_context_t *ctx = context;

  if (ctx == NULL)
    {
      return -1;
    }

  memset(ctx, 0, sizeof(*ctx));
  mines_model_init(&ctx->model, 0x5a17u);
  return 0;
}

const smart_band_app_ops_t smart_band_mines_app_ops =
{
  .context_size = sizeof(mines_context_t),
  .init = mines_init,
  .mount = mines_mount,
  .unmount = mines_unmount,
  .tick = NULL,
  .render = mines_render
};
