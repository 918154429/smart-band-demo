#include "smart_band_apps.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
  do                                                                         \
    {                                                                        \
      if (!(condition))                                                      \
        {                                                                    \
          fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
                  #condition);                                               \
          return 1;                                                          \
        }                                                                    \
    }                                                                        \
  while (0)

#define TEST_OBJECT_CAPACITY 64
#define TEST_TEXT_CAPACITY 32

struct lv_font_s
{
  int unused;
};

struct lv_event_s
{
  uintptr_t user_data;
};

struct lv_obj_s
{
  char text[TEST_TEXT_CAPACITY];
  struct lv_obj_s *child;
  lv_event_cb_t callback;
  uintptr_t user_data;
};

static struct lv_obj_s g_objects[TEST_OBJECT_CAPACITY];
static size_t g_object_count;
static unsigned int g_lvgl_writes;
static uint32_t g_now_ms;
static struct lv_font_s g_font;

int smart_band_timer_app_build(lv_obj_t *parent,
                               const smart_band_app_host_t *host);
void smart_band_timer_app_tick_at(const smart_band_app_host_t *host,
                                  uint32_t now_ms);
void smart_band_timer_app_unmount(void);
int smart_band_stopwatch_app_build(lv_obj_t *parent,
                                   const smart_band_app_host_t *host);
void smart_band_stopwatch_app_tick_at(const smart_band_app_host_t *host,
                                      uint32_t now_ms);
void smart_band_stopwatch_app_unmount(void);

static lv_obj_t *new_object(void)
{
  lv_obj_t *object;

  if (g_object_count >= TEST_OBJECT_CAPACITY)
    {
      return NULL;
    }

  object = &g_objects[g_object_count];
  g_object_count++;
  return object;
}

void *lv_event_get_user_data(lv_event_t *event)
{
  return (void *)event->user_data;
}

lv_obj_t *lv_obj_get_child(lv_obj_t *parent, int index)
{
  return index == 0 ? parent->child : NULL;
}

void lv_label_set_text(lv_obj_t *label, const char *text)
{
  (void)snprintf(label->text, sizeof(label->text), "%s", text);
  g_lvgl_writes++;
}

uint32_t lv_tick_get(void)
{
  return g_now_ms;
}

static lv_coord_t scale(int value)
{
  return value;
}

static const lv_font_t *font(void)
{
  return &g_font;
}

static lv_obj_t *create_box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                            lv_coord_t w, lv_coord_t h, lv_color_t color,
                            lv_coord_t radius)
{
  (void)parent;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)color;
  (void)radius;
  return new_object();
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *selected_font,
                              lv_color_t color, lv_text_align_t align)
{
  lv_obj_t *label = new_object();

  (void)parent;
  (void)selected_font;
  (void)color;
  (void)align;
  if (label != NULL)
    {
      lv_label_set_text(label, text);
    }

  return label;
}

static lv_obj_t *create_action_button(lv_obj_t *parent, const char *text,
                                      lv_coord_t x, lv_coord_t y,
                                      lv_coord_t w, lv_coord_t h,
                                      lv_color_t color, lv_event_cb_t cb,
                                      uintptr_t data)
{
  lv_obj_t *button = new_object();
  lv_obj_t *label = new_object();

  (void)parent;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)color;
  if (button == NULL || label == NULL)
    {
      return NULL;
    }

  button->child = label;
  button->callback = cb;
  button->user_data = data;
  lv_label_set_text(label, text);
  return button;
}

static void place_label(lv_obj_t *label, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h)
{
  (void)label;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
}

static void reset_objects(void)
{
  memset(g_objects, 0, sizeof(g_objects));
  g_object_count = 0;
}

static lv_obj_t *find_text(const char *text)
{
  size_t index;

  for (index = 0; index < g_object_count; index++)
    {
      if (strcmp(g_objects[index].text, text) == 0)
        {
          return &g_objects[index];
        }
    }

  return NULL;
}

static lv_obj_t *find_button(const char *label_text)
{
  size_t index;

  for (index = 0; index < g_object_count; index++)
    {
      lv_obj_t *child = g_objects[index].child;

      if (child != NULL && strcmp(child->text, label_text) == 0)
        {
          return &g_objects[index];
        }
    }

  return NULL;
}

static int click(lv_obj_t *button)
{
  lv_event_t event;

  CHECK(button != NULL);
  CHECK(button->callback != NULL);
  event.user_data = button->user_data;
  button->callback(&event);
  return 0;
}

static smart_band_app_host_t make_host(void)
{
  smart_band_app_host_t host;

  memset(&host, 0, sizeof(host));
  host.screen_w = 330;
  host.screen_h = 626;
  host.sx = scale;
  host.sy = scale;
  host.font_14 = font;
  host.font_time = font;
  host.create_box = create_box;
  host.create_label = create_label;
  host.create_action_button = create_action_button;
  host.place_label = place_label;
  return host;
}

static int test_timer_elapsed_and_unmount(void)
{
  smart_band_app_host_t host = make_host();
  lv_obj_t parent;
  lv_obj_t *display;
  unsigned int writes_before_tick;

  memset(&parent, 0, sizeof(parent));
  reset_objects();
  g_now_ms = 1000;

  CHECK(smart_band_timer_app_build(&parent, &host) == 0);
  display = find_text("05:00");
  CHECK(display != NULL);
  CHECK(click(find_button("Start")) == 0);

  smart_band_timer_app_tick_at(&host, 6500);
  CHECK(strcmp(display->text, "04:55") == 0);

  smart_band_timer_app_unmount();
  writes_before_tick = g_lvgl_writes;
  smart_band_timer_app_tick_at(&host, 11500);
  CHECK(g_lvgl_writes == writes_before_tick);

  reset_objects();
  CHECK(smart_band_timer_app_build(&parent, &host) == 0);
  CHECK(find_text("04:50") != NULL);
  g_now_ms = 11500;
  CHECK(click(find_button("Pause")) == 0);
  CHECK(click(find_button("Reset")) == 0);
  smart_band_timer_app_unmount();
  return 0;
}

static int test_stopwatch_elapsed_and_unmount(void)
{
  smart_band_app_host_t host = make_host();
  lv_obj_t parent;
  lv_obj_t *display;
  unsigned int writes_before_tick;

  memset(&parent, 0, sizeof(parent));
  reset_objects();
  g_now_ms = 10000;

  CHECK(smart_band_stopwatch_app_build(&parent, &host) == 0);
  display = find_text("00:00");
  CHECK(display != NULL);
  CHECK(click(find_button("Start")) == 0);

  smart_band_stopwatch_app_tick_at(&host, 15750);
  CHECK(strcmp(display->text, "00:05") == 0);

  smart_band_stopwatch_app_unmount();
  writes_before_tick = g_lvgl_writes;
  smart_band_stopwatch_app_tick_at(&host, 20250);
  CHECK(g_lvgl_writes == writes_before_tick);

  reset_objects();
  CHECK(smart_band_stopwatch_app_build(&parent, &host) == 0);
  CHECK(find_text("00:10") != NULL);
  g_now_ms = 20250;
  CHECK(click(find_button("Pause")) == 0);
  CHECK(click(find_button("Reset")) == 0);
  smart_band_stopwatch_app_unmount();
  return 0;
}

static int test_tick_wraparound(void)
{
  smart_band_app_host_t host = make_host();
  lv_obj_t parent;

  memset(&parent, 0, sizeof(parent));
  reset_objects();
  g_now_ms = UINT32_MAX - 1999u;
  CHECK(smart_band_timer_app_build(&parent, &host) == 0);
  CHECK(click(find_button("Start")) == 0);
  smart_band_timer_app_tick_at(&host, 3000u);
  CHECK(find_text("04:55") != NULL);
  g_now_ms = 3000u;
  CHECK(click(find_button("Pause")) == 0);
  CHECK(click(find_button("Reset")) == 0);
  smart_band_timer_app_unmount();

  reset_objects();
  g_now_ms = UINT32_MAX - 1999u;
  CHECK(smart_band_stopwatch_app_build(&parent, &host) == 0);
  CHECK(click(find_button("Start")) == 0);
  smart_band_stopwatch_app_tick_at(&host, 3000u);
  CHECK(find_text("00:05") != NULL);
  g_now_ms = 3000u;
  CHECK(click(find_button("Pause")) == 0);
  CHECK(click(find_button("Reset")) == 0);
  smart_band_stopwatch_app_unmount();
  return 0;
}

int main(void)
{
  CHECK(test_timer_elapsed_and_unmount() == 0);
  CHECK(test_stopwatch_elapsed_and_unmount() == 0);
  CHECK(test_tick_wraparound() == 0);
  puts("timer and stopwatch production C tests passed");
  return 0;
}
