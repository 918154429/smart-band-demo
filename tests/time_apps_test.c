#include "smart_band_apps.h"

#include "icon_assets.h"

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

#define TEST_OBJECT_CAPACITY 256
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
  struct lv_obj_s *parent;
  struct lv_obj_s *child;
  lv_event_cb_t callback;
  uintptr_t user_data;
  int width;
  int height;
  bool valid;
};

static struct lv_obj_s g_objects[TEST_OBJECT_CAPACITY];
static size_t g_object_count;
static unsigned int g_lvgl_writes;
static uint32_t g_now_ms;
static struct lv_font_s g_font;

const lv_image_dsc_t smart_band_icon_weather = {0};
const lv_image_dsc_t smart_band_icon_calculator = {0};
const lv_image_dsc_t smart_band_icon_timer = {0};
const lv_image_dsc_t smart_band_icon_game2048 = {0};
const lv_image_dsc_t smart_band_icon_stopwatch = {0};
const lv_image_dsc_t smart_band_icon_mines = {0};
const lv_image_dsc_t smart_band_icon_tetris = {0};
const lv_image_dsc_t smart_band_icon_wooden_fish = {0};

const smart_band_app_ops_t smart_band_weather_app_ops =
{
  .context_size = 0
};
const smart_band_app_ops_t smart_band_calculator_app_ops =
{
  .context_size = 0
};
const smart_band_app_ops_t smart_band_2048_app_ops =
{
  .context_size = 0
};
const smart_band_app_ops_t smart_band_mines_app_ops =
{
  .context_size = 0
};
const smart_band_app_ops_t smart_band_tetris_app_ops =
{
  .context_size = 0
};
const smart_band_app_ops_t smart_band_wooden_fish_app_ops =
{
  .context_size = 0
};

static lv_obj_t *new_object(lv_obj_t *parent)
{
  lv_obj_t *object;

  if (g_object_count >= TEST_OBJECT_CAPACITY)
    {
      return NULL;
    }

  object = &g_objects[g_object_count];
  g_object_count++;
  object->parent = parent;
  object->valid = true;
  return object;
}

void *lv_event_get_user_data(lv_event_t *event)
{
  return (void *)event->user_data;
}

lv_obj_t *lv_obj_create(lv_obj_t *parent)
{
  return new_object(parent);
}

lv_obj_t *lv_obj_get_child(lv_obj_t *parent, int index)
{
  if (parent == NULL || !parent->valid || index != 0 ||
      parent->child == NULL || !parent->child->valid)
    {
      return NULL;
    }

  return parent->child;
}

int lv_obj_get_width(lv_obj_t *object)
{
  return object == NULL ? 0 : object->width;
}

int lv_obj_get_height(lv_obj_t *object)
{
  return object == NULL ? 0 : object->height;
}

void lv_obj_remove_style_all(lv_obj_t *object)
{
  (void)object;
}

void lv_obj_set_pos(lv_obj_t *object, int x, int y)
{
  (void)object;
  (void)x;
  (void)y;
}

void lv_obj_set_size(lv_obj_t *object, int width, int height)
{
  if (object != NULL)
    {
      object->width = width;
      object->height = height;
    }
}

int lv_obj_is_valid(lv_obj_t *object)
{
  return object != NULL && object->valid;
}

static bool object_is_descendant(const lv_obj_t *object,
                                 const lv_obj_t *ancestor)
{
  const lv_obj_t *current = object;

  while (current != NULL)
    {
      if (current == ancestor)
        {
          return true;
        }

      current = current->parent;
    }

  return false;
}

void lv_obj_del(lv_obj_t *object)
{
  size_t index;

  if (object == NULL || !object->valid)
    {
      return;
    }

  for (index = 0; index < g_object_count; index++)
    {
      if (g_objects[index].valid &&
          object_is_descendant(&g_objects[index], object))
        {
          g_objects[index].valid = false;
          g_objects[index].callback = NULL;
          g_objects[index].user_data = 0;
        }
    }
}

void lv_label_set_text(lv_obj_t *label, const char *text)
{
  if (label != NULL && label->valid)
    {
      (void)snprintf(label->text, sizeof(label->text), "%s", text);
      g_lvgl_writes++;
    }
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
  lv_obj_t *object = new_object(parent);

  (void)x;
  (void)y;
  (void)color;
  (void)radius;
  if (object != NULL)
    {
      object->width = w;
      object->height = h;
    }

  return object;
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *selected_font,
                              lv_color_t color, lv_text_align_t align)
{
  lv_obj_t *label = new_object(parent);

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
  lv_obj_t *button = new_object(parent);
  lv_obj_t *label;

  (void)x;
  (void)y;
  (void)color;
  if (button == NULL)
    {
      return NULL;
    }

  label = new_object(button);
  if (label == NULL)
    {
      return NULL;
    }

  button->width = w;
  button->height = h;
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
  g_lvgl_writes = 0;
}

static void init_parent(lv_obj_t *parent)
{
  memset(parent, 0, sizeof(*parent));
  parent->width = 330;
  parent->height = 532;
  parent->valid = true;
}

static lv_obj_t *find_text_under(lv_obj_t *root, const char *text)
{
  size_t index;

  for (index = 0; index < g_object_count; index++)
    {
      if (g_objects[index].valid &&
          object_is_descendant(&g_objects[index], root) &&
          strcmp(g_objects[index].text, text) == 0)
        {
          return &g_objects[index];
        }
    }

  return NULL;
}

static lv_obj_t *find_button_under(lv_obj_t *root, const char *label_text)
{
  size_t index;

  for (index = 0; index < g_object_count; index++)
    {
      lv_obj_t *child = g_objects[index].child;

      if (g_objects[index].valid &&
          object_is_descendant(&g_objects[index], root) && child != NULL &&
          child->valid && strcmp(child->text, label_text) == 0)
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
  CHECK(button->valid);
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

static int mount_app(smart_band_apps_runtime_t *runtime,
                     smart_band_app_id_t id, lv_obj_t *parent,
                     const smart_band_app_host_t *host)
{
  CHECK(smart_band_app_mount(runtime, id, parent, host) == 0);
  CHECK(runtime->owned_root != NULL);
  CHECK(lv_obj_is_valid(runtime->owned_root));
  CHECK(smart_band_apps_active(runtime) == id);
  return 0;
}

static int test_timer_elapsed_and_unmount(void)
{
  smart_band_apps_runtime_t runtime;
  smart_band_app_host_t host = make_host();
  lv_obj_t parent;
  lv_obj_t *display;
  lv_obj_t *old_root;
  unsigned int writes_before_tick;

  init_parent(&parent);
  reset_objects();
  CHECK(smart_band_apps_init(&runtime) == 0);
  g_now_ms = 1000;

  CHECK(mount_app(&runtime, SMART_BAND_APP_TIMER, &parent, &host) == 0);
  display = find_text_under(runtime.owned_root, "05:00");
  CHECK(display != NULL);
  CHECK(click(find_button_under(runtime.owned_root, "Start")) == 0);

  smart_band_apps_tick_at(&runtime, true, 6500);
  smart_band_app_render(&runtime, &host);
  CHECK(strcmp(display->text, "04:55") == 0);

  old_root = runtime.owned_root;
  smart_band_app_unmount(&runtime);
  CHECK(!lv_obj_is_valid(old_root));
  CHECK(smart_band_apps_active(&runtime) == SMART_BAND_APP_NONE);
  writes_before_tick = g_lvgl_writes;
  smart_band_apps_tick_at(&runtime, false, 11500);
  CHECK(g_lvgl_writes == writes_before_tick);

  CHECK(mount_app(&runtime, SMART_BAND_APP_TIMER, &parent, &host) == 0);
  CHECK(find_text_under(runtime.owned_root, "04:50") != NULL);
  g_now_ms = 11500;
  CHECK(click(find_button_under(runtime.owned_root, "Pause")) == 0);
  CHECK(click(find_button_under(runtime.owned_root, "Reset")) == 0);
  smart_band_apps_deinit(&runtime);
  return 0;
}

static int test_stopwatch_elapsed_and_unmount(void)
{
  smart_band_apps_runtime_t runtime;
  smart_band_app_host_t host = make_host();
  lv_obj_t parent;
  lv_obj_t *display;
  lv_obj_t *old_root;
  unsigned int writes_before_tick;

  init_parent(&parent);
  reset_objects();
  CHECK(smart_band_apps_init(&runtime) == 0);
  g_now_ms = 10000;

  CHECK(mount_app(&runtime, SMART_BAND_APP_STOPWATCH, &parent, &host) == 0);
  display = find_text_under(runtime.owned_root, "00:00");
  CHECK(display != NULL);
  CHECK(click(find_button_under(runtime.owned_root, "Start")) == 0);

  smart_band_apps_tick_at(&runtime, true, 15750);
  smart_band_app_render(&runtime, &host);
  CHECK(strcmp(display->text, "00:05") == 0);

  old_root = runtime.owned_root;
  smart_band_app_unmount(&runtime);
  CHECK(!lv_obj_is_valid(old_root));
  writes_before_tick = g_lvgl_writes;
  smart_band_apps_tick_at(&runtime, false, 20250);
  CHECK(g_lvgl_writes == writes_before_tick);

  CHECK(mount_app(&runtime, SMART_BAND_APP_STOPWATCH, &parent, &host) == 0);
  CHECK(find_text_under(runtime.owned_root, "00:10") != NULL);
  g_now_ms = 20250;
  CHECK(click(find_button_under(runtime.owned_root, "Pause")) == 0);
  CHECK(click(find_button_under(runtime.owned_root, "Reset")) == 0);
  smart_band_apps_deinit(&runtime);
  return 0;
}

static int test_tick_wraparound(void)
{
  smart_band_apps_runtime_t runtime;
  smart_band_app_host_t host = make_host();
  lv_obj_t parent;

  init_parent(&parent);
  reset_objects();
  CHECK(smart_band_apps_init(&runtime) == 0);
  g_now_ms = UINT32_MAX - 1999u;
  CHECK(mount_app(&runtime, SMART_BAND_APP_TIMER, &parent, &host) == 0);
  CHECK(click(find_button_under(runtime.owned_root, "Start")) == 0);
  smart_band_apps_tick_at(&runtime, true, 3000u);
  smart_band_app_render(&runtime, &host);
  CHECK(find_text_under(runtime.owned_root, "04:55") != NULL);
  g_now_ms = 3000u;
  CHECK(click(find_button_under(runtime.owned_root, "Pause")) == 0);
  CHECK(click(find_button_under(runtime.owned_root, "Reset")) == 0);
  smart_band_app_unmount(&runtime);

  g_now_ms = UINT32_MAX - 1999u;
  CHECK(mount_app(&runtime, SMART_BAND_APP_STOPWATCH, &parent, &host) == 0);
  CHECK(click(find_button_under(runtime.owned_root, "Start")) == 0);
  smart_band_apps_tick_at(&runtime, true, 3000u);
  smart_band_app_render(&runtime, &host);
  CHECK(find_text_under(runtime.owned_root, "00:05") != NULL);
  g_now_ms = 3000u;
  CHECK(click(find_button_under(runtime.owned_root, "Pause")) == 0);
  CHECK(click(find_button_under(runtime.owned_root, "Reset")) == 0);
  smart_band_apps_deinit(&runtime);
  return 0;
}

static int test_runtime_isolation(void)
{
  smart_band_apps_runtime_t first;
  smart_band_apps_runtime_t second;
  smart_band_app_host_t host = make_host();
  lv_obj_t first_parent;
  lv_obj_t second_parent;
  lv_obj_t *first_display;
  lv_obj_t *second_display;

  init_parent(&first_parent);
  init_parent(&second_parent);
  reset_objects();
  CHECK(smart_band_apps_init(&first) == 0);
  CHECK(smart_band_apps_init(&second) == 0);
  CHECK(mount_app(&first, SMART_BAND_APP_TIMER, &first_parent, &host) == 0);
  CHECK(mount_app(&second, SMART_BAND_APP_TIMER, &second_parent, &host) == 0);
  first_display = find_text_under(first.owned_root, "05:00");
  second_display = find_text_under(second.owned_root, "05:00");
  CHECK(first_display != NULL);
  CHECK(second_display != NULL);
  CHECK(first_display != second_display);

  g_now_ms = 1000;
  CHECK(click(find_button_under(first.owned_root, "Start")) == 0);
  smart_band_apps_tick_at(&first, true, 6000);
  smart_band_apps_tick_at(&second, true, 6000);
  smart_band_app_render(&first, &host);
  smart_band_app_render(&second, &host);
  CHECK(strcmp(first_display->text, "04:55") == 0);
  CHECK(strcmp(second_display->text, "05:00") == 0);

  g_now_ms = 7000;
  CHECK(click(find_button_under(second.owned_root, "Start")) == 0);
  smart_band_apps_tick_at(&first, true, 9000);
  smart_band_apps_tick_at(&second, true, 9000);
  smart_band_app_render(&first, &host);
  smart_band_app_render(&second, &host);
  CHECK(strcmp(first_display->text, "04:52") == 0);
  CHECK(strcmp(second_display->text, "04:58") == 0);

  smart_band_apps_deinit(&first);
  CHECK(smart_band_apps_active(&second) == SMART_BAND_APP_TIMER);
  CHECK(lv_obj_is_valid(second.owned_root));
  smart_band_apps_deinit(&second);
  return 0;
}

int main(void)
{
  CHECK(test_timer_elapsed_and_unmount() == 0);
  CHECK(test_stopwatch_elapsed_and_unmount() == 0);
  CHECK(test_tick_wraparound() == 0);
  CHECK(test_runtime_isolation() == 0);
  puts("timer and stopwatch production runtime tests passed");
  return 0;
}
