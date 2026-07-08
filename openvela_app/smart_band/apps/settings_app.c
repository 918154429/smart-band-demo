#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdio.h>

static lv_obj_t *g_brightness;
static lv_obj_t *g_dnd;
static int g_brightness_percent = 70;
static bool g_dnd_enabled;

void smart_band_settings_app_update(const smart_band_app_host_t *host)
{
  char value[16];

  (void)host;
  snprintf(value, sizeof(value), "%d%%", g_brightness_percent);
  if (g_brightness != NULL)
    {
      lv_label_set_text(g_brightness, value);
    }

  if (g_dnd != NULL)
    {
      lv_label_set_text(g_dnd, g_dnd_enabled ? "On" : "Off");
    }
}

static void settings_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);

  if (action == 1)
    {
      g_brightness_percent -= 10;
      if (g_brightness_percent < 10)
        {
          g_brightness_percent = 10;
        }
    }
  else if (action == 2)
    {
      g_brightness_percent += 10;
      if (g_brightness_percent > 100)
        {
          g_brightness_percent = 100;
        }
    }
  else
    {
      g_dnd_enabled = !g_dnd_enabled;
    }

  smart_band_settings_app_update(NULL);
}

int smart_band_settings_app_build(lv_obj_t *parent,
                                  const smart_band_app_host_t *host)
{
  lv_obj_t *bright_label;
  lv_obj_t *dnd_label;

  g_brightness = NULL;
  g_dnd = NULL;

  bright_label = host->create_label(parent, "Brightness", host->font_16(),
                                    lv_color_hex(0x6f8790),
                                    LV_TEXT_ALIGN_LEFT);
  g_brightness = host->create_label(parent, "--", host->font_32(),
                                    lv_color_hex(0x293b53),
                                    LV_TEXT_ALIGN_LEFT);
  dnd_label = host->create_label(parent, "Do Not Disturb", host->font_16(),
                                 lv_color_hex(0x6f8790),
                                 LV_TEXT_ALIGN_LEFT);
  g_dnd = host->create_label(parent, "--", host->font_32(),
                             lv_color_hex(0x293b53), LV_TEXT_ALIGN_LEFT);
  if (bright_label == NULL || g_brightness == NULL ||
      dnd_label == NULL || g_dnd == NULL)
    {
      return -1;
    }

  host->place_label(bright_label, host->sx(28), host->sy(24),
                    host->sx(160), host->sy(24));
  host->place_label(g_brightness, host->sx(28), host->sy(52),
                    host->sx(120), host->sy(44));
  host->place_label(dnd_label, host->sx(28), host->sy(144),
                    host->sx(180), host->sy(24));
  host->place_label(g_dnd, host->sx(28), host->sy(172),
                    host->sx(120), host->sy(44));

  if (host->create_action_button(parent, "-", host->sx(180), host->sy(50),
                                 host->sx(52), host->sy(52),
                                 lv_color_hex(0x6f8790), settings_cb,
                                 1) == NULL ||
      host->create_action_button(parent, "+", host->sx(246), host->sy(50),
                                 host->sx(52), host->sy(52),
                                 lv_color_hex(0x80cbc3), settings_cb,
                                 2) == NULL ||
      host->create_action_button(parent, "DND", host->sx(180),
                                 host->sy(168), host->sx(118),
                                 host->sy(54), lv_color_hex(0xa98bd6),
                                 settings_cb, 3) == NULL)
    {
      return -1;
    }

  smart_band_settings_app_update(host);
  return 0;
}
