#include "calculator_model.h"
#include "game_2048_model.h"
#include "mines_model.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
  do                                                                         \
    {                                                                        \
      if (!(condition))                                                      \
        {                                                                    \
          fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
                  #condition);                                               \
          return 1;                                                          \
        }                                                                    \
    }                                                                        \
  while (0)

static int test_calculator_model(void)
{
  calculator_model_t model;
  size_t length;
  int index;

  calculator_model_init(&model);
  CHECK(strcmp(calculator_model_display(&model), "0") == 0);
  CHECK(calculator_model_press(&model, '1'));
  CHECK(calculator_model_press(&model, '2'));
  CHECK(calculator_model_press(&model, '.'));
  CHECK(calculator_model_press(&model, '3'));
  CHECK(!calculator_model_press(&model, '.'));
  CHECK(calculator_model_press(&model, '+'));
  CHECK(strcmp(calculator_model_expression(&model), "12.3 +") == 0);
  CHECK(calculator_model_press(&model, '4'));
  CHECK(calculator_model_press(&model, '='));
  CHECK(strcmp(calculator_model_display(&model), "16.3") == 0);

  CHECK(calculator_model_press(&model, 'C'));
  CHECK(calculator_model_press(&model, '9'));
  CHECK(calculator_model_press(&model, '/'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '='));
  CHECK(strcmp(calculator_model_display(&model), "Error") == 0);
  CHECK(calculator_model_press(&model, 'B'));
  CHECK(strcmp(calculator_model_display(&model), "0") == 0);

  CHECK(calculator_model_press(&model, '1'));
  CHECK(calculator_model_press(&model, '/'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '.'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '1'));
  CHECK(calculator_model_press(&model, '='));
  CHECK(strcmp(calculator_model_display(&model), "1e+08") == 0);

  CHECK(calculator_model_press(&model, 'C'));
  CHECK(calculator_model_press(&model, '1'));
  CHECK(calculator_model_press(&model, '/'));
  CHECK(calculator_model_press(&model, '1'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '0'));
  CHECK(calculator_model_press(&model, '='));
  CHECK(strcmp(calculator_model_display(&model), "0.0001") == 0);

  CHECK(calculator_model_press(&model, '7'));
  CHECK(calculator_model_press(&model, 'S'));
  CHECK(strcmp(calculator_model_display(&model), "-7") == 0);
  CHECK(calculator_model_press(&model, 'B'));
  CHECK(strcmp(calculator_model_display(&model), "0") == 0);
  CHECK(!calculator_model_press(&model, '?'));

  CHECK(calculator_model_press(&model, 'C'));
  for (index = 0; index < CALCULATOR_MODEL_MAX_INPUT_LENGTH; index++)
    {
      CHECK(calculator_model_press(&model, '9'));
    }

  length = strlen(calculator_model_display(&model));
  CHECK(length == CALCULATOR_MODEL_MAX_INPUT_LENGTH);
  CHECK(!calculator_model_press(&model, '9'));
  CHECK(strlen(calculator_model_display(&model)) == length);
  CHECK(!calculator_model_press(NULL, '1'));
  return 0;
}

static void clear_2048(game_2048_model_t *model, uint32_t seed)
{
  memset(model, 0, sizeof(*model));
  model->seed = seed;
}

static int test_game_2048_model(void)
{
  game_2048_model_t first;
  game_2048_model_t second;
  game_2048_model_t snapshot;
  uint32_t seed;

  game_2048_model_init(&first, 0x20482048u);
  game_2048_model_init(&second, 0x20482048u);
  game_2048_model_new_game(&first);
  game_2048_model_new_game(&second);
  CHECK(memcmp(&first, &second, sizeof(first)) == 0);

  clear_2048(&first, 123u);
  first.board[0][0] = 2u;
  seed = first.seed;
  snapshot = first;
  CHECK(!game_2048_model_move(&first, GAME_2048_MOVE_LEFT));
  CHECK(memcmp(&first, &snapshot, sizeof(first)) == 0);
  CHECK(first.seed == seed);
  CHECK(first.board[0][0] == 2u);
  CHECK(!game_2048_model_move(&first, (game_2048_move_t)99));
  CHECK(first.seed == seed);

  clear_2048(&first, 456u);
  first.board[0][0] = 2u;
  first.board[0][1] = 2u;
  first.board[0][2] = 2u;
  first.board[0][3] = 2u;
  CHECK(game_2048_model_move(&first, GAME_2048_MOVE_LEFT));
  CHECK(first.board[0][0] == 4u);
  CHECK(first.board[0][1] == 4u);
  CHECK(game_2048_model_score(&first) == 8u);

  clear_2048(&first, 457u);
  first.board[0][0] = 2u;
  first.board[0][1] = 2u;
  first.board[0][2] = 4u;
  first.board[0][3] = 4u;
  CHECK(game_2048_model_move(&first, GAME_2048_MOVE_LEFT));
  CHECK(first.board[0][0] == 4u);
  CHECK(first.board[0][1] == 8u);
  CHECK(game_2048_model_score(&first) == 12u);

  clear_2048(&first, 458u);
  first.board[1][1] = 2u;
  first.board[1][2] = 2u;
  CHECK(game_2048_model_move(&first, GAME_2048_MOVE_RIGHT));
  CHECK(first.board[1][3] == 4u);

  clear_2048(&first, 459u);
  first.board[1][2] = 2u;
  first.board[2][2] = 2u;
  CHECK(game_2048_model_move(&first, GAME_2048_MOVE_UP));
  CHECK(first.board[0][2] == 4u);

  clear_2048(&first, 460u);
  first.board[1][2] = 2u;
  first.board[2][2] = 2u;
  CHECK(game_2048_model_move(&first, GAME_2048_MOVE_DOWN));
  CHECK(first.board[3][2] == 4u);

  clear_2048(&first, 789u);
  first.board[0][0] = 32768u;
  first.board[0][1] = 32768u;
  CHECK(game_2048_model_move(&first, GAME_2048_MOVE_LEFT));
  CHECK(first.board[0][0] == 65536u);

  clear_2048(&first, 790u);
  first.board[0][0] = UINT32_MAX / 2u + 1u;
  first.board[0][1] = UINT32_MAX / 2u + 1u;
  snapshot = first;
  CHECK(!game_2048_model_move(&first, GAME_2048_MOVE_LEFT));
  CHECK(memcmp(&first, &snapshot, sizeof(first)) == 0);

  clear_2048(&first, 1u);
  for (size_t row = 0; row < GAME_2048_SIZE; row++)
    {
      for (size_t column = 0; column < GAME_2048_SIZE; column++)
        {
          first.board[row][column] =
            ((row + column) % 2u == 0u) ? 2u : 4u;
        }
    }

  CHECK(!game_2048_model_can_move(&first));
  CHECK(!game_2048_model_move(&first, GAME_2048_MOVE_LEFT));
  CHECK(game_2048_model_game_over(&first));

  clear_2048(&first, 2u);
  for (size_t row = 0; row < GAME_2048_SIZE; row++)
    {
      for (size_t column = 0; column < GAME_2048_SIZE; column++)
        {
          first.board[row][column] =
            ((row + column) % 2u == 0u) ? 2u : 4u;
        }
    }

  first.board[0][0] = UINT32_MAX / 2u + 1u;
  first.board[0][1] = UINT32_MAX / 2u + 1u;
  CHECK(!game_2048_model_can_move(&first));
  seed = first.seed;
  CHECK(!game_2048_model_move(&first, GAME_2048_MOVE_LEFT));
  CHECK(first.seed == seed);
  CHECK(first.board[0][0] == UINT32_MAX / 2u + 1u);
  CHECK(first.board[0][1] == UINT32_MAX / 2u + 1u);
  CHECK(game_2048_model_game_over(&first));
  CHECK(game_2048_model_cell(NULL, 0, 0) == 0u);
  return 0;
}

static int count_mines(const mines_model_t *model)
{
  int count = 0;

  for (int index = 0; index < MINES_MODEL_CELLS; index++)
    {
      if (mines_model_is_mine(model, index))
        {
          count++;
        }
    }

  return count;
}

static int expected_neighbors(const mines_model_t *model, int index)
{
  int row = index / MINES_MODEL_SIZE;
  int column = index % MINES_MODEL_SIZE;
  int count = 0;

  for (int row_delta = -1; row_delta <= 1; row_delta++)
    {
      for (int column_delta = -1; column_delta <= 1; column_delta++)
        {
          int next_row = row + row_delta;
          int next_column = column + column_delta;
          int next_index;

          if ((row_delta == 0 && column_delta == 0) || next_row < 0 ||
              next_row >= MINES_MODEL_SIZE || next_column < 0 ||
              next_column >= MINES_MODEL_SIZE)
            {
              continue;
            }

          next_index = next_row * MINES_MODEL_SIZE + next_column;
          if (mines_model_is_mine(model, next_index))
            {
              count++;
            }
        }
    }

  return count;
}

static int test_mines_model(void)
{
  mines_model_t model;
  mines_model_t duplicate;
  mines_model_t snapshot;
  int mine_index = -1;
  mines_reveal_result_t result = MINES_REVEAL_INVALID;

  mines_model_init(&model, 0x5a17u);
  memset(&duplicate, 0, sizeof(duplicate));
  CHECK(mines_model_mine_count(&model) == 8);
  CHECK(count_mines(&model) == 8);
  CHECK(mines_model_new_game(&model, MINES_DIFFICULTY_EASY, 42u));
  CHECK(mines_model_mine_count(&model) == 5);
  CHECK(count_mines(&model) == 5);
  CHECK(mines_model_new_game(&duplicate, MINES_DIFFICULTY_EASY, 42u));
  CHECK(memcmp(&model, &duplicate, sizeof(model)) == 0);
  CHECK(mines_model_new_game(&model, MINES_DIFFICULTY_HARD, 43u));
  CHECK(mines_model_mine_count(&model) == 11);
  CHECK(count_mines(&model) == 11);

  snapshot = model;
  CHECK(!mines_model_new_game(&model, (mines_difficulty_t)99, 100u));
  CHECK(memcmp(&model, &snapshot, sizeof(model)) == 0);

  for (int index = 0; index < MINES_MODEL_CELLS; index++)
    {
      CHECK(mines_model_neighbor_count(&model, index) ==
            expected_neighbors(&model, index));
    }

  snapshot = model;
  CHECK(mines_model_reveal(&model, -1) == MINES_REVEAL_INVALID);
  CHECK(memcmp(&model, &snapshot, sizeof(model)) == 0);
  CHECK(mines_model_reveal(&model, MINES_MODEL_CELLS) ==
        MINES_REVEAL_INVALID);
  CHECK(memcmp(&model, &snapshot, sizeof(model)) == 0);

  for (int index = 0; index < MINES_MODEL_CELLS; index++)
    {
      if (mines_model_is_mine(&model, index))
        {
          mine_index = index;
          break;
        }
    }

  CHECK(mine_index >= 0);
  CHECK(mines_model_reveal(&model, mine_index) == MINES_REVEAL_MINE);
  CHECK(mines_model_status(&model) == MINES_STATUS_LOST);
  CHECK(mines_model_reveal(&model, mine_index) == MINES_REVEAL_IGNORED);

  for (uint32_t seed_value = 1u; seed_value <= 100u; seed_value++)
    {
      bool found_zero = false;

      CHECK(mines_model_new_game(&model, MINES_DIFFICULTY_EASY, seed_value));
      for (int index = 0; index < MINES_MODEL_CELLS; index++)
        {
          if (!mines_model_is_mine(&model, index) &&
              mines_model_neighbor_count(&model, index) == 0)
            {
              CHECK(mines_model_reveal(&model, index) == MINES_REVEAL_SAFE ||
                    mines_model_status(&model) == MINES_STATUS_WON);
              CHECK(mines_model_revealed_safe_count(&model) > 1);
              found_zero = true;
              break;
            }
        }

      if (found_zero)
        {
          break;
        }

      CHECK(seed_value < 100u);
    }

  CHECK(mines_model_new_game(&model, MINES_DIFFICULTY_EASY, 99u));
  for (int index = 0; index < MINES_MODEL_CELLS; index++)
    {
      if (!mines_model_is_mine(&model, index))
        {
          result = mines_model_reveal(&model, index);
        }
    }

  CHECK(result == MINES_REVEAL_WIN ||
        mines_model_status(&model) == MINES_STATUS_WON);
  CHECK(mines_model_status(&model) == MINES_STATUS_WON);
  CHECK(mines_model_revealed_safe_count(&model) ==
        MINES_MODEL_CELLS - mines_model_mine_count(&model));
  return 0;
}

int main(void)
{
  CHECK(test_calculator_model() == 0);
  CHECK(test_game_2048_model() == 0);
  CHECK(test_mines_model() == 0);
  puts("smart band production app logic tests passed");
  return 0;
}
