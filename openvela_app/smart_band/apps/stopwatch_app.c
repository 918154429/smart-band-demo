#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdio.h>

static lv_obj_t *g_display;
static lv_obj_t *g_status;
static int g_seconds;
static bool g_running;

static void format_stopwatch(char *buffer, size_t size)
{
  snprintf(buffer, size, "%02d:%02d", g_seconds / 60, g_seconds % 60);
}

void smart_band_stopwatch_app_update(const smart_band_app_host_t *host)
{
  char value[16];

  (void)host;
  format_stopwatch(value, sizeof(value));
  if (g_display != NULL)
    {
      lv_label_set_text(g_display, value);
    }

  if (g_status != NULL)
    {
      lv_label_set_text(g_status, g_running ? "Running" : "Paused");
    }
}

void smart_band_stopwatch_app_tick(const smart_band_app_host_t *host)
{
  if (!g_running)
    {
      return;
    }

  g_seconds++;
  smart_band_stopwatch_app_update(host);
}

static void stopwatch_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);

  if (action == 1)
    {
      g_running = !g_running;
    }
  else
    {
      g_seconds = 0;
      g_running = false;
    }

  smart_band_stopwatch_app_update(NULL);
}

int smart_band_stopwatch_app_build(lv_obj_t *parent,
                                   const smart_band_app_host_t *host)
{
  lv_obj_t *panel;

  g_display = NULL;
  g_status = NULL;

  panel = host->create_box(parent, host->sx(22), host->sy(18),
                           host->screen_w - host->sx(44), host->sy(174),
                           lv_color_hex(0xeef7ff), host->sx(28));
  if (panel == NULL)
    {
      return -1;
    }

  g_display = host->create_label(panel, "00:00", host->font_time(),
                                 lv_color_hex(0x293b53),
                                 LV_TEXT_ALIGN_CENTER);
  g_status = host->create_label(panel, "Paused", host->font_14(),
                                lv_color_hex(0x6f8790),
                                LV_TEXT_ALIGN_CENTER);
  if (g_display == NULL || g_status == NULL)
    {
      return -1;
    }

  host->place_label(g_display, host->sx(12), host->sy(40),
                    host->screen_w - host->sx(68), host->sy(66));
  host->place_label(g_status, host->sx(12), host->sy(116),
                    host->screen_w - host->sx(68), host->sy(24));

  if (host->create_action_button(parent, "Start", host->sx(54),
                                 host->sy(224), host->sx(96), host->sy(54),
                                 lv_color_hex(0x73a1d6), stopwatch_cb,
                                 1) == NULL ||
      host->create_action_button(parent, "Reset", host->sx(180),
                                 host->sy(224), host->sx(96), host->sy(54),
                                 lv_color_hex(0x6f8790), stopwatch_cb,
                                 2) == NULL)
    {
      return -1;
    }

  smart_band_stopwatch_app_update(host);
  return 0;
}
