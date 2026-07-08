#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdio.h>

static lv_obj_t *g_title;
static lv_obj_t *g_status;
static lv_obj_t *g_progress;
static int g_track;
static int g_volume = 60;
static bool g_playing;

void smart_band_music_app_update(const smart_band_app_host_t *host)
{
  static const char *const tracks[] =
  {
    "Morning Walk", "Lo-Fi Set", "Night Run"
  };

  char status[32];
  int progress = 0;

  if (host != NULL && host->model != NULL)
    {
      progress = (int)((host->model->ticks % 180u) * 100u / 180u);
    }

  if (g_track < 0 || g_track >= 3)
    {
      g_track = 0;
    }

  snprintf(status, sizeof(status), "%s  Vol %d%%",
           g_playing ? "Playing" : "Paused", g_volume);
  if (g_title != NULL)
    {
      lv_label_set_text(g_title, tracks[g_track]);
    }

  if (g_status != NULL)
    {
      lv_label_set_text(g_status, status);
    }

  if (g_progress != NULL)
    {
      lv_bar_set_value(g_progress, g_playing ? progress : 0, LV_ANIM_ON);
    }
}

static void music_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);

  if (action == 1)
    {
      g_track = (g_track + 2) % 3;
    }
  else if (action == 2)
    {
      g_playing = !g_playing;
    }
  else if (action == 3)
    {
      g_track = (g_track + 1) % 3;
    }
  else
    {
      g_volume += 10;
      if (g_volume > 100)
        {
          g_volume = 40;
        }
    }

  smart_band_music_app_update(NULL);
}

int smart_band_music_app_build(lv_obj_t *parent,
                               const smart_band_app_host_t *host)
{
  lv_obj_t *panel;

  g_title = NULL;
  g_status = NULL;
  g_progress = NULL;

  panel = host->create_box(parent, host->sx(22), host->sy(14),
                           host->screen_w - host->sx(44), host->sy(170),
                           lv_color_hex(0xfff0eb), host->sx(28));
  if (panel == NULL)
    {
      return -1;
    }

  g_title = host->create_label(panel, "--", host->font_20(),
                               lv_color_hex(0x293b53),
                               LV_TEXT_ALIGN_CENTER);
  g_status = host->create_label(panel, "--", host->font_14(),
                                lv_color_hex(0x81939a),
                                LV_TEXT_ALIGN_CENTER);
  g_progress = lv_bar_create(panel);
  if (g_title == NULL || g_status == NULL || g_progress == NULL)
    {
      return -1;
    }

  lv_obj_remove_style_all(g_progress);
  host->place_label(g_title, host->sx(14), host->sy(38),
                    host->screen_w - host->sx(72), host->sy(32));
  host->place_label(g_status, host->sx(14), host->sy(74),
                    host->screen_w - host->sx(72), host->sy(24));
  lv_obj_set_pos(g_progress, host->sx(34), host->sy(118));
  lv_obj_set_size(g_progress, host->screen_w - host->sx(112), host->sy(8));
  lv_obj_set_style_radius(g_progress, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(g_progress, lv_color_hex(0xf9d9d4), 0);
  lv_obj_set_style_bg_opa(g_progress, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(g_progress, lv_color_hex(0xf08d88),
                            LV_PART_INDICATOR);
  lv_obj_set_style_radius(g_progress, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_bar_set_range(g_progress, 0, 100);

  if (host->create_action_button(parent, "Prev", host->sx(22),
                                 host->sy(212), host->sx(64), host->sy(54),
                                 lv_color_hex(0x6f8790), music_cb,
                                 1) == NULL ||
      host->create_action_button(parent, "Play", host->sx(96),
                                 host->sy(212), host->sx(74), host->sy(54),
                                 lv_color_hex(0xf08d88), music_cb,
                                 2) == NULL ||
      host->create_action_button(parent, "Next", host->sx(180),
                                 host->sy(212), host->sx(64), host->sy(54),
                                 lv_color_hex(0x6f8790), music_cb,
                                 3) == NULL ||
      host->create_action_button(parent, "Vol", host->sx(254),
                                 host->sy(212), host->sx(54), host->sy(54),
                                 lv_color_hex(0x80cbc3), music_cb,
                                 4) == NULL)
    {
      return -1;
    }

  smart_band_music_app_update(host);
  return 0;
}
