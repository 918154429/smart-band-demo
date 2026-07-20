#include "calculator_model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void calculator_model_update_expression(calculator_model_t *model)
{
  if (model->has_lhs)
    {
      (void)snprintf(model->expression, sizeof(model->expression), "%s %c",
                     model->lhs_text, model->op);
    }
  else
    {
      model->expression[0] = '\0';
    }
}

static bool calculator_model_format_value(double value, char *buffer,
                                          size_t size)
{
  char *dot;
  char *end;
  double magnitude;

  if (!isfinite(value))
    {
      return false;
    }

  if (value == 0.0)
    {
      value = 0.0;
    }

  magnitude = value < 0.0 ? -value : value;
  if (magnitude >= 10000000.0)
    {
      (void)snprintf(buffer, size, "%.4g", value);
      return true;
    }

  if (value == (double)(long)value)
    {
      (void)snprintf(buffer, size, "%ld", (long)value);
      return true;
    }

  (void)snprintf(buffer, size, "%.4f", value);
  dot = strchr(buffer, '.');
  if (dot == NULL)
    {
      return true;
    }

  end = buffer + strlen(buffer);
  while (end > dot + 1 && end[-1] == '0')
    {
      end--;
    }

  if (end > dot && end[-1] == '.')
    {
      end--;
    }

  *end = '\0';
  return true;
}

static void calculator_model_clear(calculator_model_t *model)
{
  (void)snprintf(model->text, sizeof(model->text), "0");
  model->lhs_text[0] = '\0';
  model->expression[0] = '\0';
  model->lhs = 0.0;
  model->op = 0;
  model->has_lhs = false;
  model->reset_next = false;
  model->error = false;
}

static void calculator_model_set_error(calculator_model_t *model)
{
  (void)snprintf(model->text, sizeof(model->text), "Error");
  model->lhs_text[0] = '\0';
  model->expression[0] = '\0';
  model->lhs = 0.0;
  model->op = 0;
  model->has_lhs = false;
  model->reset_next = true;
  model->error = true;
}

static bool calculator_model_current_value(const calculator_model_t *model,
                                           double *value)
{
  char *end;

  *value = strtod(model->text, &end);
  return end != model->text && *end == '\0' && isfinite(*value);
}

static bool calculator_model_compute(calculator_model_t *model, double rhs)
{
  double result;

  switch (model->op)
    {
      case '+':
        result = model->lhs + rhs;
        break;

      case '-':
        result = model->lhs - rhs;
        break;

      case '*':
        result = model->lhs * rhs;
        break;

      case '/':
        if (rhs == 0.0)
          {
            calculator_model_set_error(model);
            return false;
          }

        result = model->lhs / rhs;
        break;

      default:
        return false;
    }

  if (!calculator_model_format_value(result, model->text,
                                     sizeof(model->text)))
    {
      calculator_model_set_error(model);
      return false;
    }

  model->lhs = result;
  return true;
}

static bool calculator_model_append_digit(calculator_model_t *model,
                                          char digit)
{
  size_t length;

  if (model->error || model->reset_next || strcmp(model->text, "0") == 0)
    {
      (void)snprintf(model->text, sizeof(model->text), "%c", digit);
      model->reset_next = false;
      model->error = false;
      return true;
    }

  length = strlen(model->text);
  if (length + 1 < sizeof(model->text))
    {
      model->text[length] = digit;
      model->text[length + 1] = '\0';
      return true;
    }

  return false;
}

static bool calculator_model_append_decimal(calculator_model_t *model)
{
  size_t length;

  if (model->error || model->reset_next)
    {
      (void)snprintf(model->text, sizeof(model->text), "0.");
      model->reset_next = false;
      model->error = false;
      return true;
    }

  if (strchr(model->text, '.') != NULL)
    {
      return false;
    }

  length = strlen(model->text);
  if (length + 1 < sizeof(model->text))
    {
      model->text[length] = '.';
      model->text[length + 1] = '\0';
      return true;
    }

  return false;
}

static bool calculator_model_toggle_sign(calculator_model_t *model)
{
  size_t length;

  if (model->error || strcmp(model->text, "0") == 0)
    {
      return false;
    }

  if (model->text[0] == '-')
    {
      (void)memmove(model->text, model->text + 1, strlen(model->text));
      return true;
    }

  length = strlen(model->text);
  if (length + 1 < sizeof(model->text))
    {
      (void)memmove(model->text + 1, model->text, length + 1);
      model->text[0] = '-';
      return true;
    }

  return false;
}

static bool calculator_model_backspace(calculator_model_t *model)
{
  size_t length;

  if (model->error || model->reset_next)
    {
      (void)snprintf(model->text, sizeof(model->text), "0");
      model->reset_next = false;
      model->error = false;
      return true;
    }

  length = strlen(model->text);
  if (length <= 1)
    {
      (void)snprintf(model->text, sizeof(model->text), "0");
      return true;
    }

  model->text[length - 1] = '\0';
  if (strcmp(model->text, "-") == 0)
    {
      (void)snprintf(model->text, sizeof(model->text), "0");
    }

  return true;
}

static bool calculator_model_set_operator(calculator_model_t *model, char op)
{
  double current;

  if (model->error || !calculator_model_current_value(model, &current))
    {
      return false;
    }

  if (model->has_lhs && !model->reset_next)
    {
      if (!calculator_model_compute(model, current))
        {
          return model->error;
        }

      current = model->lhs;
    }

  model->lhs = current;
  if (!calculator_model_format_value(model->lhs, model->lhs_text,
                                     sizeof(model->lhs_text)))
    {
      calculator_model_set_error(model);
      return false;
    }

  model->op = op;
  model->has_lhs = true;
  model->reset_next = true;
  return true;
}

static bool calculator_model_equals(calculator_model_t *model)
{
  bool computed;
  double current;

  if (!model->has_lhs || model->error ||
      !calculator_model_current_value(model, &current))
    {
      return false;
    }

  computed = calculator_model_compute(model, current);
  if (computed)
    {
      model->lhs_text[0] = '\0';
      model->op = 0;
      model->has_lhs = false;
      model->reset_next = true;
    }

  return computed || model->error;
}

void calculator_model_init(calculator_model_t *model)
{
  if (model != NULL)
    {
      calculator_model_clear(model);
    }
}

bool calculator_model_press(calculator_model_t *model, char key)
{
  bool accepted;

  if (model == NULL)
    {
      return false;
    }

  if (key >= '0' && key <= '9')
    {
      accepted = calculator_model_append_digit(model, key);
    }
  else if (key == '.')
    {
      accepted = calculator_model_append_decimal(model);
    }
  else if (key == '+' || key == '-' || key == '*' || key == '/')
    {
      accepted = calculator_model_set_operator(model, key);
    }
  else if (key == '=')
    {
      accepted = calculator_model_equals(model);
    }
  else if (key == 'B')
    {
      accepted = calculator_model_backspace(model);
    }
  else if (key == 'S')
    {
      accepted = calculator_model_toggle_sign(model);
    }
  else if (key == 'C')
    {
      calculator_model_clear(model);
      accepted = true;
    }
  else
    {
      return false;
    }

  if (accepted)
    {
      calculator_model_update_expression(model);
    }

  return accepted;
}

const char *calculator_model_display(const calculator_model_t *model)
{
  return model == NULL ? "" : model->text;
}

const char *calculator_model_expression(const calculator_model_t *model)
{
  return model == NULL ? "" : model->expression;
}
