#ifndef GAME_2048_MODEL_H
#define GAME_2048_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GAME_2048_SIZE 4
#define GAME_2048_CELLS (GAME_2048_SIZE * GAME_2048_SIZE)

typedef enum
{
  GAME_2048_MOVE_UP = 0,
  GAME_2048_MOVE_LEFT,
  GAME_2048_MOVE_RIGHT,
  GAME_2048_MOVE_DOWN
} game_2048_move_t;

typedef struct
{
  uint32_t board[GAME_2048_SIZE][GAME_2048_SIZE];
  uint64_t score;
  uint32_t seed;
  bool won;
  bool game_over;
} game_2048_model_t;

void game_2048_model_init(game_2048_model_t *model, uint32_t seed);
void game_2048_model_new_game(game_2048_model_t *model);
bool game_2048_model_move(game_2048_model_t *model,
                          game_2048_move_t direction);
bool game_2048_model_can_move(const game_2048_model_t *model);
uint32_t game_2048_model_cell(const game_2048_model_t *model,
                              size_t row, size_t column);
uint64_t game_2048_model_score(const game_2048_model_t *model);
bool game_2048_model_won(const game_2048_model_t *model);
bool game_2048_model_game_over(const game_2048_model_t *model);

#endif
