#ifndef SMART_BAND_CALCULATOR_MODEL_H
#define SMART_BAND_CALCULATOR_MODEL_H

#include <stdbool.h>

#define CALCULATOR_MODEL_TEXT_CAPACITY 24
#define CALCULATOR_MODEL_EXPRESSION_CAPACITY 32
#define CALCULATOR_MODEL_MAX_INPUT_LENGTH \
  (CALCULATOR_MODEL_TEXT_CAPACITY - 1)

typedef struct
{
  char text[CALCULATOR_MODEL_TEXT_CAPACITY];
  char lhs_text[CALCULATOR_MODEL_TEXT_CAPACITY];
  char expression[CALCULATOR_MODEL_EXPRESSION_CAPACITY];
  double lhs;
  char op;
  bool has_lhs;
  bool reset_next;
  bool error;
} calculator_model_t;

void calculator_model_init(calculator_model_t *model);
bool calculator_model_press(calculator_model_t *model, char key);
const char *calculator_model_display(const calculator_model_t *model);
const char *calculator_model_expression(const calculator_model_t *model);

#endif
