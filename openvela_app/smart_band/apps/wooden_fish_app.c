#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdio.h>

static lv_obj_t *g_count;
static lv_obj_t *g_mode;
static int g_merit;
static bool g_auto;

void smart_band_wooden_fish_app_update(const smart_band_app_host_t *host)
{
  char value[32];

  (void)host;
  snprintf(value, sizeof(value), "Merit %d", g_merit);
  if (g_count != NULL)
    {
      lv_label_set_text(g_count, value);
    }

  if (g_mode != NULL)
    {
      lv_label_set_text(g_mode, g_auto ? "Auto on" : "Tap mode");
    }
}

void smart_band_wooden_fish_app_tick(const smart_band_app_host_t *host)
{
  if (!g_auto)
    {
      return;
    }

  g_merit++;
  smart_band_wooden_fish_app_update(host);
}

static void fish_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);

  if (action == 1)
    {
      g_merit++;
    }
  else if (action == 2)
    {
      g_auto = !g_auto;
    }
  else
    {
      g_merit = 0;
      g_auto = false;
    }

  smart_band_wooden_fish_app_update(NULL);
}

int smart_band_wooden_fish_app_build(lv_obj_t *parent,
                                     const smart_band_app_host_t *host)
{
  lv_obj_t *fish;
  lv_obj_t *label;

  g_count = NULL;
  g_mode = NULL;

  fish = host->create_action_button(parent, "KNOCK", host->sx(68),
                                    host->sy(38), host->sx(194),
                                    host->sy(150), lv_color_hex(0xd9a85f),
                                    fish_cb, 1);
  g_count = host->create_label(parent, "Merit 0", host->font_32(),
                               lv_color_hex(0x293b53),
                               LV_TEXT_ALIGN_CENTER);
  g_mode = host->create_label(parent, "Tap mode", host->font_16(),
                              lv_color_hex(0x6f8790),
                              LV_TEXT_ALIGN_CENTER);
  label = host->create_label(parent, "Each tap updates local state",
                             host->font_12(), lv_color_hex(0x81939a),
                             LV_TEXT_ALIGN_CENTER);
  if (fish == NULL || g_count == NULL || g_mode == NULL || label == NULL)
    {
      return -1;
    }

  lv_obj_set_style_radius(fish, LV_RADIUS_CIRCLE, 0);
  host->place_label(g_count, host->sx(20), host->sy(210),
                    host->screen_w - host->sx(40), host->sy(44));
  host->place_label(g_mode, host->sx(20), host->sy(258),
                    host->screen_w - host->sx(40), host->sy(26));
  host->place_label(label, host->sx(20), host->sy(288),
                    host->screen_w - host->sx(40), host->sy(20));

  if (host->create_action_button(parent, "Auto", host->sx(54),
                                 host->sy(326), host->sx(96), host->sy(50),
                                 lv_color_hex(0x80cbc3), fish_cb,
                                 2) == NULL ||
      host->create_action_button(parent, "Reset", host->sx(180),
                                 host->sy(326), host->sx(96), host->sy(50),
                                 lv_color_hex(0x6f8790), fish_cb,
                                 3) == NULL)
    {
      return -1;
    }

  smart_band_wooden_fish_app_update(host);
  return 0;
}
