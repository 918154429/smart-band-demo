#ifndef MINES_MODEL_H
#define MINES_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define MINES_MODEL_SIZE 6
#define MINES_MODEL_CELLS (MINES_MODEL_SIZE * MINES_MODEL_SIZE)

typedef enum
{
  MINES_DIFFICULTY_EASY = 0,
  MINES_DIFFICULTY_MEDIUM,
  MINES_DIFFICULTY_HARD,
  MINES_DIFFICULTY_COUNT
} mines_difficulty_t;

typedef enum
{
  MINES_STATUS_PLAYING = 0,
  MINES_STATUS_WON,
  MINES_STATUS_LOST
} mines_status_t;

typedef enum
{
  MINES_REVEAL_INVALID = 0,
  MINES_REVEAL_IGNORED,
  MINES_REVEAL_SAFE,
  MINES_REVEAL_MINE,
  MINES_REVEAL_WIN
} mines_reveal_result_t;

typedef struct
{
  uint64_t revealed;
  uint64_t mines;
  uint32_t seed;
  mines_difficulty_t difficulty;
  int mine_count;
  bool over;
} mines_model_t;

void mines_model_init(mines_model_t *model, uint32_t seed);
bool mines_model_new_game(mines_model_t *model,
                          mines_difficulty_t difficulty,
                          uint32_t seed);
mines_reveal_result_t mines_model_reveal(mines_model_t *model, int index);

int mines_model_neighbor_count(const mines_model_t *model, int index);
mines_status_t mines_model_status(const mines_model_t *model);
bool mines_model_is_revealed(const mines_model_t *model, int index);
bool mines_model_is_mine(const mines_model_t *model, int index);
mines_difficulty_t mines_model_difficulty(const mines_model_t *model);
int mines_model_mine_count(const mines_model_t *model);
int mines_model_revealed_safe_count(const mines_model_t *model);
uint32_t mines_model_seed(const mines_model_t *model);

#endif
