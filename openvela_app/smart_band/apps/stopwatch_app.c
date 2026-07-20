#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define STOPWATCH_ACTION_COUNT 2

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

typedef struct
{
  stopwatch_state_t state;
  stopwatch_view_t view;
  smart_band_app_event_binding_t bindings[STOPWATCH_ACTION_COUNT];
} stopwatch_context_t;

_Static_assert(sizeof(stopwatch_context_t) <= SMART_BAND_APP_CONTEXT_CAPACITY,
               "stopwatch context exceeds app storage");

static void format_stopwatch(const stopwatch_context_t *stopwatch,
                             char *buffer, size_t size)
{
  uint64_t seconds = stopwatch->state.elapsed_ms / 1000u;

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

static bool stopwatch_advance_to(stopwatch_context_t *stopwatch,
                                 uint32_t now_ms)
{
  uint32_t elapsed_ms;

  if (!stopwatch->state.running)
    {
      return false;
    }

  elapsed_ms = now_ms - stopwatch->state.last_now_ms;
  stopwatch->state.last_now_ms = now_ms;
  stopwatch->state.elapsed_ms += elapsed_ms;
  return elapsed_ms != 0;
}

static int stopwatch_init(void *context)
{
  if (context == NULL)
    {
      return -1;
    }

  memset(context, 0, sizeof(stopwatch_context_t));
  return 0;
}

static void stopwatch_unmount(void *context)
{
  stopwatch_context_t *stopwatch = context;

  if (stopwatch != NULL)
    {
      memset(&stopwatch->view, 0, sizeof(stopwatch->view));
    }
}

static void stopwatch_render(void *context,
                             const smart_band_app_host_t *host)
{
  stopwatch_context_t *stopwatch = context;
  char value[32];

  (void)host;
  if (stopwatch == NULL || !stopwatch->view.mounted)
    {
      return;
    }

  format_stopwatch(stopwatch, value, sizeof(value));
  lv_label_set_text(stopwatch->view.display, value);
  lv_label_set_text(stopwatch->view.status,
                    stopwatch->state.running ? "Running" : "Paused");
  set_button_text(stopwatch->view.start_button,
                  stopwatch->state.running ? "Pause" : "Start");
}

static bool stopwatch_tick(void *context, uint32_t now_ms)
{
  stopwatch_context_t *stopwatch = context;

  return stopwatch != NULL && stopwatch_advance_to(stopwatch, now_ms);
}

static void stopwatch_cb(lv_event_t *event)
{
  smart_band_app_event_binding_t *binding =
    lv_event_get_user_data(event);
  stopwatch_context_t *stopwatch;
  uint32_t now_ms = lv_tick_get();

  if (binding == NULL || binding->context == NULL)
    {
      return;
    }

  stopwatch = binding->context;
  if (binding->action == 1)
    {
      if (stopwatch->state.running)
        {
          stopwatch_advance_to(stopwatch, now_ms);
          stopwatch->state.running = false;
        }
      else
        {
          stopwatch->state.last_now_ms = now_ms;
          stopwatch->state.running = true;
        }
    }
  else
    {
      stopwatch->state.elapsed_ms = 0;
      stopwatch->state.running = false;
    }

  stopwatch_render(stopwatch, NULL);
}

static int stopwatch_mount(void *context, lv_obj_t *parent,
                           const smart_band_app_host_t *host)
{
  stopwatch_context_t *stopwatch = context;
  lv_obj_t *panel;
  size_t index;

  if (stopwatch == NULL || parent == NULL || host == NULL)
    {
      return -1;
    }

  stopwatch_unmount(stopwatch);
  for (index = 0; index < STOPWATCH_ACTION_COUNT; index++)
    {
      stopwatch->bindings[index].context = stopwatch;
      stopwatch->bindings[index].action = index + 1;
    }

  panel = host->create_box(parent, host->sx(22), host->sy(18),
                           host->screen_w - host->sx(44), host->sy(174),
                           lv_color_hex(0xeef7ff), host->sx(28));
  if (panel == NULL)
    {
      return -1;
    }

  stopwatch->view.display =
    host->create_label(panel, "00:00", host->font_time(),
                       lv_color_hex(0x293b53), LV_TEXT_ALIGN_CENTER);
  stopwatch->view.status =
    host->create_label(panel, "Paused", host->font_14(),
                       lv_color_hex(0x6f8790), LV_TEXT_ALIGN_CENTER);
  if (stopwatch->view.display == NULL || stopwatch->view.status == NULL)
    {
      stopwatch_unmount(stopwatch);
      return -1;
    }

  host->place_label(stopwatch->view.display, host->sx(12), host->sy(40),
                    host->screen_w - host->sx(68), host->sy(66));
  host->place_label(stopwatch->view.status, host->sx(12), host->sy(116),
                    host->screen_w - host->sx(68), host->sy(24));

  stopwatch->view.start_button =
    host->create_action_button(parent, "Start", host->sx(54), host->sy(224),
                               host->sx(96), host->sy(54),
                               lv_color_hex(0x73a1d6), stopwatch_cb,
                               (uintptr_t)&stopwatch->bindings[0]);
  if (stopwatch->view.start_button == NULL ||
      host->create_action_button(
        parent, "Reset", host->sx(180), host->sy(224), host->sx(96),
        host->sy(54), lv_color_hex(0x6f8790), stopwatch_cb,
        (uintptr_t)&stopwatch->bindings[1]) == NULL)
    {
      stopwatch_unmount(stopwatch);
      return -1;
    }

  stopwatch->view.mounted = true;
  stopwatch_render(stopwatch, host);
  return 0;
}

const smart_band_app_ops_t smart_band_stopwatch_app_ops =
{
  .context_size = sizeof(stopwatch_context_t),
  .init = stopwatch_init,
  .mount = stopwatch_mount,
  .unmount = stopwatch_unmount,
  .tick = stopwatch_tick,
  .render = stopwatch_render
};
