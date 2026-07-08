#include "smart_band_apps.h"

#include <stdbool.h>

static lv_obj_t *g_panel;
static lv_obj_t *g_status;
static bool g_on;

void smart_band_flashlight_app_update(const smart_band_app_host_t *host)
{
  (void)host;
  if (g_panel != NULL)
    {
      lv_obj_set_style_bg_color(g_panel,
                                g_on ? lv_color_hex(0xffffff) :
                                lv_color_hex(0x263943), 0);
    }

  if (g_status != NULL)
    {
      lv_label_set_text(g_status, g_on ? "On" : "Off");
    }
}

static void flashlight_cb(lv_event_t *event)
{
  (void)event;

  g_on = !g_on;
  smart_band_flashlight_app_update(NULL);
}

int smart_band_flashlight_app_build(lv_obj_t *parent,
                                    const smart_band_app_host_t *host)
{
  lv_obj_t *caption;

  g_panel = host->create_box(parent, host->sx(38), host->sy(20),
                             host->screen_w - host->sx(76), host->sy(220),
                             lv_color_hex(0x263943), host->sx(34));
  g_status = host->create_label(parent, "Off", host->font_32(),
                                lv_color_hex(0x293b53),
                                LV_TEXT_ALIGN_CENTER);
  caption = host->create_label(parent, "Tap to toggle bright screen",
                               host->font_12(), lv_color_hex(0x81939a),
                               LV_TEXT_ALIGN_CENTER);
  if (g_panel == NULL || g_status == NULL || caption == NULL)
    {
      return -1;
    }

  host->place_label(g_status, host->sx(20), host->sy(262),
                    host->screen_w - host->sx(40), host->sy(44));
  host->place_label(caption, host->sx(20), host->sy(308),
                    host->screen_w - host->sx(40), host->sy(20));

  if (host->create_action_button(parent, "Toggle", host->sx(106),
                                 host->sy(342), host->sx(118),
                                 host->sy(54), lv_color_hex(0xf5c66e),
                                 flashlight_cb, 1) == NULL)
    {
      return -1;
    }

  smart_band_flashlight_app_update(host);
  return 0;
}
