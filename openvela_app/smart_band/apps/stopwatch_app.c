#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
  uint64_t elapsed_ms;
  uint32_t last_now_ms;
  bool running;
} stopwatch_state_t;

typedef struct
{
  lv_obj_t *display;
  lv_obj_t *status;
  lv_obj_t *start_button;
  bool mounted;
} stopwatch_view_t;

static stopwatch_state_t g_state;
static stopwatch_view_t g_view;

static void format_stopwatch(char *buffer, size_t size)
{
  uint64_t seconds = g_state.elapsed_ms / 1000u;

  snprintf(buffer, size, "%02llu:%02llu",
           (unsigned long long)(seconds / 60u),
           (unsigned long long)(seconds % 60u));
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

static void stopwatch_advance_to(uint32_t now_ms)
{
  uint32_t elapsed_ms;

  if (!g_state.running)
    {
      return;
    }

  elapsed_ms = now_ms - g_state.last_now_ms;
  g_state.last_now_ms = now_ms;
  g_state.elapsed_ms += elapsed_ms;
}

void smart_band_stopwatch_app_unmount(void)
{
  memset(&g_view, 0, sizeof(g_view));
}

void smart_band_stopwatch_app_update(const smart_band_app_host_t *host)
{
  char value[16];

  (void)host;
  if (!g_view.mounted)
    {
      return;
    }

  format_stopwatch(value, sizeof(value));
  lv_label_set_text(g_view.display, value);
  lv_label_set_text(g_view.status, g_state.running ? "Running" : "Paused");
  set_button_text(g_view.start_button,
                  g_state.running ? "Pause" : "Start");
}

void smart_band_stopwatch_app_tick_at(const smart_band_app_host_t *host,
                                      uint32_t now_ms)
{
  if (!g_state.running)
    {
      return;
    }

  stopwatch_advance_to(now_ms);
  if (g_view.mounted)
    {
      smart_band_stopwatch_app_update(host);
    }
}

void smart_band_stopwatch_app_tick(const smart_band_app_host_t *host)
{
  smart_band_stopwatch_app_tick_at(host, lv_tick_get());
}

static void stopwatch_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);
  uint32_t now_ms = lv_tick_get();

  if (action == 1)
    {
      if (g_state.running)
        {
          stopwatch_advance_to(now_ms);
          g_state.running = false;
        }
      else
        {
          g_state.last_now_ms = now_ms;
          g_state.running = true;
        }
    }
  else
    {
      g_state.elapsed_ms = 0;
      g_state.running = false;
    }

  smart_band_stopwatch_app_update(NULL);
}

int smart_band_stopwatch_app_build(lv_obj_t *parent,
                                   const smart_band_app_host_t *host)
{
  lv_obj_t *panel;

  smart_band_stopwatch_app_unmount();

  panel = host->create_box(parent, host->sx(22), host->sy(18),
                           host->screen_w - host->sx(44), host->sy(174),
                           lv_color_hex(0xeef7ff), host->sx(28));
  if (panel == NULL)
    {
      return -1;
    }

  g_view.display = host->create_label(panel, "00:00", host->font_time(),
                                      lv_color_hex(0x293b53),
                                      LV_TEXT_ALIGN_CENTER);
  g_view.status = host->create_label(panel, "Paused", host->font_14(),
                                     lv_color_hex(0x6f8790),
                                     LV_TEXT_ALIGN_CENTER);
  if (g_view.display == NULL || g_view.status == NULL)
    {
      smart_band_stopwatch_app_unmount();
      return -1;
    }

  host->place_label(g_view.display, host->sx(12), host->sy(40),
                    host->screen_w - host->sx(68), host->sy(66));
  host->place_label(g_view.status, host->sx(12), host->sy(116),
                    host->screen_w - host->sx(68), host->sy(24));

  g_view.start_button =
    host->create_action_button(parent, "Start", host->sx(54), host->sy(224),
                               host->sx(96), host->sy(54),
                               lv_color_hex(0x73a1d6), stopwatch_cb, 1);
  if (g_view.start_button == NULL ||
      host->create_action_button(parent, "Reset", host->sx(180),
                                 host->sy(224), host->sx(96), host->sy(54),
                                 lv_color_hex(0x6f8790), stopwatch_cb,
                                 2) == NULL)
    {
      smart_band_stopwatch_app_unmount();
      return -1;
    }

  g_view.mounted = true;
  smart_band_stopwatch_app_update(host);
  return 0;
}
