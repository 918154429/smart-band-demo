#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TETRIS_STEP_MS 3000u
#define TETRIS_ACTION_LEFT 1u
#define TETRIS_ACTION_DROP 2u
#define TETRIS_ACTION_NEW 3u
#define TETRIS_ACTION_COUNT 3

typedef struct
{
  uint8_t rows[SMART_BAND_TETRIS_ROWS];
  int x;
  int y;
  int score;
  uint32_t last_step_ms;
  bool running;
  bool tick_started;

  lv_obj_t *status;
  lv_obj_t *cells[SMART_BAND_TETRIS_ROWS * SMART_BAND_TETRIS_COLS];
  bool mounted;
  smart_band_app_event_binding_t bindings[TETRIS_ACTION_COUNT];
} tetris_context_t;

_Static_assert(sizeof(tetris_context_t) <= SMART_BAND_APP_CONTEXT_CAPACITY,
               "tetris app context exceeds runtime capacity");

static bool tetris_occupied(const tetris_context_t *context, int x, int y)
{
  if (x < 0 || x >= SMART_BAND_TETRIS_COLS ||
      y < 0 || y >= SMART_BAND_TETRIS_ROWS)
    {
      return true;
    }

  return (context->rows[y] & (uint8_t)(1u << x)) != 0;
}

static bool tetris_can_place(const tetris_context_t *context, int x, int y)
{
  return !tetris_occupied(context, x, y) &&
         !tetris_occupied(context, x + 1, y) &&
         !tetris_occupied(context, x, y + 1) &&
         !tetris_occupied(context, x + 1, y + 1);
}

static void tetris_clear_lines(tetris_context_t *context)
{
  uint8_t full = (uint8_t)((1u << SMART_BAND_TETRIS_COLS) - 1u);

  for (int row = SMART_BAND_TETRIS_ROWS - 1; row >= 0; row--)
    {
      if (context->rows[row] == full)
        {
          for (int move = row; move > 0; move--)
            {
              context->rows[move] = context->rows[move - 1];
            }

          context->rows[0] = 0;
          context->score += 10;
          row++;
        }
    }
}

static void tetris_spawn(tetris_context_t *context)
{
  context->x = 2;
  context->y = 0;
  if (!tetris_can_place(context, context->x, context->y))
    {
      context->running = false;
    }
}

static void tetris_lock_piece(tetris_context_t *context)
{
  for (int dy = 0; dy < 2; dy++)
    {
      for (int dx = 0; dx < 2; dx++)
        {
          int x = context->x + dx;
          int y = context->y + dy;

          if (x >= 0 && x < SMART_BAND_TETRIS_COLS &&
              y >= 0 && y < SMART_BAND_TETRIS_ROWS)
            {
              context->rows[y] |= (uint8_t)(1u << x);
            }
        }
    }

  tetris_clear_lines(context);
  tetris_spawn(context);
}

static void tetris_step(tetris_context_t *context)
{
  if (!context->running)
    {
      return;
    }

  if (tetris_can_place(context, context->x, context->y + 1))
    {
      context->y++;
    }
  else
    {
      tetris_lock_piece(context);
    }
}

static bool tetris_piece_at(const tetris_context_t *context, int x, int y)
{
  return x >= context->x && x < context->x + 2 &&
         y >= context->y && y < context->y + 2;
}

static void tetris_new_game(tetris_context_t *context)
{
  memset(context->rows, 0, sizeof(context->rows));
  context->score = 0;
  context->running = true;
  context->tick_started = false;
  tetris_spawn(context);
}

static void tetris_render(void *opaque,
                          const smart_band_app_host_t *host)
{
  tetris_context_t *context = opaque;
  char status[32];

  (void)host;
  if (context == NULL || !context->mounted)
    {
      return;
    }

  for (int row = 0; row < SMART_BAND_TETRIS_ROWS; row++)
    {
      for (int col = 0; col < SMART_BAND_TETRIS_COLS; col++)
        {
          int index = row * SMART_BAND_TETRIS_COLS + col;
          bool filled =
            (context->rows[row] & (uint8_t)(1u << col)) != 0;
          bool falling = context->running &&
                         tetris_piece_at(context, col, row);
          lv_obj_t *cell = context->cells[index];

          if (cell != NULL)
            {
              lv_obj_set_style_bg_color(cell,
                                        falling ? lv_color_hex(0xf5c66e) :
                                        (filled ? lv_color_hex(0x6f8790) :
                                        lv_color_hex(0xeaf4f2)), 0);
            }
        }
    }

  snprintf(status, sizeof(status), "Score %d%s", context->score,
           context->running ? "" : "  Paused");
  if (context->status != NULL)
    {
      lv_label_set_text(context->status, status);
    }
}

static void tetris_cb(lv_event_t *event)
{
  smart_band_app_event_binding_t *binding =
    (smart_band_app_event_binding_t *)lv_event_get_user_data(event);
  tetris_context_t *context;

  if (binding == NULL || binding->context == NULL)
    {
      return;
    }

  context = binding->context;
  if (binding->action == TETRIS_ACTION_LEFT)
    {
      if (context->running &&
          tetris_can_place(context, context->x - 1, context->y))
        {
          context->x--;
        }
    }
  else if (binding->action == TETRIS_ACTION_DROP)
    {
      if (!context->running)
        {
          context->running = true;
        }

      while (tetris_can_place(context, context->x, context->y + 1))
        {
          context->y++;
        }

      tetris_step(context);
    }
  else if (binding->action == TETRIS_ACTION_NEW)
    {
      tetris_new_game(context);
    }
  else
    {
      return;
    }

  tetris_render(context, NULL);
}

static int tetris_init(void *opaque)
{
  tetris_context_t *context = opaque;

  if (context == NULL)
    {
      return -1;
    }

  memset(context, 0, sizeof(*context));
  tetris_new_game(context);
  return 0;
}

static void tetris_unmount(void *opaque)
{
  tetris_context_t *context = opaque;

  if (context == NULL)
    {
      return;
    }

  context->status = NULL;
  memset(context->cells, 0, sizeof(context->cells));
  memset(context->bindings, 0, sizeof(context->bindings));
  context->mounted = false;
  context->tick_started = false;
}

static int tetris_mount(void *opaque, lv_obj_t *parent,
                        const smart_band_app_host_t *host)
{
  tetris_context_t *context = opaque;
  lv_coord_t cell;
  lv_coord_t gap;
  lv_coord_t grid_w;
  lv_coord_t start_x;
  lv_coord_t start_y;

  if (context == NULL || parent == NULL || host == NULL)
    {
      return -1;
    }

  tetris_unmount(context);
  cell = host->sy(25);
  gap = host->sx(4);
  grid_w = cell * SMART_BAND_TETRIS_COLS +
           gap * (SMART_BAND_TETRIS_COLS - 1);
  start_x = (host->screen_w - grid_w) / 2;
  start_y = host->sy(6);

  for (int row = 0; row < SMART_BAND_TETRIS_ROWS; row++)
    {
      for (int col = 0; col < SMART_BAND_TETRIS_COLS; col++)
        {
          int index = row * SMART_BAND_TETRIS_COLS + col;

          context->cells[index] =
            host->create_box(parent, start_x + col * (cell + gap),
                             start_y + row * (cell + gap), cell, cell,
                             lv_color_hex(0xeaf4f2), host->sx(5));
          if (context->cells[index] == NULL)
            {
              tetris_unmount(context);
              return -1;
            }
        }
    }

  context->status = host->create_label(parent, "Score 0", host->font_16(),
                                       lv_color_hex(0x293b53),
                                       LV_TEXT_ALIGN_CENTER);
  if (context->status == NULL)
    {
      tetris_unmount(context);
      return -1;
    }

  host->place_label(context->status, host->sx(20), host->sy(244),
                    host->screen_w - host->sx(40), host->sy(26));

  context->bindings[0].context = context;
  context->bindings[0].action = TETRIS_ACTION_LEFT;
  context->bindings[1].context = context;
  context->bindings[1].action = TETRIS_ACTION_DROP;
  context->bindings[2].context = context;
  context->bindings[2].action = TETRIS_ACTION_NEW;

  if (host->create_action_button(
        parent, "Left", host->sx(34), host->sy(288), host->sx(74),
        host->sy(52), lv_color_hex(0x6f8790), tetris_cb,
        (uintptr_t)&context->bindings[0]) == NULL ||
      host->create_action_button(
        parent, "Drop", host->sx(128), host->sy(288), host->sx(74),
        host->sy(52), lv_color_hex(0x62bfb6), tetris_cb,
        (uintptr_t)&context->bindings[1]) == NULL ||
      host->create_action_button(
        parent, "New", host->sx(222), host->sy(288), host->sx(74),
        host->sy(52), lv_color_hex(0x8aa8d8), tetris_cb,
        (uintptr_t)&context->bindings[2]) == NULL)
    {
      tetris_unmount(context);
      return -1;
    }

  context->mounted = true;
  return 0;
}

static bool tetris_tick(void *opaque, uint32_t now_ms)
{
  tetris_context_t *context = opaque;
  bool changed = false;

  if (context == NULL || !context->running)
    {
      return false;
    }

  if (!context->tick_started)
    {
      context->last_step_ms = now_ms;
      context->tick_started = true;
      return false;
    }

  while (context->running &&
         now_ms - context->last_step_ms >= TETRIS_STEP_MS)
    {
      context->last_step_ms += TETRIS_STEP_MS;
      tetris_step(context);
      changed = true;
    }

  return changed;
}

static void tetris_set_visible(void *opaque, bool visible, uint32_t now_ms)
{
  tetris_context_t *context = opaque;

  if (context == NULL)
    {
      return;
    }

  context->last_step_ms = now_ms;
  context->tick_started = visible;
}

const smart_band_app_ops_t smart_band_tetris_app_ops =
{
  .context_size = sizeof(tetris_context_t),
  .init = tetris_init,
  .mount = tetris_mount,
  .unmount = tetris_unmount,
  .tick = tetris_tick,
  .set_visible = tetris_set_visible,
  .render = tetris_render
};
