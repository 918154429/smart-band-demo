#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define GAME_SIZE 4
#define GAME_CELLS (GAME_SIZE * GAME_SIZE)

typedef enum
{
  MOVE_UP = 0,
  MOVE_LEFT,
  MOVE_RIGHT,
  MOVE_DOWN,
  MOVE_NEW
} move_action_t;

static lv_obj_t *g_cells[GAME_CELLS];
static lv_obj_t *g_cell_labels[GAME_CELLS];
static lv_obj_t *g_score_label;
static lv_obj_t *g_status_label;
static lv_obj_t *g_board_obj;
static uint16_t g_board[GAME_SIZE][GAME_SIZE];
static int g_score;
static uint32_t g_seed = 0x20482048;
static bool g_won;
static bool g_game_over;
static bool g_press_valid;
static lv_point_t g_press_point;

static uint32_t game_rand(void)
{
  g_seed = g_seed * 1103515245u + 12345u;
  return (g_seed >> 16) & 0x7fffu;
}

static uint32_t tile_color(uint16_t value)
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

static uint32_t tile_text_color(uint16_t value)
{
  return value <= 4 ? 0x293b53 : 0xffffff;
}

static void add_random_tile(void)
{
  int empty[GAME_CELLS];
  int count = 0;
  int pick;
  int index;

  for (int r = 0; r < GAME_SIZE; r++)
    {
      for (int c = 0; c < GAME_SIZE; c++)
        {
          if (g_board[r][c] == 0)
            {
              empty[count++] = r * GAME_SIZE + c;
            }
        }
    }

  if (count == 0)
    {
      return;
    }

  pick = (int)(game_rand() % (uint32_t)count);
  index = empty[pick];
  g_board[index / GAME_SIZE][index % GAME_SIZE] =
    (game_rand() % 10u) == 0u ? 4 : 2;
}

static bool can_move_board(void)
{
  for (int r = 0; r < GAME_SIZE; r++)
    {
      for (int c = 0; c < GAME_SIZE; c++)
        {
          if (g_board[r][c] == 0)
            {
              return true;
            }

          if (c + 1 < GAME_SIZE && g_board[r][c] == g_board[r][c + 1])
            {
              return true;
            }

          if (r + 1 < GAME_SIZE && g_board[r][c] == g_board[r + 1][c])
            {
              return true;
            }
        }
    }

  return false;
}

static bool merge_line(const uint16_t in[GAME_SIZE], uint16_t out[GAME_SIZE],
                       int *score_gain)
{
  uint16_t packed[GAME_SIZE] = {0};
  int count = 0;
  int out_index = 0;
  bool changed = false;

  for (int i = 0; i < GAME_SIZE; i++)
    {
      if (in[i] != 0)
        {
          packed[count++] = in[i];
        }
    }

  for (int i = 0; i < count; i++)
    {
      if (i + 1 < count && packed[i] == packed[i + 1])
        {
          out[out_index] = packed[i] * 2;
          *score_gain += out[out_index];
          if (out[out_index] >= 2048)
            {
              g_won = true;
            }

          i++;
        }
      else
        {
          out[out_index] = packed[i];
        }

      out_index++;
    }

  while (out_index < GAME_SIZE)
    {
      out[out_index++] = 0;
    }

  for (int i = 0; i < GAME_SIZE; i++)
    {
      if (out[i] != in[i])
        {
          changed = true;
        }
    }

  return changed;
}

static bool move_board(move_action_t action)
{
  bool moved = false;
  int score_gain = 0;

  for (int lane = 0; lane < GAME_SIZE; lane++)
    {
      uint16_t in[GAME_SIZE] = {0};
      uint16_t out[GAME_SIZE] = {0};
      bool lane_changed;

      for (int i = 0; i < GAME_SIZE; i++)
        {
          switch (action)
            {
              case MOVE_LEFT:
                in[i] = g_board[lane][i];
                break;
              case MOVE_RIGHT:
                in[i] = g_board[lane][GAME_SIZE - 1 - i];
                break;
              case MOVE_UP:
                in[i] = g_board[i][lane];
                break;
              case MOVE_DOWN:
                in[i] = g_board[GAME_SIZE - 1 - i][lane];
                break;
              default:
                break;
            }
        }

      lane_changed = merge_line(in, out, &score_gain);
      moved = moved || lane_changed;

      for (int i = 0; i < GAME_SIZE; i++)
        {
          switch (action)
            {
              case MOVE_LEFT:
                g_board[lane][i] = out[i];
                break;
              case MOVE_RIGHT:
                g_board[lane][GAME_SIZE - 1 - i] = out[i];
                break;
              case MOVE_UP:
                g_board[i][lane] = out[i];
                break;
              case MOVE_DOWN:
                g_board[GAME_SIZE - 1 - i][lane] = out[i];
                break;
              default:
                break;
            }
        }
    }

  if (moved)
    {
      g_score += score_gain;
      add_random_tile();
      g_game_over = !can_move_board();
    }

  return moved;
}

static void new_game(void)
{
  for (int r = 0; r < GAME_SIZE; r++)
    {
      for (int c = 0; c < GAME_SIZE; c++)
        {
          g_board[r][c] = 0;
        }
    }

  g_score = 0;
  g_won = false;
  g_game_over = false;
  add_random_tile();
  add_random_tile();
}

void smart_band_music_app_update(const smart_band_app_host_t *host)
{
  char buffer[24];

  (void)host;
  for (int r = 0; r < GAME_SIZE; r++)
    {
      for (int c = 0; c < GAME_SIZE; c++)
        {
          int index = r * GAME_SIZE + c;
          uint16_t value = g_board[r][c];

          if (g_cells[index] != NULL)
            {
              lv_obj_set_style_bg_color(g_cells[index],
                                        lv_color_hex(tile_color(value)), 0);
            }

          if (g_cell_labels[index] != NULL)
            {
              if (value == 0)
                {
                  lv_label_set_text(g_cell_labels[index], "");
                }
              else
                {
                  snprintf(buffer, sizeof(buffer), "%u", value);
                  lv_label_set_text(g_cell_labels[index], buffer);
                }

              lv_obj_set_style_text_color(g_cell_labels[index],
                                          lv_color_hex(tile_text_color(value)),
                                          0);
            }
        }
    }

  if (g_score_label != NULL)
    {
      snprintf(buffer, sizeof(buffer), "%d", g_score);
      lv_label_set_text(g_score_label, buffer);
    }

  if (g_status_label != NULL)
    {
      if (g_game_over)
        {
          lv_label_set_text(g_status_label, "Game over");
        }
      else if (g_won)
        {
          lv_label_set_text(g_status_label, "2048 reached");
        }
      else
        {
          lv_label_set_text(g_status_label, "Swipe or tap arrows");
        }
    }

  if (g_board_obj != NULL)
    {
      lv_obj_invalidate(g_board_obj);
    }
}

static void move_cb(lv_event_t *event)
{
  move_action_t action = (move_action_t)(uintptr_t)lv_event_get_user_data(event);

  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    {
      return;
    }

  if (action == MOVE_NEW)
    {
      new_game();
    }
  else if (!g_game_over)
    {
      move_board(action);
    }

  smart_band_music_app_update(NULL);
}

static lv_coord_t abs_coord(lv_coord_t value)
{
  return value < 0 ? -value : value;
}

static void board_swipe_cb(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);
  lv_indev_t *indev = lv_indev_get_act();
  lv_point_t point;
  lv_coord_t dx;
  lv_coord_t dy;
  move_action_t action;

  if (indev == NULL)
    {
      return;
    }

  if (code == LV_EVENT_PRESSED)
    {
      lv_indev_get_point(indev, &g_press_point);
      g_press_valid = true;
      return;
    }

  if (code != LV_EVENT_RELEASED || !g_press_valid || g_game_over)
    {
      return;
    }

  g_press_valid = false;
  lv_indev_get_point(indev, &point);
  dx = point.x - g_press_point.x;
  dy = point.y - g_press_point.y;

  if (abs_coord(dx) < 24 && abs_coord(dy) < 24)
    {
      return;
    }

  if (abs_coord(dx) > abs_coord(dy))
    {
      action = dx > 0 ? MOVE_RIGHT : MOVE_LEFT;
    }
  else
    {
      action = dy > 0 ? MOVE_DOWN : MOVE_UP;
    }

  move_board(action);
  smart_band_music_app_update(NULL);
}

static lv_obj_t *create_text(lv_obj_t *parent, const char *text,
                             const smart_band_app_host_t *host,
                             const lv_font_t *font, uint32_t color,
                             lv_text_align_t align,
                             lv_coord_t x, lv_coord_t y,
                             lv_coord_t w, lv_coord_t h)
{
  lv_obj_t *label = lv_label_create(parent);

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

static lv_obj_t *create_tile(lv_obj_t *parent,
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

  g_cells[index] = tile;
  g_cell_labels[index] = label;
  return tile;
}

int smart_band_music_app_build(lv_obj_t *parent,
                               const smart_band_app_host_t *host)
{
  lv_coord_t margin = host->sx(18);
  lv_coord_t board_size = host->sx(190);
  lv_coord_t gap = host->sx(6);
  lv_coord_t tile_size = (board_size - gap * 5) / GAME_SIZE;
  lv_coord_t board_x = (host->screen_w - board_size) / 2;
  lv_coord_t board_y = host->sy(62);
  lv_coord_t controls_y = board_y + board_size + host->sy(12);
  lv_coord_t button_w = host->sx(62);
  lv_coord_t button_h = host->sy(42);

  for (int i = 0; i < GAME_CELLS; i++)
    {
      g_cells[i] = NULL;
      g_cell_labels[i] = NULL;
    }

  g_score_label = NULL;
  g_status_label = NULL;
  g_board_obj = NULL;
  g_press_valid = false;

  if (create_text(parent, "2048", host, host->font_20(), 0x293b53,
                  LV_TEXT_ALIGN_LEFT, margin, host->sy(4),
                  host->sx(92), host->sy(30)) == NULL ||
      create_text(parent, "Score", host, host->font_12(), 0x81939a,
                  LV_TEXT_ALIGN_RIGHT, host->screen_w - host->sx(112),
                  host->sy(4), host->sx(44), host->sy(18)) == NULL)
    {
      return -1;
    }

  g_score_label = create_text(parent, "0", host, host->font_20(), 0x293b53,
                              LV_TEXT_ALIGN_RIGHT,
                              host->screen_w - host->sx(68), host->sy(2),
                              host->sx(50), host->sy(30));
  g_status_label = create_text(parent, "", host, host->font_12(), 0x6f8790,
                               LV_TEXT_ALIGN_LEFT, margin, host->sy(34),
                               host->screen_w - margin * 2, host->sy(22));
  if (g_score_label == NULL || g_status_label == NULL)
    {
      return -1;
    }

  g_board_obj = lv_obj_create(parent);
  if (g_board_obj == NULL)
    {
      return -1;
    }

  lv_obj_remove_style_all(g_board_obj);
  lv_obj_add_flag(g_board_obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(g_board_obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(g_board_obj, board_x, board_y);
  lv_obj_set_size(g_board_obj, board_size, board_size);
  lv_obj_set_style_bg_color(g_board_obj, lv_color_hex(0xdce9e8), 0);
  lv_obj_set_style_bg_opa(g_board_obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_board_obj, host->sx(14), 0);
  lv_obj_add_event_cb(g_board_obj, board_swipe_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(g_board_obj, board_swipe_cb, LV_EVENT_RELEASED, NULL);

  for (int r = 0; r < GAME_SIZE; r++)
    {
      for (int c = 0; c < GAME_SIZE; c++)
        {
          int index = r * GAME_SIZE + c;
          if (create_tile(g_board_obj, host, gap + c * (tile_size + gap),
                          gap + r * (tile_size + gap), tile_size,
                          index) == NULL)
            {
              return -1;
            }
        }
    }

  if (host->create_action_button(parent, "<", margin, controls_y,
                                 button_w, button_h, lv_color_hex(0x6f8790),
                                 move_cb, MOVE_LEFT) == NULL ||
      host->create_action_button(parent, "^",
                                 margin + button_w + host->sx(8),
                                 controls_y, button_w, button_h,
                                 lv_color_hex(0x80cbc3), move_cb,
                                 MOVE_UP) == NULL ||
      host->create_action_button(parent, "v",
                                 margin + (button_w + host->sx(8)) * 2,
                                 controls_y, button_w, button_h,
                                 lv_color_hex(0x80cbc3), move_cb,
                                 MOVE_DOWN) == NULL ||
      host->create_action_button(parent, ">",
                                 margin + (button_w + host->sx(8)) * 3,
                                 controls_y, button_w, button_h,
                                 lv_color_hex(0x6f8790), move_cb,
                                 MOVE_RIGHT) == NULL ||
      host->create_action_button(parent, "New",
                                 host->screen_w - host->sx(90),
                                 host->sy(34), host->sx(72), host->sy(36),
                                 lv_color_hex(0xf08d88), move_cb,
                                 MOVE_NEW) == NULL)
    {
      return -1;
    }

  new_game();
  smart_band_music_app_update(host);
  return 0;
}
