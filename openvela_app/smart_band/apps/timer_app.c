#include "smart_band_apps.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TIMER_DEFAULT_MS (5u * 60u * 1000u)
#define TIMER_STEP_MS (60u * 1000u)
#define TIMER_MAX_MS (99u * 60u * 1000u)
#define TIMER_ACTION_COUNT 4

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

typedef struct
{
  timer_state_t state;
  timer_view_t view;
  smart_band_app_monotonic_now_fn monotonic_now;
  void *clock_context;
  smart_band_app_event_binding_t bindings[TIMER_ACTION_COUNT];
} timer_context_t;

_Static_assert(sizeof(timer_context_t) <= SMART_BAND_APP_CONTEXT_CAPACITY,
               "timer context exceeds app storage");

static uint32_t timer_display_seconds(const timer_context_t *timer)
{
  return (timer->state.remaining_ms + 999u) / 1000u;
}

static void format_timer(const timer_context_t *timer, char *buffer,
                         size_t size)
{
  uint32_t seconds = timer_display_seconds(timer);

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

static bool timer_advance_to(timer_context_t *timer, uint32_t now_ms)
{
  uint32_t elapsed_ms;

  if (!timer->state.running)
    {
      return false;
    }

  elapsed_ms = now_ms - timer->state.last_now_ms;
  timer->state.last_now_ms = now_ms;
  if (elapsed_ms >= timer->state.remaining_ms)
    {
      timer->state.remaining_ms = 0;
      timer->state.running = false;
    }
  else
    {
      timer->state.remaining_ms -= elapsed_ms;
    }

  return elapsed_ms != 0;
}

static int timer_init(void *context)
{
  timer_context_t *timer = context;

  if (timer == NULL)
    {
      return -1;
    }

  memset(timer, 0, sizeof(*timer));
  timer->state.remaining_ms = TIMER_DEFAULT_MS;
  return 0;
}

static void timer_unmount(void *context)
{
  timer_context_t *timer = context;

  if (timer != NULL)
    {
      memset(&timer->view, 0, sizeof(timer->view));
    }
}

static void timer_render(void *context,
                         const smart_band_app_host_t *host)
{
  timer_context_t *timer = context;
  char value[16];

  (void)host;
  if (timer == NULL || !timer->view.mounted)
    {
      return;
    }

  format_timer(timer, value, sizeof(value));
  lv_label_set_text(timer->view.display, value);
  lv_label_set_text(timer->view.status,
                    timer->state.running ? "Running" :
                    (timer->state.remaining_ms == 0 ? "Done" : "Ready"));
  set_button_text(timer->view.start_button,
                  timer->state.running ? "Pause" : "Start");
}

static bool timer_tick(void *context, uint32_t now_ms)
{
  timer_context_t *timer = context;

  return timer != NULL && timer_advance_to(timer, now_ms);
}

static void timer_cb(lv_event_t *event)
{
  smart_band_app_event_binding_t *binding =
    lv_event_get_user_data(event);
  timer_context_t *timer;
  uint32_t now_ms;

  if (binding == NULL || binding->context == NULL)
    {
      return;
    }

  timer = binding->context;
  now_ms = timer->monotonic_now == NULL ? lv_tick_get() :
           timer->monotonic_now(timer->clock_context);
  if (binding->action == 1)
    {
      if (timer->state.running)
        {
          timer_render(timer, NULL);
          return;
        }

      if (timer->state.remaining_ms >= TIMER_STEP_MS)
        {
          timer->state.remaining_ms -= TIMER_STEP_MS;
        }
      else
        {
          timer->state.remaining_ms = 0;
        }
    }
  else if (binding->action == 2)
    {
      if (timer->state.running)
        {
          timer_render(timer, NULL);
          return;
        }

      if (timer->state.remaining_ms <= TIMER_MAX_MS - TIMER_STEP_MS)
        {
          timer->state.remaining_ms += TIMER_STEP_MS;
        }
      else
        {
          timer->state.remaining_ms = TIMER_MAX_MS;
        }
    }
  else if (binding->action == 3)
    {
      if (timer->state.running)
        {
          timer_advance_to(timer, now_ms);
          timer->state.running = false;
        }
      else
        {
          if (timer->state.remaining_ms == 0)
            {
              timer->state.remaining_ms = TIMER_DEFAULT_MS;
            }

          timer->state.last_now_ms = now_ms;
          timer->state.running = true;
        }
    }
  else
    {
      timer->state.remaining_ms = TIMER_DEFAULT_MS;
      timer->state.running = false;
    }

  timer_render(timer, NULL);
}

static int timer_mount(void *context, lv_obj_t *parent,
                       const smart_band_app_host_t *host)
{
  timer_context_t *timer = context;
  lv_obj_t *panel;
  size_t index;

  if (timer == NULL || parent == NULL || host == NULL)
    {
      return -1;
    }

  timer_unmount(timer);
  timer->monotonic_now = host->monotonic_now;
  timer->clock_context = host->clock_context;
  for (index = 0; index < TIMER_ACTION_COUNT; index++)
    {
      timer->bindings[index].context = timer;
      timer->bindings[index].action = index + 1;
    }

  panel = host->create_box(parent, host->sx(22), host->sy(14),
                           host->screen_w - host->sx(44), host->sy(180),
                           lv_color_hex(0xf2f5ff), host->sx(28));
  if (panel == NULL)
    {
      return -1;
    }

  timer->view.display =
    host->create_label(panel, "05:00", host->font_time(),
                       lv_color_hex(0x293b53), LV_TEXT_ALIGN_CENTER);
  timer->view.status =
    host->create_label(panel, "Ready", host->font_14(),
                       lv_color_hex(0x81939a), LV_TEXT_ALIGN_CENTER);
  if (timer->view.display == NULL || timer->view.status == NULL)
    {
      timer_unmount(timer);
      return -1;
    }

  host->place_label(timer->view.display, host->sx(12), host->sy(38),
                    host->screen_w - host->sx(68), host->sy(70));
  host->place_label(timer->view.status, host->sx(12), host->sy(116),
                    host->screen_w - host->sx(68), host->sy(26));

  if (host->create_action_button(
        parent, "-1m", host->sx(20), host->sy(220), host->sx(64),
        host->sy(54), lv_color_hex(0x8aa8d8), timer_cb,
        (uintptr_t)&timer->bindings[0]) == NULL ||
      host->create_action_button(
        parent, "+1m", host->sx(92), host->sy(220), host->sx(64),
        host->sy(54), lv_color_hex(0xa98bd6), timer_cb,
        (uintptr_t)&timer->bindings[1]) == NULL)
    {
      timer_unmount(timer);
      return -1;
    }

  timer->view.start_button =
    host->create_action_button(parent, "Start", host->sx(164), host->sy(220),
                               host->sx(64), host->sy(54),
                               lv_color_hex(0x80cbc3), timer_cb,
                               (uintptr_t)&timer->bindings[2]);
  if (timer->view.start_button == NULL ||
      host->create_action_button(
        parent, "Reset", host->sx(236), host->sy(220), host->sx(64),
        host->sy(54), lv_color_hex(0x6f8790), timer_cb,
        (uintptr_t)&timer->bindings[3]) == NULL)
    {
      timer_unmount(timer);
      return -1;
    }

  timer->view.mounted = true;
  timer_render(timer, host);
  return 0;
}

const smart_band_app_ops_t smart_band_timer_app_ops =
{
  .context_size = sizeof(timer_context_t),
  .init = timer_init,
  .mount = timer_mount,
  .unmount = timer_unmount,
  .tick = timer_tick,
  .render = timer_render
};
