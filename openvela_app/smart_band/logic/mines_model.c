#include "mines_model.h"

#include <stddef.h>
#include <string.h>

static bool mines_model_valid_index(int index)
{
  return index >= 0 && index < MINES_MODEL_CELLS;
}

static uint64_t mines_model_mask(int index)
{
  return 1ull << (unsigned int)index;
}

static int mines_model_difficulty_mines(mines_difficulty_t difficulty)
{
  switch (difficulty)
    {
      case MINES_DIFFICULTY_EASY:
        return 5;

      case MINES_DIFFICULTY_HARD:
        return 11;

      case MINES_DIFFICULTY_MEDIUM:
        return 8;

      default:
        return 0;
    }
}

static uint32_t mines_model_next_random(mines_model_t *model)
{
  model->seed = model->seed * 1664525u + 1013904223u;
  return model->seed;
}

static void mines_model_place_mines(mines_model_t *model)
{
  int placed = 0;

  while (placed < model->mine_count)
    {
      int index = (int)(mines_model_next_random(model) % MINES_MODEL_CELLS);
      uint64_t mask = mines_model_mask(index);

      if ((model->mines & mask) == 0)
        {
          model->mines |= mask;
          placed++;
        }
    }
}

static void mines_model_reveal_safe(mines_model_t *model, int index)
{
  int row;
  int col;
  uint64_t mask;

  if (!mines_model_valid_index(index))
    {
      return;
    }

  mask = mines_model_mask(index);
  if ((model->revealed & mask) != 0 || (model->mines & mask) != 0)
    {
      return;
    }

  model->revealed |= mask;
  if (mines_model_neighbor_count(model, index) != 0)
    {
      return;
    }

  row = index / MINES_MODEL_SIZE;
  col = index % MINES_MODEL_SIZE;
  for (int dy = -1; dy <= 1; dy++)
    {
      for (int dx = -1; dx <= 1; dx++)
        {
          int neighbor_row = row + dy;
          int neighbor_col = col + dx;

          if ((dx != 0 || dy != 0) &&
              neighbor_row >= 0 && neighbor_row < MINES_MODEL_SIZE &&
              neighbor_col >= 0 && neighbor_col < MINES_MODEL_SIZE)
            {
              mines_model_reveal_safe(model,
                                      neighbor_row * MINES_MODEL_SIZE +
                                      neighbor_col);
            }
        }
    }
}

void mines_model_init(mines_model_t *model, uint32_t seed)
{
  if (model == NULL)
    {
      return;
    }

  memset(model, 0, sizeof(*model));
  (void)mines_model_new_game(model, MINES_DIFFICULTY_MEDIUM, seed);
}

bool mines_model_new_game(mines_model_t *model,
                          mines_difficulty_t difficulty,
                          uint32_t seed)
{
  int mine_count = mines_model_difficulty_mines(difficulty);

  if (model == NULL || mine_count == 0)
    {
      return false;
    }

  model->revealed = 0;
  model->mines = 0;
  model->seed = seed;
  model->difficulty = difficulty;
  model->mine_count = mine_count;
  model->over = false;
  mines_model_place_mines(model);
  return true;
}

mines_reveal_result_t mines_model_reveal(mines_model_t *model, int index)
{
  uint64_t mask;

  if (model == NULL || !mines_model_valid_index(index))
    {
      return MINES_REVEAL_INVALID;
    }

  mask = mines_model_mask(index);
  if (model->over || (model->revealed & mask) != 0)
    {
      return MINES_REVEAL_IGNORED;
    }

  if ((model->mines & mask) != 0)
    {
      model->revealed |= model->mines;
      model->over = true;
      return MINES_REVEAL_MINE;
    }

  mines_model_reveal_safe(model, index);
  if (mines_model_revealed_safe_count(model) ==
      MINES_MODEL_CELLS - model->mine_count)
    {
      model->over = true;
      return MINES_REVEAL_WIN;
    }

  return MINES_REVEAL_SAFE;
}

int mines_model_neighbor_count(const mines_model_t *model, int index)
{
  int row;
  int col;
  int count = 0;

  if (model == NULL || !mines_model_valid_index(index))
    {
      return -1;
    }

  row = index / MINES_MODEL_SIZE;
  col = index % MINES_MODEL_SIZE;
  for (int dy = -1; dy <= 1; dy++)
    {
      for (int dx = -1; dx <= 1; dx++)
        {
          int neighbor_row = row + dy;
          int neighbor_col = col + dx;

          if ((dx != 0 || dy != 0) &&
              neighbor_row >= 0 && neighbor_row < MINES_MODEL_SIZE &&
              neighbor_col >= 0 && neighbor_col < MINES_MODEL_SIZE &&
              mines_model_is_mine(model,
                                  neighbor_row * MINES_MODEL_SIZE +
                                  neighbor_col))
            {
              count++;
            }
        }
    }

  return count;
}

mines_status_t mines_model_status(const mines_model_t *model)
{
  if (model == NULL || !model->over)
    {
      return MINES_STATUS_PLAYING;
    }

  if ((model->revealed & model->mines) != 0)
    {
      return MINES_STATUS_LOST;
    }

  return MINES_STATUS_WON;
}

bool mines_model_is_revealed(const mines_model_t *model, int index)
{
  return model != NULL && mines_model_valid_index(index) &&
         (model->revealed & mines_model_mask(index)) != 0;
}

bool mines_model_is_mine(const mines_model_t *model, int index)
{
  return model != NULL && mines_model_valid_index(index) &&
         (model->mines & mines_model_mask(index)) != 0;
}

mines_difficulty_t mines_model_difficulty(const mines_model_t *model)
{
  return model == NULL ? MINES_DIFFICULTY_MEDIUM : model->difficulty;
}

int mines_model_mine_count(const mines_model_t *model)
{
  return model == NULL ? 0 : model->mine_count;
}

int mines_model_revealed_safe_count(const mines_model_t *model)
{
  int revealed_safe = 0;

  if (model == NULL)
    {
      return 0;
    }

  for (int i = 0; i < MINES_MODEL_CELLS; i++)
    {
      uint64_t mask = mines_model_mask(i);

      if ((model->revealed & mask) != 0 && (model->mines & mask) == 0)
        {
          revealed_safe++;
        }
    }

  return revealed_safe;
}

uint32_t mines_model_seed(const mines_model_t *model)
{
  return model == NULL ? 0 : model->seed;
}
