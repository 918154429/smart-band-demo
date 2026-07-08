#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static lv_obj_t *g_status;
static lv_obj_t *g_cells[SMART_BAND_MINE_CELLS];
static uint16_t g_revealed;
static uint16_t g_mines;
static bool g_over;

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
              (g_mines & (uint16_t)(1u << ni)) != 0)
            {
              count++;
            }
        }
    }

  return count;
}

void smart_band_mines_app_update(const smart_band_app_host_t *host)
{
  int safe_cells = SMART_BAND_MINE_CELLS - 3;
  int revealed_safe = 0;
  char status[32];

  (void)host;
  for (int i = 0; i < SMART_BAND_MINE_CELLS; i++)
    {
      lv_obj_t *cell = g_cells[i];
      bool is_mine = (g_mines & (uint16_t)(1u << i)) != 0;
      bool revealed = (g_revealed & (uint16_t)(1u << i)) != 0;
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

  if (g_over)
    {
      snprintf(status, sizeof(status), "Boom. New game?");
    }
  else if (revealed_safe >= safe_cells)
    {
      snprintf(status, sizeof(status), "Cleared");
      g_over = true;
    }
  else
    {
      snprintf(status, sizeof(status), "%d safe left",
               safe_cells - revealed_safe);
    }

  if (g_status != NULL)
    {
      lv_label_set_text(g_status, status);
    }
}

static void start_game(void)
{
  g_mines = (uint16_t)((1u << 1) | (1u << 6) | (1u << 11));
  g_revealed = 0;
  g_over = false;
}

static void mine_cell_cb(lv_event_t *event)
{
  int index = (int)(uintptr_t)lv_event_get_user_data(event);

  if (index < 0 || index >= SMART_BAND_MINE_CELLS || g_over)
    {
      return;
    }

  g_revealed |= (uint16_t)(1u << index);
  if ((g_mines & (uint16_t)(1u << index)) != 0)
    {
      g_revealed |= g_mines;
      g_over = true;
    }

  smart_band_mines_app_update(NULL);
}

static void mine_new_cb(lv_event_t *event)
{
  (void)event;

  start_game();
  smart_band_mines_app_update(NULL);
}

int smart_band_mines_app_build(lv_obj_t *parent,
                               const smart_band_app_host_t *host)
{
  lv_coord_t cell = host->sy(48);
  lv_coord_t gap = host->sx(8);
  lv_coord_t grid_w = cell * SMART_BAND_MINE_SIZE +
                      gap * (SMART_BAND_MINE_SIZE - 1);
  lv_coord_t start_x = (host->screen_w - grid_w) / 2;
  lv_coord_t start_y = host->sy(48);

  memset(g_cells, 0, sizeof(g_cells));
  start_game();

  g_status = host->create_label(parent, "Find the safe tiles",
                                host->font_16(), lv_color_hex(0x293b53),
                                LV_TEXT_ALIGN_CENTER);
  if (g_status == NULL)
    {
      return -1;
    }

  host->place_label(g_status, host->sx(18), host->sy(8),
                    host->screen_w - host->sx(36), host->sy(26));

  for (int i = 0; i < SMART_BAND_MINE_CELLS; i++)
    {
      int row = i / SMART_BAND_MINE_SIZE;
      int col = i % SMART_BAND_MINE_SIZE;

      g_cells[i] = host->create_action_button(parent, " ",
                                              start_x +
                                              col * (cell + gap),
                                              start_y +
                                              row * (cell + gap),
                                              cell, cell,
                                              lv_color_hex(0x80cbc3),
                                              mine_cell_cb,
                                              (uintptr_t)i);
      if (g_cells[i] == NULL)
        {
          return -1;
        }
    }

  if (host->create_action_button(parent, "New", host->sx(110),
                                 host->sy(284), host->sx(110),
                                 host->sy(54), lv_color_hex(0x8aa8d8),
                                 mine_new_cb, 0) == NULL)
    {
      return -1;
    }

  smart_band_mines_app_update(host);
  return 0;
}
