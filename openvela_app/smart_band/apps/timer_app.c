#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdio.h>

static lv_obj_t *g_display;
static lv_obj_t *g_status;
static lv_obj_t *g_start_button;
static int g_seconds = 5 * 60;
static bool g_running;

void smart_band_timer_app_unmount(void)
{
  g_display = NULL;
  g_status = NULL;
  g_start_button = NULL;
}

static void format_timer(char *buffer, size_t size)
{
  if (g_seconds < 0)
    {
      g_seconds = 0;
    }

  snprintf(buffer, size, "%02d:%02d", g_seconds / 60, g_seconds % 60);
}

static void set_button_text(lv_obj_t *button, const char *text)
{
  lv_obj_t *label;

  if (button == NULL)
    {
      return;
    }

  label = lv_obj_get_child(button, 0);
  if (label != NULL)
    {
      lv_label_set_text(label, text);
    }
}

void smart_band_timer_app_update(const smart_band_app_host_t *host)
{
  char value[16];

  (void)host;
  format_timer(value, sizeof(value));
  if (g_display != NULL)
    {
      lv_label_set_text(g_display, value);
    }

  if (g_status != NULL)
    {
      lv_label_set_text(g_status,
                        g_running ? "Running" :
                        (g_seconds == 0 ? "Done" : "Ready"));
    }

  set_button_text(g_start_button, g_running ? "Pause" : "Start");
}

void smart_band_timer_app_tick(const smart_band_app_host_t *host)
{
  if (!g_running)
    {
      return;
    }

  if (g_seconds > 0)
    {
      g_seconds--;
    }

  if (g_seconds <= 0)
    {
      g_seconds = 0;
      g_running = false;
    }

  smart_band_timer_app_update(host);
}

static void timer_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);

  if (action == 1)
    {
      if (g_running)
        {
          smart_band_timer_app_update(NULL);
          return;
        }

      if (g_seconds >= 60)
        {
          g_seconds -= 60;
        }
      else
        {
          g_seconds = 0;
        }
    }
  else if (action == 2)
    {
      if (g_running)
        {
          smart_band_timer_app_update(NULL);
          return;
        }

      g_seconds += 60;
      if (g_seconds > 99 * 60)
        {
          g_seconds = 99 * 60;
        }
    }
  else if (action == 3)
    {
      if (g_seconds <= 0)
        {
          g_seconds = 5 * 60;
        }

      g_running = !g_running;
    }
  else
    {
      g_seconds = 5 * 60;
      g_running = false;
    }

  smart_band_timer_app_update(NULL);
}

int smart_band_timer_app_build(lv_obj_t *parent,
                               const smart_band_app_host_t *host)
{
  lv_obj_t *panel;

  smart_band_timer_app_unmount();

  panel = host->create_box(parent, host->sx(22), host->sy(14),
                           host->screen_w - host->sx(44), host->sy(180),
                           lv_color_hex(0xf2f5ff), host->sx(28));
  if (panel == NULL)
    {
      return -1;
    }

  g_display = host->create_label(panel, "05:00", host->font_time(),
                                 lv_color_hex(0x293b53),
                                 LV_TEXT_ALIGN_CENTER);
  g_status = host->create_label(panel, "Ready", host->font_14(),
                                lv_color_hex(0x81939a),
                                LV_TEXT_ALIGN_CENTER);
  if (g_display == NULL || g_status == NULL)
    {
      return -1;
    }

  host->place_label(g_display, host->sx(12), host->sy(38),
                    host->screen_w - host->sx(68), host->sy(70));
  host->place_label(g_status, host->sx(12), host->sy(116),
                    host->screen_w - host->sx(68), host->sy(26));

  if (host->create_action_button(parent, "-1m", host->sx(20), host->sy(220),
                                 host->sx(64), host->sy(54),
                                 lv_color_hex(0x8aa8d8), timer_cb,
                                 1) == NULL ||
      host->create_action_button(parent, "+1m", host->sx(92), host->sy(220),
                                 host->sx(64), host->sy(54),
                                 lv_color_hex(0xa98bd6), timer_cb,
                                 2) == NULL)
    {
      return -1;
    }

  g_start_button = host->create_action_button(parent, "Start",
                                              host->sx(164), host->sy(220),
                                              host->sx(64), host->sy(54),
                                              lv_color_hex(0x80cbc3),
                                              timer_cb, 3);
  if (g_start_button == NULL ||
      host->create_action_button(parent, "Reset", host->sx(236),
                                 host->sy(220), host->sx(64), host->sy(54),
                                 lv_color_hex(0x6f8790), timer_cb,
                                 4) == NULL)
    {
      return -1;
    }

  smart_band_timer_app_update(host);
  return 0;
}
