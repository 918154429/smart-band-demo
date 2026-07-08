#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  const char *label;
  char code;
  uint32_t color;
  uint8_t col;
  uint8_t row;
  uint8_t col_span;
  uint8_t row_span;
} calculator_key_t;

static lv_obj_t *g_display;
static lv_obj_t *g_expression;
static char g_text[24] = "0";
static char g_lhs_text[24];
static double g_lhs;
static char g_op;
static bool g_has_lhs;
static bool g_reset_next;
static bool g_error;

static void calculator_format_value(double value, char *buffer, size_t size)
{
  char *dot;
  char *end;
  double magnitude;

  if (value > -0.0005 && value < 0.0005)
    {
      value = 0.0;
    }

  magnitude = value < 0.0 ? -value : value;
  if (magnitude >= 10000000.0)
    {
      snprintf(buffer, size, "%.4g", value);
      return;
    }

  if (value == (double)(long)value)
    {
      snprintf(buffer, size, "%ld", (long)value);
      return;
    }

  snprintf(buffer, size, "%.4f", value);
  dot = strchr(buffer, '.');
  if (dot == NULL)
    {
      return;
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
}

static void calculator_clear_all(void)
{
  snprintf(g_text, sizeof(g_text), "0");
  g_lhs_text[0] = '\0';
  g_lhs = 0.0;
  g_op = 0;
  g_has_lhs = false;
  g_reset_next = false;
  g_error = false;
}

static double calculator_display_value(void)
{
  return strtod(g_text, NULL);
}

static void calculator_set_error(void)
{
  snprintf(g_text, sizeof(g_text), "Err");
  g_lhs_text[0] = '\0';
  g_has_lhs = false;
  g_reset_next = true;
  g_error = true;
}

static bool calculator_compute(double rhs)
{
  double result;

  switch (g_op)
    {
      case '+':
        result = g_lhs + rhs;
        break;

      case '-':
        result = g_lhs - rhs;
        break;

      case '*':
        result = g_lhs * rhs;
        break;

      case '/':
        if (rhs > -0.0000001 && rhs < 0.0000001)
          {
            calculator_set_error();
            return false;
          }

        result = g_lhs / rhs;
        break;

      default:
        return false;
    }

  calculator_format_value(result, g_text, sizeof(g_text));
  g_lhs = result;
  return true;
}

static void calculator_append_char(char key)
{
  size_t len;

  if (g_error || g_reset_next || strcmp(g_text, "0") == 0)
    {
      snprintf(g_text, sizeof(g_text), "%c", key);
      g_reset_next = false;
      g_error = false;
      return;
    }

  len = strlen(g_text);
  if (len + 1 < sizeof(g_text))
    {
      g_text[len] = key;
      g_text[len + 1] = '\0';
    }
}

static void calculator_append_decimal(void)
{
  size_t len;

  if (g_error || g_reset_next)
    {
      snprintf(g_text, sizeof(g_text), "0.");
      g_reset_next = false;
      g_error = false;
      return;
    }

  if (strchr(g_text, '.') != NULL)
    {
      return;
    }

  len = strlen(g_text);
  if (len + 1 < sizeof(g_text))
    {
      g_text[len] = '.';
      g_text[len + 1] = '\0';
    }
}

static void calculator_toggle_sign(void)
{
  size_t len;

  if (g_error || strcmp(g_text, "0") == 0)
    {
      return;
    }

  if (g_text[0] == '-')
    {
      memmove(g_text, g_text + 1, strlen(g_text));
      return;
    }

  len = strlen(g_text);
  if (len + 1 < sizeof(g_text))
    {
      memmove(g_text + 1, g_text, len + 1);
      g_text[0] = '-';
    }
}

static void calculator_backspace(void)
{
  size_t len;

  if (g_error || g_reset_next)
    {
      snprintf(g_text, sizeof(g_text), "0");
      g_reset_next = false;
      g_error = false;
      return;
    }

  len = strlen(g_text);
  if (len > 1)
    {
      g_text[len - 1] = '\0';
      if (strcmp(g_text, "-") == 0)
        {
          snprintf(g_text, sizeof(g_text), "0");
        }
    }
  else
    {
      snprintf(g_text, sizeof(g_text), "0");
    }
}

static void calculator_set_operator(char key)
{
  if (g_error)
    {
      return;
    }

  if (g_has_lhs && !g_reset_next)
    {
      if (!calculator_compute(calculator_display_value()))
        {
          return;
        }
    }

  g_lhs = calculator_display_value();
  calculator_format_value(g_lhs, g_lhs_text, sizeof(g_lhs_text));
  g_op = key;
  g_has_lhs = true;
  g_reset_next = true;
}

static void calculator_equals(void)
{
  if (!g_has_lhs || g_error)
    {
      return;
    }

  if (calculator_compute(calculator_display_value()))
    {
      g_lhs_text[0] = '\0';
      g_has_lhs = false;
      g_reset_next = true;
    }
}

void smart_band_calculator_app_update(const smart_band_app_host_t *host)
{
  char expression[32];

  (void)host;
  if (g_display != NULL)
    {
      lv_label_set_text(g_display, g_text);
    }

  if (g_expression != NULL)
    {
      if (g_has_lhs)
        {
          snprintf(expression, sizeof(expression), "%s %c", g_lhs_text, g_op);
          lv_label_set_text(g_expression, expression);
        }
      else
        {
          lv_label_set_text(g_expression, "");
        }
    }
}

static void calculator_cb(lv_event_t *event)
{
  const calculator_key_t *key_info =
    (const calculator_key_t *)lv_event_get_user_data(event);
  char key;

  if (key_info == NULL)
    {
      return;
    }

  key = key_info->code;
  if (key >= '0' && key <= '9')
    {
      calculator_append_char(key);
    }
  else if (key == '.')
    {
      calculator_append_decimal();
    }
  else if (key == '+' || key == '-' || key == '*' || key == '/')
    {
      calculator_set_operator(key);
    }
  else if (key == '=')
    {
      calculator_equals();
    }
  else if (key == 'B')
    {
      calculator_backspace();
    }
  else if (key == 'S')
    {
      calculator_toggle_sign();
    }
  else
    {
      calculator_clear_all();
    }

  smart_band_calculator_app_update(NULL);
}

int smart_band_calculator_app_build(lv_obj_t *parent,
                                    const smart_band_app_host_t *host)
{
  static const calculator_key_t keys[] =
  {
    { "C",   'C', 0xf08d88, 0, 0, 1, 1 },
    { "DEL", 'B', 0x8aa8d8, 1, 0, 1, 1 },
    { "/",   '/', 0x80cbc3, 2, 0, 1, 1 },
    { "*",   '*', 0x80cbc3, 3, 0, 1, 1 },
    { "7",   '7', 0x6f8790, 0, 1, 1, 1 },
    { "8",   '8', 0x6f8790, 1, 1, 1, 1 },
    { "9",   '9', 0x6f8790, 2, 1, 1, 1 },
    { "-",   '-', 0x80cbc3, 3, 1, 1, 1 },
    { "4",   '4', 0x6f8790, 0, 2, 1, 1 },
    { "5",   '5', 0x6f8790, 1, 2, 1, 1 },
    { "6",   '6', 0x6f8790, 2, 2, 1, 1 },
    { "+",   '+', 0x80cbc3, 3, 2, 1, 1 },
    { "1",   '1', 0x6f8790, 0, 3, 1, 1 },
    { "2",   '2', 0x6f8790, 1, 3, 1, 1 },
    { "3",   '3', 0x6f8790, 2, 3, 1, 1 },
    { "=",   '=', 0xf5c66e, 3, 3, 1, 2 },
    { "+/-", 'S', 0x6f8790, 0, 4, 1, 1 },
    { "0",   '0', 0x6f8790, 1, 4, 1, 1 },
    { ".",   '.', 0x6f8790, 2, 4, 1, 1 }
  };
  lv_coord_t margin = host->sx(16);
  lv_coord_t gap_x = host->sx(7);
  lv_coord_t gap_y = host->sy(7);
  lv_coord_t display_h = host->sy(66);
  lv_coord_t grid_y = host->sy(82);
  lv_coord_t key_w = (host->screen_w - margin * 2 - gap_x * 3) / 4;
  lv_coord_t key_h = host->sy(46);
  lv_obj_t *display_box;

  calculator_clear_all();
  g_display = NULL;
  g_expression = NULL;

  display_box = host->create_box(parent, margin, host->sy(4),
                                 host->screen_w - margin * 2, display_h,
                                 lv_color_hex(0xf3f7fb), host->sx(18));
  if (display_box == NULL)
    {
      return -1;
    }

  lv_obj_set_style_border_width(display_box, 1, 0);
  lv_obj_set_style_border_color(display_box, lv_color_hex(0xe3edf2), 0);

  g_expression = host->create_label(display_box, "", host->font_12(),
                                    lv_color_hex(0x81939a),
                                    LV_TEXT_ALIGN_RIGHT);
  g_display = host->create_label(display_box, g_text, host->font_32(),
                                 lv_color_hex(0x293b53),
                                 LV_TEXT_ALIGN_RIGHT);
  if (g_expression == NULL || g_display == NULL)
    {
      return -1;
    }

  host->place_label(g_expression, host->sx(12), host->sy(7),
                    lv_obj_get_width(display_box) - host->sx(24),
                    host->sy(18));
  host->place_label(g_display, host->sx(12), host->sy(24),
                    lv_obj_get_width(display_box) - host->sx(24),
                    host->sy(36));

  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
      const calculator_key_t *key = &keys[i];
      lv_coord_t x = margin + key->col * (key_w + gap_x);
      lv_coord_t y = grid_y + key->row * (key_h + gap_y);
      lv_coord_t w = key_w * key->col_span +
                     gap_x * (key->col_span - 1);
      lv_coord_t h = key_h * key->row_span +
                     gap_y * (key->row_span - 1);
      lv_obj_t *button;

      button = host->create_action_button(parent, key->label, x, y, w, h,
                                          lv_color_hex(key->color),
                                          calculator_cb, (uintptr_t)key);
      if (button == NULL)
        {
          return -1;
        }

      lv_obj_set_style_radius(button, host->sx(12), 0);
      lv_obj_set_style_shadow_width(button, host->sx(4), 0);
    }

  smart_band_calculator_app_update(host);
  return 0;
}
