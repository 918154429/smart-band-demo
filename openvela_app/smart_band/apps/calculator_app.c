#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_obj_t *g_display;
static char g_text[24] = "0";
static int g_lhs;
static char g_op;
static bool g_has_lhs;
static bool g_reset_next;

void smart_band_calculator_app_update(const smart_band_app_host_t *host)
{
  (void)host;
  if (g_display != NULL)
    {
      lv_label_set_text(g_display, g_text);
    }
}

static void calculator_cb(lv_event_t *event)
{
  char key = (char)(uintptr_t)lv_event_get_user_data(event);
  size_t len;

  if (key >= '0' && key <= '9')
    {
      if (g_reset_next || strcmp(g_text, "0") == 0)
        {
          snprintf(g_text, sizeof(g_text), "%c", key);
          g_reset_next = false;
        }
      else
        {
          len = strlen(g_text);
          if (len + 1 < sizeof(g_text))
            {
              g_text[len] = key;
              g_text[len + 1] = '\0';
            }
        }
    }
  else if (key == '+' || key == '-')
    {
      g_lhs = atoi(g_text);
      g_op = key;
      g_has_lhs = true;
      g_reset_next = true;
    }
  else if (key == '=')
    {
      if (g_has_lhs)
        {
          int rhs = atoi(g_text);
          int result = g_op == '-' ? g_lhs - rhs : g_lhs + rhs;

          snprintf(g_text, sizeof(g_text), "%d", result);
          g_has_lhs = false;
          g_reset_next = true;
        }
    }
  else if (key == '<')
    {
      len = strlen(g_text);
      if (len > 1)
        {
          g_text[len - 1] = '\0';
        }
      else
        {
          snprintf(g_text, sizeof(g_text), "0");
        }
    }
  else
    {
      snprintf(g_text, sizeof(g_text), "0");
      g_has_lhs = false;
      g_reset_next = false;
    }

  smart_band_calculator_app_update(NULL);
}

int smart_band_calculator_app_build(lv_obj_t *parent,
                                    const smart_band_app_host_t *host)
{
  static const char *const keys[] =
  {
    "7", "8", "9", "+",
    "4", "5", "6", "-",
    "1", "2", "3", "=",
    "C", "0", "<", "="
  };

  lv_coord_t margin = host->sx(22);
  lv_coord_t gap = host->sx(8);
  lv_coord_t display_h = host->sy(78);
  lv_coord_t key_w = (host->screen_w - margin * 2 - gap * 3) / 4;
  lv_coord_t key_h = host->sy(54);
  lv_obj_t *display_box;

  g_display = NULL;
  if (g_text[0] == '\0')
    {
      snprintf(g_text, sizeof(g_text), "0");
    }

  display_box = host->create_box(parent, margin, host->sy(8),
                                 host->screen_w - margin * 2, display_h,
                                 lv_color_hex(0xf3f7fb), host->sx(20));
  if (display_box == NULL)
    {
      return -1;
    }

  g_display = host->create_label(display_box, g_text, host->font_32(),
                                 lv_color_hex(0x293b53),
                                 LV_TEXT_ALIGN_RIGHT);
  if (g_display == NULL)
    {
      return -1;
    }

  host->place_label(g_display, host->sx(12), host->sy(18),
                    lv_obj_get_width(display_box) - host->sx(24),
                    host->sy(44));

  for (int i = 0; i < 16; i++)
    {
      int row = i / 4;
      int col = i % 4;
      lv_color_t color = (col == 3 || i >= 12) ?
                         lv_color_hex(0x80cbc3) :
                         lv_color_hex(0x6f8790);

      if (host->create_action_button(parent, keys[i],
                                     margin + col * (key_w + gap),
                                     host->sy(102) +
                                     row * (key_h + host->sy(8)),
                                     key_w, key_h, color, calculator_cb,
                                     (uintptr_t)keys[i][0]) == NULL)
        {
          return -1;
        }
    }

  smart_band_calculator_app_update(host);
  return 0;
}
