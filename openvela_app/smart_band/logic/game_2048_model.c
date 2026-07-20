#include "game_2048_model.h"

#include <limits.h>
#include <string.h>

static bool game_2048_direction_valid(game_2048_move_t direction)
{
  return direction >= GAME_2048_MOVE_UP &&
         direction <= GAME_2048_MOVE_DOWN;
}

static uint32_t game_2048_next_random(game_2048_model_t *model)
{
  model->seed = model->seed * 1103515245u + 12345u;
  return (model->seed >> 16) & 0x7fffu;
}

static void game_2048_add_random_tile(game_2048_model_t *model)
{
  size_t empty[GAME_2048_CELLS];
  size_t count = 0;
  size_t index;

  for (size_t row = 0; row < GAME_2048_SIZE; row++)
    {
      for (size_t column = 0; column < GAME_2048_SIZE; column++)
        {
          if (model->board[row][column] == 0u)
            {
              empty[count++] = row * GAME_2048_SIZE + column;
            }
        }
    }

  if (count == 0u)
    {
      return;
    }

  index = empty[game_2048_next_random(model) % count];
  model->board[index / GAME_2048_SIZE][index % GAME_2048_SIZE] =
    game_2048_next_random(model) % 10u == 0u ? 4u : 2u;
}

static bool game_2048_merge_line(const uint32_t input[GAME_2048_SIZE],
                                 uint32_t output[GAME_2048_SIZE],
                                 uint64_t *score_gain, bool *won)
{
  uint32_t packed[GAME_2048_SIZE] = {0u};
  size_t count = 0;
  size_t output_index = 0;
  bool changed = false;

  for (size_t index = 0; index < GAME_2048_SIZE; index++)
    {
      if (input[index] != 0u)
        {
          packed[count++] = input[index];
        }
    }

  for (size_t index = 0; index < count; index++)
    {
      if (index + 1u < count && packed[index] == packed[index + 1u] &&
          packed[index] <= UINT32_MAX / 2u)
        {
          output[output_index] = packed[index] * 2u;
          *score_gain += output[output_index];
          if (output[output_index] >= 2048u)
            {
              *won = true;
            }

          index++;
        }
      else
        {
          output[output_index] = packed[index];
        }

      output_index++;
    }

  for (size_t index = 0; index < GAME_2048_SIZE; index++)
    {
      if (output[index] != input[index])
        {
          changed = true;
        }
    }

  return changed;
}

static uint32_t game_2048_read_lane(const game_2048_model_t *model,
                                    game_2048_move_t direction,
                                    size_t lane, size_t index)
{
  switch (direction)
    {
      case GAME_2048_MOVE_LEFT:
        return model->board[lane][index];
      case GAME_2048_MOVE_RIGHT:
        return model->board[lane][GAME_2048_SIZE - 1u - index];
      case GAME_2048_MOVE_UP:
        return model->board[index][lane];
      case GAME_2048_MOVE_DOWN:
        return model->board[GAME_2048_SIZE - 1u - index][lane];
      default:
        return 0u;
    }
}

static void game_2048_write_lane(game_2048_model_t *model,
                                 game_2048_move_t direction,
                                 size_t lane, size_t index, uint32_t value)
{
  switch (direction)
    {
      case GAME_2048_MOVE_LEFT:
        model->board[lane][index] = value;
        break;
      case GAME_2048_MOVE_RIGHT:
        model->board[lane][GAME_2048_SIZE - 1u - index] = value;
        break;
      case GAME_2048_MOVE_UP:
        model->board[index][lane] = value;
        break;
      case GAME_2048_MOVE_DOWN:
        model->board[GAME_2048_SIZE - 1u - index][lane] = value;
        break;
      default:
        break;
    }
}

void game_2048_model_init(game_2048_model_t *model, uint32_t seed)
{
  if (model == NULL)
    {
      return;
    }

  memset(model, 0, sizeof(*model));
  model->seed = seed;
}

void game_2048_model_new_game(game_2048_model_t *model)
{
  uint32_t seed;

  if (model == NULL)
    {
      return;
    }

  seed = model->seed;
  memset(model, 0, sizeof(*model));
  model->seed = seed;
  game_2048_add_random_tile(model);
  game_2048_add_random_tile(model);
}

bool game_2048_model_move(game_2048_model_t *model,
                          game_2048_move_t direction)
{
  uint64_t score_gain = 0u;
  bool moved = false;
  bool won;

  if (model == NULL || !game_2048_direction_valid(direction) ||
      model->game_over)
    {
      return false;
    }

  won = model->won;
  for (size_t lane = 0; lane < GAME_2048_SIZE; lane++)
    {
      uint32_t input[GAME_2048_SIZE];
      uint32_t output[GAME_2048_SIZE] = {0u};
      bool lane_changed;

      for (size_t index = 0; index < GAME_2048_SIZE; index++)
        {
          input[index] = game_2048_read_lane(model, direction, lane, index);
        }

      lane_changed = game_2048_merge_line(input, output, &score_gain, &won);
      moved = moved || lane_changed;
      for (size_t index = 0; index < GAME_2048_SIZE; index++)
        {
          game_2048_write_lane(model, direction, lane, index, output[index]);
        }
    }

  if (moved)
    {
      model->score += score_gain;
      model->won = won;
      game_2048_add_random_tile(model);
    }

  model->game_over = !game_2048_model_can_move(model);
  return moved;
}

bool game_2048_model_can_move(const game_2048_model_t *model)
{
  if (model == NULL)
    {
      return false;
    }

  for (size_t row = 0; row < GAME_2048_SIZE; row++)
    {
      for (size_t column = 0; column < GAME_2048_SIZE; column++)
        {
          if (model->board[row][column] == 0u)
            {
              return true;
            }

          if (column + 1u < GAME_2048_SIZE &&
              model->board[row][column] <= UINT32_MAX / 2u &&
              model->board[row][column] == model->board[row][column + 1u])
            {
              return true;
            }

          if (row + 1u < GAME_2048_SIZE &&
              model->board[row][column] <= UINT32_MAX / 2u &&
              model->board[row][column] == model->board[row + 1u][column])
            {
              return true;
            }
        }
    }

  return false;
}

uint32_t game_2048_model_cell(const game_2048_model_t *model,
                              size_t row, size_t column)
{
  if (model == NULL || row >= GAME_2048_SIZE || column >= GAME_2048_SIZE)
    {
      return 0u;
    }

  return model->board[row][column];
}

uint64_t game_2048_model_score(const game_2048_model_t *model)
{
  return model == NULL ? 0u : model->score;
}

bool game_2048_model_won(const game_2048_model_t *model)
{
  return model != NULL && model->won;
}

bool game_2048_model_game_over(const game_2048_model_t *model)
{
  return model != NULL && model->game_over;
}
