#include <lvgl/lvgl.h>

#include "fake_lvgl/fake_lvgl_test.h"

#include <stdlib.h>
#include <string.h>

#define FAKE_LVGL_TEXT_CAPACITY 128
#define FAKE_LVGL_EVENT_SNAPSHOT_CAPACITY 16

struct lv_font_s
{
  int unused;
};

typedef struct fake_event_handler_s fake_event_handler_t;

struct fake_event_handler_s
{
  lv_event_cb_t callback;
  lv_event_code_t code;
  void *user_data;
  fake_event_handler_t *next;
};

struct lv_obj_s
{
  lv_coord_t x;
  lv_coord_t y;
  lv_coord_t width;
  lv_coord_t height;
  uint32_t flags;
  char text[FAKE_LVGL_TEXT_CAPACITY];
  lv_obj_t *parent;
  lv_obj_t *first_child;
  lv_obj_t *last_child;
  lv_obj_t *previous_sibling;
  lv_obj_t *next_sibling;
  lv_obj_t *next_live;
  fake_event_handler_t *events;
};

struct lv_event_s
{
  lv_event_code_t code;
  void *user_data;
};

struct lv_indev_s
{
  lv_point_t point;
};

struct lv_timer_s
{
  lv_timer_cb_t callback;
  void *user_data;
  uint32_t period;
  uint32_t next_due;
  lv_timer_t *next;
};

const lv_font_t smart_band_test_default_font = {0};

static lv_obj_t g_screen;
static lv_obj_t *g_live_objects;
static lv_timer_t *g_timers;
static lv_indev_t g_indev;
static uint32_t g_tick;
static size_t g_live_object_count;
static size_t g_live_event_count;
static size_t g_live_timer_count;
static size_t g_object_create_attempts;
static size_t g_timer_create_attempts;
static size_t g_fail_object_create_at;
static size_t g_fail_timer_create_at;

static bool object_is_live(const lv_obj_t *object)
{
  const lv_obj_t *cursor;

  if (object == &g_screen)
    {
      return true;
    }

  for (cursor = g_live_objects; cursor != NULL; cursor = cursor->next_live)
    {
      if (cursor == object)
        {
          return true;
        }
    }

  return false;
}

static bool timer_is_live(const lv_timer_t *timer)
{
  const lv_timer_t *cursor;

  for (cursor = g_timers; cursor != NULL; cursor = cursor->next)
    {
      if (cursor == timer)
        {
          return true;
        }
    }

  return false;
}

static void link_child(lv_obj_t *parent, lv_obj_t *child)
{
  child->parent = parent;
  child->previous_sibling = parent->last_child;
  if (parent->last_child != NULL)
    {
      parent->last_child->next_sibling = child;
    }
  else
    {
      parent->first_child = child;
    }

  parent->last_child = child;
}

static void unlink_child(lv_obj_t *object)
{
  lv_obj_t *parent = object->parent;

  if (parent == NULL)
    {
      return;
    }

  if (object->previous_sibling != NULL)
    {
      object->previous_sibling->next_sibling = object->next_sibling;
    }
  else
    {
      parent->first_child = object->next_sibling;
    }

  if (object->next_sibling != NULL)
    {
      object->next_sibling->previous_sibling = object->previous_sibling;
    }
  else
    {
      parent->last_child = object->previous_sibling;
    }

  object->parent = NULL;
  object->previous_sibling = NULL;
  object->next_sibling = NULL;
}

static void unlink_live_object(lv_obj_t *object)
{
  lv_obj_t **cursor = &g_live_objects;

  while (*cursor != NULL)
    {
      if (*cursor == object)
        {
          *cursor = object->next_live;
          return;
        }

      cursor = &(*cursor)->next_live;
    }
}

static void delete_object(lv_obj_t *object)
{
  fake_event_handler_t *handler;

  if (object == NULL || !object_is_live(object))
    {
      return;
    }

  if (object == &g_screen)
    {
      lv_obj_clean(object);
      return;
    }

  while (object->first_child != NULL)
    {
      delete_object(object->first_child);
    }

  handler = object->events;
  while (handler != NULL)
    {
      fake_event_handler_t *next = handler->next;
      free(handler);
      handler = next;
      g_live_event_count--;
    }

  unlink_child(object);
  unlink_live_object(object);
  g_live_object_count--;
  free(object);
}

static lv_obj_t *create_object(lv_obj_t *parent, uint32_t default_flags)
{
  lv_obj_t *object;

  g_object_create_attempts++;
  if (g_object_create_attempts == g_fail_object_create_at ||
      parent == NULL || !object_is_live(parent))
    {
      return NULL;
    }

  object = calloc(1, sizeof(*object));
  if (object == NULL)
    {
      return NULL;
    }

  object->width = 1;
  object->height = 1;
  object->flags = default_flags;
  object->next_live = g_live_objects;
  g_live_objects = object;
  link_child(parent, object);
  g_live_object_count++;
  return object;
}

static lv_obj_t *find_text_recursive(lv_obj_t *object, const char *text,
                                     size_t *occurrence)
{
  lv_obj_t *child;

  if (object == NULL || text == NULL || !object_is_live(object))
    {
      return NULL;
    }

  if (strcmp(object->text, text) == 0)
    {
      if (*occurrence == 0)
        {
          return object;
        }

      (*occurrence)--;
    }

  for (child = object->first_child; child != NULL;
       child = child->next_sibling)
    {
      lv_obj_t *found = find_text_recursive(child, text, occurrence);
      if (found != NULL)
        {
          return found;
        }
    }

  return NULL;
}

void fake_lvgl_reset(void)
{
  while (g_timers != NULL)
    {
      lv_timer_del(g_timers);
    }

  lv_obj_clean(&g_screen);
  memset(&g_screen, 0, sizeof(g_screen));
  g_screen.width = 330;
  g_screen.height = 626;
  g_screen.flags = LV_OBJ_FLAG_SCROLLABLE;
  memset(&g_indev, 0, sizeof(g_indev));
  g_tick = 0;
  g_object_create_attempts = 0;
  g_timer_create_attempts = 0;
  g_fail_object_create_at = 0;
  g_fail_timer_create_at = 0;
}

size_t fake_lvgl_live_object_count(void) { return g_live_object_count; }
size_t fake_lvgl_live_event_count(void) { return g_live_event_count; }
size_t fake_lvgl_live_timer_count(void) { return g_live_timer_count; }
size_t fake_lvgl_object_create_attempts(void)
{
  return g_object_create_attempts;
}
size_t fake_lvgl_timer_create_attempts(void)
{
  return g_timer_create_attempts;
}

void fake_lvgl_fail_object_create_at(size_t nth)
{
  g_object_create_attempts = 0;
  g_fail_object_create_at = nth;
}

void fake_lvgl_fail_timer_create_at(size_t nth)
{
  g_timer_create_attempts = 0;
  g_fail_timer_create_at = nth;
}

const char *fake_lvgl_obj_text(const lv_obj_t *object)
{
  return object_is_live(object) ? object->text : NULL;
}

bool fake_lvgl_obj_has_flag(const lv_obj_t *object, uint32_t flag)
{
  return object_is_live(object) && (object->flags & flag) == flag;
}

lv_obj_t *fake_lvgl_obj_parent(const lv_obj_t *object)
{
  return object_is_live(object) ? object->parent : NULL;
}

lv_obj_t *fake_lvgl_find_text(lv_obj_t *subtree, const char *text,
                              size_t occurrence)
{
  return find_text_recursive(subtree, text, &occurrence);
}

void fake_lvgl_set_pointer(lv_coord_t x, lv_coord_t y)
{
  g_indev.point.x = x;
  g_indev.point.y = y;
}

void fake_lvgl_send_event(lv_obj_t *object, lv_event_code_t code)
{
  lv_event_cb_t callbacks[FAKE_LVGL_EVENT_SNAPSHOT_CAPACITY];
  void *user_data[FAKE_LVGL_EVENT_SNAPSHOT_CAPACITY];
  fake_event_handler_t *handler;
  size_t count = 0;
  size_t index;

  if (!object_is_live(object))
    {
      return;
    }

  for (handler = object->events; handler != NULL; handler = handler->next)
    {
      if (handler->code == code &&
          count < FAKE_LVGL_EVENT_SNAPSHOT_CAPACITY)
        {
          callbacks[count] = handler->callback;
          user_data[count] = handler->user_data;
          count++;
        }
    }

  for (index = 0; index < count; index++)
    {
      lv_event_t event;
      event.code = code;
      event.user_data = user_data[index];
      callbacks[index](&event);
    }
}

void fake_lvgl_set_tick(uint32_t tick) { g_tick = tick; }

size_t fake_lvgl_advance_tick(uint32_t delta_ms)
{
  lv_timer_t *timer;
  size_t callbacks = 0;

  g_tick += delta_ms;
  for (timer = g_timers; timer != NULL; timer = timer->next)
    {
      while ((int32_t)(g_tick - timer->next_due) >= 0)
        {
          timer->next_due += timer->period;
          timer->callback(timer);
          callbacks++;
        }
    }

  return callbacks;
}

void *lv_event_get_user_data(lv_event_t *event)
{
  return event == NULL ? NULL : event->user_data;
}

lv_event_code_t lv_event_get_code(lv_event_t *event)
{
  return event == NULL ? 0 : event->code;
}

lv_obj_t *lv_obj_create(lv_obj_t *parent)
{
  return create_object(parent, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *lv_btn_create(lv_obj_t *parent)
{
  return create_object(parent, LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t *lv_label_create(lv_obj_t *parent)
{
  return create_object(parent, 0);
}

lv_obj_t *lv_image_create(lv_obj_t *parent)
{
  return create_object(parent, 0);
}

lv_obj_t *lv_bar_create(lv_obj_t *parent)
{
  return create_object(parent, 0);
}

lv_obj_t *lv_obj_get_child(lv_obj_t *parent, int32_t index)
{
  lv_obj_t *child;

  if (!object_is_live(parent))
    {
      return NULL;
    }

  child = index < 0 ? parent->last_child : parent->first_child;
  while (child != NULL && index != 0 && index != -1)
    {
      if (index > 0)
        {
          index--;
          child = child->next_sibling;
        }
      else
        {
          index++;
          child = child->previous_sibling;
        }
    }

  return child;
}

uint32_t lv_obj_get_child_count(lv_obj_t *parent)
{
  lv_obj_t *child;
  uint32_t count = 0;

  if (!object_is_live(parent))
    {
      return 0;
    }

  for (child = parent->first_child; child != NULL;
       child = child->next_sibling)
    {
      count++;
    }

  return count;
}

lv_coord_t lv_obj_get_width(lv_obj_t *object)
{
  return object_is_live(object) ? object->width : 0;
}

lv_coord_t lv_obj_get_height(lv_obj_t *object)
{
  return object_is_live(object) ? object->height : 0;
}

lv_coord_t lv_obj_get_y(lv_obj_t *object)
{
  return object_is_live(object) ? object->y : 0;
}

void lv_obj_remove_style_all(lv_obj_t *object) { (void)object; }

void lv_obj_set_pos(lv_obj_t *object, lv_coord_t x, lv_coord_t y)
{
  if (object_is_live(object))
    {
      object->x = x;
      object->y = y;
    }
}

void lv_obj_set_y(lv_obj_t *object, lv_coord_t y)
{
  if (object_is_live(object))
    {
      object->y = y;
    }
}

void lv_obj_set_size(lv_obj_t *object, lv_coord_t width, lv_coord_t height)
{
  if (object_is_live(object))
    {
      object->width = width;
      object->height = height;
    }
}

#define DEFINE_OBJ_NOOP_2(name, type1, type2) \
  void name(lv_obj_t *object, type1 value1, type2 value2) \
  { (void)object; (void)value1; (void)value2; }

void lv_obj_align(lv_obj_t *object, int align, lv_coord_t x, lv_coord_t y)
{
  if (!object_is_live(object))
    {
      return;
    }

  object->x = x;
  object->y = y;
  if (align == LV_ALIGN_BOTTOM_MID && object->parent != NULL)
    {
      object->x += (object->parent->width - object->width) / 2;
      object->y += object->parent->height - object->height;
    }
}

void lv_obj_center(lv_obj_t *object)
{
  if (object_is_live(object) && object->parent != NULL)
    {
      object->x = (object->parent->width - object->width) / 2;
      object->y = (object->parent->height - object->height) / 2;
    }
}

void lv_obj_update_layout(lv_obj_t *object) { (void)object; }

void lv_obj_add_flag(lv_obj_t *object, uint32_t flag)
{
  if (object_is_live(object))
    {
      object->flags |= flag;
    }
}

void lv_obj_clear_flag(lv_obj_t *object, uint32_t flag)
{
  if (object_is_live(object))
    {
      object->flags &= ~flag;
    }
}

void lv_obj_add_event_cb(lv_obj_t *object, lv_event_cb_t callback,
                         lv_event_code_t code, void *user_data)
{
  fake_event_handler_t *handler;
  fake_event_handler_t **tail;

  if (!object_is_live(object) || callback == NULL)
    {
      return;
    }

  handler = calloc(1, sizeof(*handler));
  if (handler == NULL)
    {
      return;
    }

  handler->callback = callback;
  handler->code = code;
  handler->user_data = user_data;
  tail = &object->events;
  while (*tail != NULL)
    {
      tail = &(*tail)->next;
    }

  *tail = handler;
  g_live_event_count++;
}

void lv_obj_clean(lv_obj_t *object)
{
  if (!object_is_live(object))
    {
      return;
    }

  while (object->first_child != NULL)
    {
      delete_object(object->first_child);
    }
}

void lv_obj_move_foreground(lv_obj_t *object)
{
  lv_obj_t *parent;

  if (!object_is_live(object) || object->parent == NULL ||
      object->parent->last_child == object)
    {
      return;
    }

  parent = object->parent;
  unlink_child(object);
  link_child(parent, object);
}

void lv_obj_invalidate(lv_obj_t *object) { (void)object; }

int lv_obj_is_valid(lv_obj_t *object) { return object_is_live(object); }

void lv_obj_del(lv_obj_t *object) { delete_object(object); }

DEFINE_OBJ_NOOP_2(lv_obj_set_style_bg_color, lv_color_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_bg_grad_color, lv_color_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_bg_grad_dir, int, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_bg_opa, lv_opa_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_border_color, lv_color_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_border_width, lv_coord_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_clip_corner, bool, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_pad_all, lv_coord_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_radius, lv_coord_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_shadow_color, lv_color_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_shadow_offset_y, lv_coord_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_shadow_opa, lv_opa_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_shadow_width, lv_coord_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_text_align, lv_text_align_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_text_color, lv_color_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_text_font, const lv_font_t *, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_text_opa, lv_opa_t, uint32_t)
DEFINE_OBJ_NOOP_2(lv_obj_set_style_transform_rotation, int32_t, uint32_t)

void lv_label_set_text(lv_obj_t *label, const char *text)
{
  size_t length;

  if (!object_is_live(label))
    {
      return;
    }

  if (text == NULL)
    {
      label->text[0] = '\0';
      return;
    }

  length = strlen(text);
  if (length >= sizeof(label->text))
    {
      length = sizeof(label->text) - 1;
    }

  memcpy(label->text, text, length);
  label->text[length] = '\0';
}

void lv_label_set_long_mode(lv_obj_t *label, int mode)
{
  (void)label;
  (void)mode;
}

void lv_image_set_src(lv_obj_t *image, const void *source)
{
  (void)image;
  (void)source;
}

void lv_image_set_scale(lv_obj_t *image, uint32_t scale)
{
  (void)image;
  (void)scale;
}

void lv_bar_set_range(lv_obj_t *bar, int32_t minimum, int32_t maximum)
{
  (void)bar;
  (void)minimum;
  (void)maximum;
}

void lv_bar_set_value(lv_obj_t *bar, int32_t value, int animated)
{
  (void)bar;
  (void)value;
  (void)animated;
}

lv_indev_t *lv_indev_get_act(void) { return &g_indev; }

void lv_indev_get_point(lv_indev_t *indev, lv_point_t *point)
{
  if (indev != NULL && point != NULL)
    {
      *point = indev->point;
    }
}

lv_obj_t *lv_scr_act(void) { return &g_screen; }
uint32_t lv_tick_get(void) { return g_tick; }
uint32_t lv_tick_elaps(uint32_t previous_tick) { return g_tick - previous_tick; }

lv_timer_t *lv_timer_create(lv_timer_cb_t callback, uint32_t period,
                            void *user_data)
{
  lv_timer_t *timer;

  g_timer_create_attempts++;
  if (g_timer_create_attempts == g_fail_timer_create_at || callback == NULL)
    {
      return NULL;
    }

  timer = calloc(1, sizeof(*timer));
  if (timer == NULL)
    {
      return NULL;
    }

  timer->callback = callback;
  timer->user_data = user_data;
  timer->period = period == 0 ? 1 : period;
  timer->next_due = g_tick + timer->period;
  timer->next = g_timers;
  g_timers = timer;
  g_live_timer_count++;
  return timer;
}

void lv_timer_del(lv_timer_t *timer)
{
  lv_timer_t **cursor;

  if (!timer_is_live(timer))
    {
      return;
    }

  cursor = &g_timers;
  while (*cursor != timer)
    {
      cursor = &(*cursor)->next;
    }

  *cursor = timer->next;
  g_live_timer_count--;
  free(timer);
}

void lv_refr_now(void *display) { (void)display; }

void lv_anim_init(lv_anim_t *anim)
{
  if (anim != NULL)
    {
      memset(anim, 0, sizeof(*anim));
    }
}

void lv_anim_set_var(lv_anim_t *anim, void *object) { anim->var = object; }

void lv_anim_set_values(lv_anim_t *anim, int32_t start, int32_t end)
{
  anim->start_value = start;
  anim->end_value = end;
}

void lv_anim_set_exec_cb(lv_anim_t *anim, lv_anim_exec_xcb_t callback)
{
  anim->exec_cb = callback;
}

void lv_anim_set_time(lv_anim_t *anim, uint32_t duration)
{
  anim->duration = duration;
}

void lv_anim_set_path_cb(lv_anim_t *anim, lv_anim_path_cb_t callback)
{
  anim->path_cb = callback;
}

void lv_anim_set_ready_cb(lv_anim_t *anim, lv_anim_ready_cb_t callback)
{
  anim->ready_cb = callback;
}

void lv_anim_start(const lv_anim_t *anim)
{
  lv_anim_t completed;

  if (anim == NULL)
    {
      return;
    }

  completed = *anim;
  if (completed.exec_cb != NULL)
    {
      completed.exec_cb(completed.var, completed.end_value);
    }

  if (completed.ready_cb != NULL)
    {
      completed.ready_cb(&completed);
    }
}

int32_t lv_anim_path_ease_out(const lv_anim_t *anim)
{
  return anim == NULL ? 0 : anim->end_value;
}
