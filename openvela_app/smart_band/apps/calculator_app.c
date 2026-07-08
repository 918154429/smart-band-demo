#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  if (g_display != NULL)
    {
      lv_obj_invalidate(g_display);
    }

  if (g_expression != NULL)
    {
      lv_obj_invalidate(g_expression);
    }

  lv_refr_now(NULL);
}

static void calculator_handle_key_text(const char *key_text)
{
  char key;

  if (key_text == NULL || key_text[0] == '\0')
    {
      return;
    }

  if (strcmp(key_text, "DEL") == 0)
    {
      key = 'B';
    }
  else if (strcmp(key_text, "+/-") == 0)
    {
      key = 'S';
    }
  else
    {
      key = key_text[0];
    }

  printf("smart_band: calculator key %s\n", key_text);
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

static void calculator_keypad_cb(lv_event_t *event)
{
  lv_obj_t *keypad = (lv_obj_t *)lv_event_get_target(event);
  uint32_t id;
  const char *text;

  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED || keypad == NULL)
    {
      return;
    }

  id = lv_buttonmatrix_get_selected_button(keypad);
  text = lv_buttonmatrix_get_button_text(keypad, id);
  calculator_handle_key_text(text);
}

static lv_coord_t calculator_visible_height(lv_obj_t *parent,
                                            const smart_band_app_host_t *host)
{
  lv_coord_t parent_h = lv_obj_get_height(parent);
  lv_coord_t parent_y = lv_obj_get_y(parent);
  lv_coord_t visible_h;

  if (parent_h <= 0)
    {
      parent_h = host->screen_h;
    }

  visible_h = host->screen_h - parent_y - host->sy(8);
  if (visible_h > 0 && visible_h < parent_h)
    {
      return visible_h;
    }

  return parent_h;
}

int smart_band_calculator_app_build(lv_obj_t *parent,
                                    const smart_band_app_host_t *host)
{
  static const char *button_map[] =
  {
    "C", "DEL", "/", "*", "\n",
    "7", "8", "9", "-", "\n",
    "4", "5", "6", "+", "\n",
    "1", "2", "3", "=", "\n",
    "+/-", "0", ".", "=",
    ""
  };
  lv_coord_t margin = host->sx(16);
  lv_coord_t page_h = calculator_visible_height(parent, host);
  lv_coord_t display_h = host->sy(56);
  lv_coord_t grid_y = host->sy(68);
  lv_coord_t keypad_h = page_h - grid_y - host->sy(8);
  lv_obj_t *display_box;
  lv_obj_t *keypad;

  if (keypad_h < host->sy(250) &&
      page_h > grid_y + host->sy(258))
    {
      keypad_h = host->sy(250);
    }

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

  keypad = lv_btnmatrix_create(parent);
  if (keypad == NULL)
    {
      return -1;
    }

  lv_obj_remove_style_all(keypad);
  lv_obj_add_flag(keypad, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(keypad, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(keypad, margin, grid_y);
  lv_obj_set_size(keypad, host->screen_w - margin * 2, keypad_h);
  lv_obj_set_style_bg_opa(keypad, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(keypad, 0, 0);
  lv_obj_set_style_pad_all(keypad, 0, 0);
  lv_obj_set_style_pad_row(keypad, host->sy(7), 0);
  lv_obj_set_style_pad_column(keypad, host->sx(7), 0);
  lv_obj_set_style_bg_color(keypad, lv_color_hex(0x6f8790), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(keypad, lv_color_hex(0x293b53),
                            LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(keypad, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_radius(keypad, host->sx(12), LV_PART_ITEMS);
  lv_obj_set_style_border_width(keypad, 0, LV_PART_ITEMS);
  lv_obj_set_style_text_font(keypad, host->font_14(), LV_PART_ITEMS);
  lv_obj_set_style_text_color(keypad, lv_color_hex(0xffffff), LV_PART_ITEMS);
  lv_obj_set_style_text_align(keypad, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
  lv_obj_set_style_shadow_width(keypad, host->sx(3), LV_PART_ITEMS);
  lv_obj_set_style_shadow_color(keypad, lv_color_hex(0x314856), LV_PART_ITEMS);
  lv_obj_set_style_shadow_opa(keypad, LV_OPA_20, LV_PART_ITEMS);
  lv_obj_set_style_shadow_offset_y(keypad, host->sy(2), LV_PART_ITEMS);
  lv_buttonmatrix_set_map(keypad, button_map);
  lv_obj_add_event_cb(keypad, calculator_keypad_cb, LV_EVENT_VALUE_CHANGED,
                      NULL);

  smart_band_calculator_app_update(host);
  return 0;
}
