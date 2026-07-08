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
} calc_key_t;

static lv_obj_t *g_display;
static lv_obj_t *g_expression;
static char g_text[24] = "0";
static char g_lhs_text[24];
static double g_lhs;
static char g_op;
static bool g_has_lhs;
static bool g_reset_next;
static bool g_error;

static lv_coord_t calc_min_coord(lv_coord_t a, lv_coord_t b)
{
  return a < b ? a : b;
}

static lv_coord_t calc_max_coord(lv_coord_t a, lv_coord_t b)
{
  return a > b ? a : b;
}

static lv_coord_t calc_content_height(lv_obj_t *parent,
                                      const smart_band_app_host_t *host)
{
  lv_coord_t parent_h = lv_obj_get_height(parent);
  lv_coord_t visible_h = host->screen_h - lv_obj_get_y(parent) - 4;

  if (parent_h <= 0)
    {
      parent_h = visible_h;
    }

  if (visible_h > 0)
    {
      return calc_min_coord(parent_h, visible_h);
    }

  return parent_h;
}

static void calc_format_value(double value, char *buffer, size_t size)
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

static void calc_clear(void)
{
  snprintf(g_text, sizeof(g_text), "0");
  g_lhs_text[0] = '\0';
  g_lhs = 0.0;
  g_op = 0;
  g_has_lhs = false;
  g_reset_next = false;
  g_error = false;
}

static double calc_current_value(void)
{
  return strtod(g_text, NULL);
}

static void calc_error(void)
{
  snprintf(g_text, sizeof(g_text), "Err");
  g_lhs_text[0] = '\0';
  g_has_lhs = false;
  g_reset_next = true;
  g_error = true;
}

static bool calc_compute(double rhs)
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
            calc_error();
            return false;
          }

        result = g_lhs / rhs;
        break;

      default:
        return false;
    }

  calc_format_value(result, g_text, sizeof(g_text));
  g_lhs = result;
  return true;
}

static void calc_append_digit(char digit)
{
  size_t len;

  if (g_error || g_reset_next || strcmp(g_text, "0") == 0)
    {
      snprintf(g_text, sizeof(g_text), "%c", digit);
      g_reset_next = false;
      g_error = false;
      return;
    }

  len = strlen(g_text);
  if (len + 1 < sizeof(g_text))
    {
      g_text[len] = digit;
      g_text[len + 1] = '\0';
    }
}

static void calc_decimal(void)
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

static void calc_toggle_sign(void)
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

static void calc_backspace(void)
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
  if (len <= 1)
    {
      snprintf(g_text, sizeof(g_text), "0");
      return;
    }

  g_text[len - 1] = '\0';
  if (strcmp(g_text, "-") == 0)
    {
      snprintf(g_text, sizeof(g_text), "0");
    }
}

static void calc_set_operator(char op)
{
  if (g_error)
    {
      return;
    }

  if (g_has_lhs && !g_reset_next)
    {
      if (!calc_compute(calc_current_value()))
        {
          return;
        }
    }

  g_lhs = calc_current_value();
  calc_format_value(g_lhs, g_lhs_text, sizeof(g_lhs_text));
  g_op = op;
  g_has_lhs = true;
  g_reset_next = true;
}

static void calc_equals(void)
{
  if (!g_has_lhs || g_error)
    {
      return;
    }

  if (calc_compute(calc_current_value()))
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
      lv_obj_invalidate(g_display);
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

      lv_obj_invalidate(g_expression);
    }
}

static void calc_press(char key)
{
  printf("smart_band: calculator key %c\n", key);

  if (key >= '0' && key <= '9')
    {
      calc_append_digit(key);
    }
  else if (key == '.')
    {
      calc_decimal();
    }
  else if (key == '+' || key == '-' || key == '*' || key == '/')
    {
      calc_set_operator(key);
    }
  else if (key == '=')
    {
      calc_equals();
    }
  else if (key == 'B')
    {
      calc_backspace();
    }
  else if (key == 'S')
    {
      calc_toggle_sign();
    }
  else
    {
      calc_clear();
    }

  smart_band_calculator_app_update(NULL);
}

static void calc_key_cb(lv_event_t *event)
{
  uintptr_t code = (uintptr_t)lv_event_get_user_data(event);

  if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
      calc_press((char)code);
    }
}

static lv_obj_t *calc_make_key(lv_obj_t *parent,
                               const smart_band_app_host_t *host,
                               const calc_key_t *key,
                               lv_coord_t x, lv_coord_t y,
                               lv_coord_t w, lv_coord_t h)
{
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_t *label;

  if (button == NULL || key == NULL)
    {
      return NULL;
    }

  lv_obj_remove_style_all(button);
  lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, w, h);
  lv_obj_set_style_bg_color(button, lv_color_hex(key->color), 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x243647), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(button, calc_min_coord(host->sx(12), h / 3), 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_shadow_width(button, host->sx(3), 0);
  lv_obj_set_style_shadow_color(button, lv_color_hex(0x314856), 0);
  lv_obj_set_style_shadow_opa(button, LV_OPA_20, 0);
  lv_obj_set_style_shadow_offset_y(button, 2, 0);
  lv_obj_add_event_cb(button, calc_key_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)key->code);

  label = lv_label_create(button);
  if (label == NULL)
    {
      return NULL;
    }

  lv_obj_remove_style_all(label);
  lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_text(label, key->label);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(label, host->font_14(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(label, 2, (h - host->sy(20)) / 2);
  lv_obj_set_size(label, w - 4, host->sy(22));
  lv_obj_add_event_cb(label, calc_key_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)key->code);
  return button;
}

int smart_band_calculator_app_build(lv_obj_t *parent,
                                    const smart_band_app_host_t *host)
{
  static const calc_key_t keys[] =
  {
    { "C",   'C', 0xee8582, 0, 0, 1, 1 },
    { "DEL", 'B', 0x8298cb, 1, 0, 1, 1 },
    { "/",   '/', 0x72c4bc, 2, 0, 1, 1 },
    { "*",   '*', 0x72c4bc, 3, 0, 1, 1 },
    { "7",   '7', 0x667f89, 0, 1, 1, 1 },
    { "8",   '8', 0x667f89, 1, 1, 1, 1 },
    { "9",   '9', 0x667f89, 2, 1, 1, 1 },
    { "-",   '-', 0x72c4bc, 3, 1, 1, 1 },
    { "4",   '4', 0x667f89, 0, 2, 1, 1 },
    { "5",   '5', 0x667f89, 1, 2, 1, 1 },
    { "6",   '6', 0x667f89, 2, 2, 1, 1 },
    { "+",   '+', 0x72c4bc, 3, 2, 1, 1 },
    { "1",   '1', 0x667f89, 0, 3, 1, 1 },
    { "2",   '2', 0x667f89, 1, 3, 1, 1 },
    { "3",   '3', 0x667f89, 2, 3, 1, 1 },
    { "=",   '=', 0xf0bc62, 3, 3, 1, 2 },
    { "+/-", 'S', 0x667f89, 0, 4, 1, 1 },
    { "0",   '0', 0x667f89, 1, 4, 1, 1 },
    { ".",   '.', 0x667f89, 2, 4, 1, 1 }
  };
  lv_coord_t page_h = calc_content_height(parent, host);
  lv_coord_t margin = calc_max_coord(host->sx(10), 8);
  lv_coord_t gap_x = calc_max_coord(host->sx(6), 5);
  lv_coord_t gap_y = calc_max_coord(host->sy(6), 4);
  lv_coord_t display_h = calc_max_coord(host->sy(58), 42);
  lv_coord_t grid_y = display_h + gap_y + 4;
  lv_coord_t usable_h = page_h - grid_y - 4;
  lv_coord_t key_w;
  lv_coord_t key_h;
  lv_obj_t *display_box;

  if (usable_h < 5 * 34 + 4 * gap_y)
    {
      display_h = calc_max_coord(host->sy(44), 34);
      grid_y = display_h + gap_y + 2;
      usable_h = page_h - grid_y - 2;
    }

  key_w = (host->screen_w - margin * 2 - gap_x * 3) / 4;
  key_h = (usable_h - gap_y * 4) / 5;
  if (key_h < 28)
    {
      key_h = 28;
    }

  calc_clear();
  g_display = NULL;
  g_expression = NULL;

  display_box = host->create_box(parent, margin, 0,
                                 host->screen_w - margin * 2, display_h,
                                 lv_color_hex(0xf4f8fb), host->sx(14));
  if (display_box == NULL)
    {
      return -1;
    }

  lv_obj_set_style_border_width(display_box, 1, 0);
  lv_obj_set_style_border_color(display_box, lv_color_hex(0xe1edf1), 0);

  g_expression = host->create_label(display_box, "", host->font_12(),
                                    lv_color_hex(0x7e9198),
                                    LV_TEXT_ALIGN_RIGHT);
  g_display = host->create_label(display_box, g_text, host->font_20(),
                                 lv_color_hex(0x293b53),
                                 LV_TEXT_ALIGN_RIGHT);
  if (g_expression == NULL || g_display == NULL)
    {
      return -1;
    }

  host->place_label(g_expression, host->sx(10), 4,
                    lv_obj_get_width(display_box) - host->sx(20),
                    calc_min_coord(host->sy(18), display_h / 3));
  host->place_label(g_display, host->sx(10), display_h / 3,
                    lv_obj_get_width(display_box) - host->sx(20),
                    display_h - display_h / 3 - 4);

  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
      const calc_key_t *key = &keys[i];
      lv_coord_t x = margin + key->col * (key_w + gap_x);
      lv_coord_t y = grid_y + key->row * (key_h + gap_y);
      lv_coord_t w = key_w * key->col_span + gap_x * (key->col_span - 1);
      lv_coord_t h = key_h * key->row_span + gap_y * (key->row_span - 1);

      if (calc_make_key(parent, host, key, x, y, w, h) == NULL)
        {
          return -1;
        }
    }

  smart_band_calculator_app_update(host);
  return 0;
}
