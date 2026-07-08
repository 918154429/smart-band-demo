#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static lv_obj_t *g_status;
static lv_obj_t *g_cells[SMART_BAND_TETRIS_ROWS * SMART_BAND_TETRIS_COLS];
static uint8_t g_rows[SMART_BAND_TETRIS_ROWS];
static int g_x;
static int g_y;
static int g_score;
static bool g_running;

static bool tetris_occupied(int x, int y)
{
  if (x < 0 || x >= SMART_BAND_TETRIS_COLS ||
      y < 0 || y >= SMART_BAND_TETRIS_ROWS)
    {
      return true;
    }

  return (g_rows[y] & (uint8_t)(1u << x)) != 0;
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
      if (g_rows[row] == full)
        {
          for (int move = row; move > 0; move--)
            {
              g_rows[move] = g_rows[move - 1];
            }

          g_rows[0] = 0;
          g_score += 10;
          row++;
        }
    }
}

static void tetris_spawn(void)
{
  g_x = 2;
  g_y = 0;
  if (!tetris_can_place(g_x, g_y))
    {
      g_running = false;
      if (g_status != NULL)
        {
          lv_label_set_text(g_status, "Game over");
        }
    }
}

static void tetris_lock_piece(void)
{
  for (int dy = 0; dy < 2; dy++)
    {
      for (int dx = 0; dx < 2; dx++)
        {
          int x = g_x + dx;
          int y = g_y + dy;

          if (x >= 0 && x < SMART_BAND_TETRIS_COLS &&
              y >= 0 && y < SMART_BAND_TETRIS_ROWS)
            {
              g_rows[y] |= (uint8_t)(1u << x);
            }
        }
    }

  tetris_clear_lines();
  tetris_spawn();
}

static void tetris_step(void)
{
  if (!g_running)
    {
      return;
    }

  if (tetris_can_place(g_x, g_y + 1))
    {
      g_y++;
    }
  else
    {
      tetris_lock_piece();
    }
}

static bool tetris_piece_at(int x, int y)
{
  return x >= g_x && x < g_x + 2 && y >= g_y && y < g_y + 2;
}

void smart_band_tetris_app_update(const smart_band_app_host_t *host)
{
  char status[32];

  (void)host;
  for (int row = 0; row < SMART_BAND_TETRIS_ROWS; row++)
    {
      for (int col = 0; col < SMART_BAND_TETRIS_COLS; col++)
        {
          int index = row * SMART_BAND_TETRIS_COLS + col;
          bool filled = (g_rows[row] & (uint8_t)(1u << col)) != 0;
          bool falling = g_running && tetris_piece_at(col, row);
          lv_obj_t *cell = g_cells[index];

          if (cell != NULL)
            {
              lv_obj_set_style_bg_color(cell,
                                        falling ? lv_color_hex(0xf5c66e) :
                                        (filled ? lv_color_hex(0x6f8790) :
                                        lv_color_hex(0xeaf4f2)), 0);
            }
        }
    }

  snprintf(status, sizeof(status), "Score %d%s", g_score,
           g_running ? "" : "  Paused");
  if (g_status != NULL)
    {
      lv_label_set_text(g_status, status);
    }
}

void smart_band_tetris_app_tick(const smart_band_app_host_t *host)
{
  if (host == NULL || host->model == NULL ||
      (host->model->ticks % 3u) != 0)
    {
      return;
    }

  tetris_step();
  smart_band_tetris_app_update(host);
}

static void tetris_new_game(void)
{
  memset(g_rows, 0, sizeof(g_rows));
  g_score = 0;
  g_running = true;
  tetris_spawn();
  smart_band_tetris_app_update(NULL);
}

static void tetris_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);

  if (action == 1)
    {
      if (g_running && tetris_can_place(g_x - 1, g_y))
        {
          g_x--;
        }
    }
  else if (action == 2)
    {
      if (!g_running)
        {
          g_running = true;
        }

      while (tetris_can_place(g_x, g_y + 1))
        {
          g_y++;
        }

      tetris_step();
    }
  else
    {
      tetris_new_game();
      return;
    }

  smart_band_tetris_app_update(NULL);
}

int smart_band_tetris_app_build(lv_obj_t *parent,
                                const smart_band_app_host_t *host)
{
  lv_coord_t cell = host->sy(25);
  lv_coord_t gap = host->sx(4);
  lv_coord_t grid_w = cell * SMART_BAND_TETRIS_COLS +
                      gap * (SMART_BAND_TETRIS_COLS - 1);
  lv_coord_t start_x = (host->screen_w - grid_w) / 2;
  lv_coord_t start_y = host->sy(6);

  memset(g_cells, 0, sizeof(g_cells));
  memset(g_rows, 0, sizeof(g_rows));
  g_score = 0;
  g_running = true;
  g_status = NULL;
  tetris_spawn();

  for (int row = 0; row < SMART_BAND_TETRIS_ROWS; row++)
    {
      for (int col = 0; col < SMART_BAND_TETRIS_COLS; col++)
        {
          int index = row * SMART_BAND_TETRIS_COLS + col;

          g_cells[index] = host->create_box(parent,
                                            start_x + col * (cell + gap),
                                            start_y + row * (cell + gap),
                                            cell, cell,
                                            lv_color_hex(0xeaf4f2),
                                            host->sx(5));
          if (g_cells[index] == NULL)
            {
              return -1;
            }
        }
    }

  g_status = host->create_label(parent, "Score 0", host->font_16(),
                                lv_color_hex(0x293b53),
                                LV_TEXT_ALIGN_CENTER);
  if (g_status == NULL)
    {
      return -1;
    }

  host->place_label(g_status, host->sx(20), host->sy(244),
                    host->screen_w - host->sx(40), host->sy(26));

  if (host->create_action_button(parent, "Left", host->sx(34),
                                 host->sy(288), host->sx(74), host->sy(52),
                                 lv_color_hex(0x6f8790), tetris_cb,
                                 1) == NULL ||
      host->create_action_button(parent, "Drop", host->sx(128),
                                 host->sy(288), host->sx(74), host->sy(52),
                                 lv_color_hex(0x62bfb6), tetris_cb,
                                 2) == NULL ||
      host->create_action_button(parent, "New", host->sx(222),
                                 host->sy(288), host->sx(74), host->sy(52),
                                 lv_color_hex(0x8aa8d8), tetris_cb,
                                 3) == NULL)
    {
      return -1;
    }

  smart_band_tetris_app_update(host);
  return 0;
}
