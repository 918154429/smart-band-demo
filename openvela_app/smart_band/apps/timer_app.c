#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TIMER_DEFAULT_MS (5u * 60u * 1000u)
#define TIMER_STEP_MS (60u * 1000u)
#define TIMER_MAX_MS (99u * 60u * 1000u)

typedef struct
{
  uint32_t remaining_ms;
  uint32_t last_now_ms;
  bool running;
} timer_state_t;

typedef struct
{
  lv_obj_t *display;
  lv_obj_t *status;
  lv_obj_t *start_button;
  bool mounted;
} timer_view_t;

static timer_state_t g_state =
{
  .remaining_ms = TIMER_DEFAULT_MS
};
static timer_view_t g_view;

static uint32_t timer_display_seconds(void)
{
  return (g_state.remaining_ms + 999u) / 1000u;
}

static void format_timer(char *buffer, size_t size)
{
  uint32_t seconds = timer_display_seconds();

  snprintf(buffer, size, "%02lu:%02lu",
           (unsigned long)(seconds / 60u),
           (unsigned long)(seconds % 60u));
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

static void timer_advance_to(uint32_t now_ms)
{
  uint32_t elapsed_ms;

  if (!g_state.running)
    {
      return;
    }

  elapsed_ms = now_ms - g_state.last_now_ms;
  g_state.last_now_ms = now_ms;
  if (elapsed_ms >= g_state.remaining_ms)
    {
      g_state.remaining_ms = 0;
      g_state.running = false;
    }
  else
    {
      g_state.remaining_ms -= elapsed_ms;
    }
}

void smart_band_timer_app_unmount(void)
{
  memset(&g_view, 0, sizeof(g_view));
}

void smart_band_timer_app_update(const smart_band_app_host_t *host)
{
  char value[16];

  (void)host;
  if (!g_view.mounted)
    {
      return;
    }

  format_timer(value, sizeof(value));
  lv_label_set_text(g_view.display, value);
  lv_label_set_text(g_view.status,
                    g_state.running ? "Running" :
                    (g_state.remaining_ms == 0 ? "Done" : "Ready"));
  set_button_text(g_view.start_button,
                  g_state.running ? "Pause" : "Start");
}

void smart_band_timer_app_tick_at(const smart_band_app_host_t *host,
                                  uint32_t now_ms)
{
  if (!g_state.running)
    {
      return;
    }

  timer_advance_to(now_ms);
  if (g_view.mounted)
    {
      smart_band_timer_app_update(host);
    }
}

void smart_band_timer_app_tick(const smart_band_app_host_t *host)
{
  smart_band_timer_app_tick_at(host, lv_tick_get());
}

static void timer_cb(lv_event_t *event)
{
  uintptr_t action = (uintptr_t)lv_event_get_user_data(event);
  uint32_t now_ms = lv_tick_get();

  if (action == 1)
    {
      if (g_state.running)
        {
          smart_band_timer_app_update(NULL);
          return;
        }

      if (g_state.remaining_ms >= TIMER_STEP_MS)
        {
          g_state.remaining_ms -= TIMER_STEP_MS;
        }
      else
        {
          g_state.remaining_ms = 0;
        }
    }
  else if (action == 2)
    {
      if (g_state.running)
        {
          smart_band_timer_app_update(NULL);
          return;
        }

      if (g_state.remaining_ms <= TIMER_MAX_MS - TIMER_STEP_MS)
        {
          g_state.remaining_ms += TIMER_STEP_MS;
        }
      else
        {
          g_state.remaining_ms = TIMER_MAX_MS;
        }
    }
  else if (action == 3)
    {
      if (g_state.running)
        {
          timer_advance_to(now_ms);
          g_state.running = false;
        }
      else
        {
          if (g_state.remaining_ms == 0)
            {
              g_state.remaining_ms = TIMER_DEFAULT_MS;
            }

          g_state.last_now_ms = now_ms;
          g_state.running = true;
        }
    }
  else
    {
      g_state.remaining_ms = TIMER_DEFAULT_MS;
      g_state.running = false;
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

  g_view.display = host->create_label(panel, "05:00", host->font_time(),
                                      lv_color_hex(0x293b53),
                                      LV_TEXT_ALIGN_CENTER);
  g_view.status = host->create_label(panel, "Ready", host->font_14(),
                                     lv_color_hex(0x81939a),
                                     LV_TEXT_ALIGN_CENTER);
  if (g_view.display == NULL || g_view.status == NULL)
    {
      smart_band_timer_app_unmount();
      return -1;
    }

  host->place_label(g_view.display, host->sx(12), host->sy(38),
                    host->screen_w - host->sx(68), host->sy(70));
  host->place_label(g_view.status, host->sx(12), host->sy(116),
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
      smart_band_timer_app_unmount();
      return -1;
    }

  g_view.start_button =
    host->create_action_button(parent, "Start", host->sx(164), host->sy(220),
                               host->sx(64), host->sy(54),
                               lv_color_hex(0x80cbc3), timer_cb, 3);
  if (g_view.start_button == NULL ||
      host->create_action_button(parent, "Reset", host->sx(236),
                                 host->sy(220), host->sx(64), host->sy(54),
                                 lv_color_hex(0x6f8790), timer_cb,
                                 4) == NULL)
    {
      smart_band_timer_app_unmount();
      return -1;
    }

  g_view.mounted = true;
  smart_band_timer_app_update(host);
  return 0;
}
