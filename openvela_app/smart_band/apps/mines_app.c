#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum
{
  MINE_DIFFICULTY_EASY = 0,
  MINE_DIFFICULTY_MEDIUM,
  MINE_DIFFICULTY_HARD,
  MINE_DIFFICULTY_COUNT
} mine_difficulty_t;

static lv_obj_t *g_status;
static lv_obj_t *g_cells[SMART_BAND_MINE_CELLS];
static lv_obj_t *g_difficulty_buttons[MINE_DIFFICULTY_COUNT];
static uint64_t g_revealed;
static uint64_t g_mines;
static uint32_t g_seed = 0x5a17u;
static int g_difficulty = MINE_DIFFICULTY_MEDIUM;
static int g_mine_count;
static bool g_over;

static uint64_t mine_mask(int index)
{
  return 1ull << index;
}

static const char *mine_difficulty_name(int difficulty)
{
  switch (difficulty)
    {
      case MINE_DIFFICULTY_EASY:
        return "Easy";

      case MINE_DIFFICULTY_HARD:
        return "Hard";

      case MINE_DIFFICULTY_MEDIUM:
      default:
        return "Med";
    }
}

static int mine_difficulty_count(int difficulty)
{
  switch (difficulty)
    {
      case MINE_DIFFICULTY_EASY:
        return 5;

      case MINE_DIFFICULTY_HARD:
        return 11;

      case MINE_DIFFICULTY_MEDIUM:
      default:
        return 8;
    }
}

static uint32_t mine_next_random(void)
{
  g_seed = g_seed * 1664525u + 1013904223u;
  return g_seed;
}

static void mine_place_mines(void)
{
  int placed = 0;

  g_mines = 0;
  while (placed < g_mine_count)
    {
      int index = (int)(mine_next_random() % SMART_BAND_MINE_CELLS);

      if ((g_mines & mine_mask(index)) == 0)
        {
          g_mines |= mine_mask(index);
          placed++;
        }
    }
}

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
              (g_mines & mine_mask(ni)) != 0)
            {
              count++;
            }
        }
    }

  return count;
}

static void mine_reveal_cell(int index)
{
  int row = index / SMART_BAND_MINE_SIZE;
  int col = index % SMART_BAND_MINE_SIZE;

  if (index < 0 || index >= SMART_BAND_MINE_CELLS ||
      (g_revealed & mine_mask(index)) != 0 ||
      (g_mines & mine_mask(index)) != 0)
    {
      return;
    }

  g_revealed |= mine_mask(index);
  if (mine_neighbor_count(index) != 0)
    {
      return;
    }

  for (int dy = -1; dy <= 1; dy++)
    {
      for (int dx = -1; dx <= 1; dx++)
        {
          int nr = row + dy;
          int nc = col + dx;

          if (dx == 0 && dy == 0)
            {
              continue;
            }

          if (nr >= 0 && nr < SMART_BAND_MINE_SIZE &&
              nc >= 0 && nc < SMART_BAND_MINE_SIZE)
            {
              mine_reveal_cell(nr * SMART_BAND_MINE_SIZE + nc);
            }
        }
    }
}

static int mine_revealed_safe_count(void)
{
  int revealed_safe = 0;

  for (int i = 0; i < SMART_BAND_MINE_CELLS; i++)
    {
      if ((g_revealed & mine_mask(i)) != 0 &&
          (g_mines & mine_mask(i)) == 0)
        {
          revealed_safe++;
        }
    }

  return revealed_safe;
}

void smart_band_mines_app_update(const smart_band_app_host_t *host)
{
  int safe_cells = SMART_BAND_MINE_CELLS - g_mine_count;
  int revealed_safe = mine_revealed_safe_count();
  char status[40];

  (void)host;
  for (int i = 0; i < SMART_BAND_MINE_CELLS; i++)
    {
      lv_obj_t *cell = g_cells[i];
      bool is_mine = (g_mines & mine_mask(i)) != 0;
      bool revealed = (g_revealed & mine_mask(i)) != 0;
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

  for (int i = 0; i < MINE_DIFFICULTY_COUNT; i++)
    {
      if (g_difficulty_buttons[i] != NULL)
        {
          lv_obj_set_style_bg_color(g_difficulty_buttons[i],
                                    i == g_difficulty ?
                                    lv_color_hex(0x00796c) :
                                    lv_color_hex(0x8aa8d8), 0);
        }
    }

  if (g_over && revealed_safe < safe_cells)
    {
      snprintf(status, sizeof(status), "Boom  %s %dM",
               mine_difficulty_name(g_difficulty), g_mine_count);
    }
  else if (revealed_safe >= safe_cells)
    {
      snprintf(status, sizeof(status), "Cleared  %s",
               mine_difficulty_name(g_difficulty));
      g_over = true;
    }
  else
    {
      snprintf(status, sizeof(status), "%s  %d left",
               mine_difficulty_name(g_difficulty),
               safe_cells - revealed_safe);
    }

  if (g_status != NULL)
    {
      lv_label_set_text(g_status, status);
    }
}

static void mine_start_game(uint32_t seed_bump)
{
  g_seed = g_seed * 1103515245u + 12345u + seed_bump + lv_tick_get();
  if (g_seed == 0)
    {
      g_seed = 1;
    }

  g_mine_count = mine_difficulty_count(g_difficulty);
  g_revealed = 0;
  g_over = false;
  mine_place_mines();
}

static void mine_cell_cb(lv_event_t *event)
{
  int index = (int)(uintptr_t)lv_event_get_user_data(event);

  if (index < 0 || index >= SMART_BAND_MINE_CELLS || g_over)
    {
      return;
    }

  if ((g_mines & mine_mask(index)) != 0)
    {
      g_revealed |= g_mines;
      g_over = true;
    }
  else
    {
      mine_reveal_cell(index);
    }

  smart_band_mines_app_update(NULL);
}

static void mine_new_cb(lv_event_t *event)
{
  (void)event;

  mine_start_game(17);
  smart_band_mines_app_update(NULL);
}

static void mine_difficulty_cb(lv_event_t *event)
{
  int difficulty = (int)(uintptr_t)lv_event_get_user_data(event);

  if (difficulty >= 0 && difficulty < MINE_DIFFICULTY_COUNT)
    {
      g_difficulty = difficulty;
      mine_start_game(31u + (uint32_t)difficulty * 97u);
      smart_band_mines_app_update(NULL);
    }
}

int smart_band_mines_app_build(lv_obj_t *parent,
                               const smart_band_app_host_t *host)
{
  lv_coord_t gap = host->sx(5);
  lv_coord_t cell = host->sy(37);
  lv_coord_t max_cell = (host->screen_w - host->sx(40) -
                         gap * (SMART_BAND_MINE_SIZE - 1)) /
                        SMART_BAND_MINE_SIZE;
  lv_coord_t grid_w;
  lv_coord_t start_x;
  lv_coord_t start_y = host->sy(42);
  lv_coord_t controls_y;
  lv_coord_t button_gap = host->sx(6);
  lv_coord_t button_w = (host->screen_w - host->sx(28) * 2 -
                         button_gap * 3) / 4;
  static const char *const labels[MINE_DIFFICULTY_COUNT] =
  {
    "Easy", "Med", "Hard"
  };

  if (cell > max_cell)
    {
      cell = max_cell;
    }

  grid_w = cell * SMART_BAND_MINE_SIZE +
           gap * (SMART_BAND_MINE_SIZE - 1);
  start_x = (host->screen_w - grid_w) / 2;
  controls_y = start_y + grid_w + host->sy(14);

  memset(g_cells, 0, sizeof(g_cells));
  memset(g_difficulty_buttons, 0, sizeof(g_difficulty_buttons));
  mine_start_game(7);

  g_status = host->create_label(parent, "Med  28 left",
                                host->font_16(), lv_color_hex(0x293b53),
                                LV_TEXT_ALIGN_CENTER);
  if (g_status == NULL)
    {
      return -1;
    }

  host->place_label(g_status, host->sx(16), host->sy(6),
                    host->screen_w - host->sx(32), host->sy(26));

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

      lv_obj_set_style_radius(g_cells[i], host->sx(9), 0);
      lv_obj_set_style_shadow_width(g_cells[i], host->sx(3), 0);
    }

  for (int i = 0; i < MINE_DIFFICULTY_COUNT; i++)
    {
      g_difficulty_buttons[i] =
        host->create_action_button(parent, labels[i],
                                   host->sx(28) + i * (button_w + button_gap),
                                   controls_y, button_w, host->sy(36),
                                   lv_color_hex(0x8aa8d8),
                                   mine_difficulty_cb, (uintptr_t)i);
      if (g_difficulty_buttons[i] == NULL)
        {
          return -1;
        }
    }

  if (host->create_action_button(parent, "New",
                                 host->sx(28) +
                                 3 * (button_w + button_gap),
                                 controls_y, button_w, host->sy(36),
                                 lv_color_hex(0xf5c66e),
                                 mine_new_cb, 0) == NULL)
    {
      return -1;
    }

  smart_band_mines_app_update(host);
  return 0;
}
