#include <lvgl/lvgl.h>

#include <string.h>

struct lv_font_s
{
  int unused;
};

struct lv_obj_s
{
  lv_coord_t x;
  lv_coord_t y;
  lv_coord_t width;
  lv_coord_t height;
  bool valid;
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
};

const lv_font_t smart_band_test_default_font = {0};

static lv_obj_t g_object = {0, 0, 330, 626, true};
static lv_indev_t g_indev;
static lv_timer_t g_timer;
static uint32_t g_tick;

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
  (void)parent;
  g_object.valid = true;
  return &g_object;
}

lv_obj_t *lv_btn_create(lv_obj_t *parent) { return lv_obj_create(parent); }
lv_obj_t *lv_label_create(lv_obj_t *parent) { return lv_obj_create(parent); }
lv_obj_t *lv_image_create(lv_obj_t *parent) { return lv_obj_create(parent); }
lv_obj_t *lv_bar_create(lv_obj_t *parent) { return lv_obj_create(parent); }

lv_obj_t *lv_obj_get_child(lv_obj_t *parent, int32_t index)
{
  (void)parent;
  (void)index;
  return NULL;
}

uint32_t lv_obj_get_child_count(lv_obj_t *parent)
{
  (void)parent;
  return 0;
}

lv_coord_t lv_obj_get_width(lv_obj_t *object)
{
  return object == NULL ? 0 : object->width;
}

lv_coord_t lv_obj_get_height(lv_obj_t *object)
{
  return object == NULL ? 0 : object->height;
}

lv_coord_t lv_obj_get_y(lv_obj_t *object)
{
  return object == NULL ? 0 : object->y;
}

void lv_obj_remove_style_all(lv_obj_t *object) { (void)object; }

void lv_obj_set_pos(lv_obj_t *object, lv_coord_t x, lv_coord_t y)
{
  if (object != NULL)
    {
      object->x = x;
      object->y = y;
    }
}

void lv_obj_set_y(lv_obj_t *object, lv_coord_t y)
{
  if (object != NULL)
    {
      object->y = y;
    }
}

void lv_obj_set_size(lv_obj_t *object, lv_coord_t width, lv_coord_t height)
{
  if (object != NULL)
    {
      object->width = width;
      object->height = height;
    }
}

#define DEFINE_OBJ_NOOP_1(name, type) \
  void name(lv_obj_t *object, type value) { (void)object; (void)value; }
#define DEFINE_OBJ_NOOP_2(name, type1, type2) \
  void name(lv_obj_t *object, type1 value1, type2 value2) \
  { (void)object; (void)value1; (void)value2; }

void lv_obj_align(lv_obj_t *object, int align, lv_coord_t x, lv_coord_t y)
{
  (void)object;
  (void)align;
  (void)x;
  (void)y;
}

void lv_obj_center(lv_obj_t *object) { (void)object; }
void lv_obj_update_layout(lv_obj_t *object) { (void)object; }
DEFINE_OBJ_NOOP_1(lv_obj_add_flag, uint32_t)
DEFINE_OBJ_NOOP_1(lv_obj_clear_flag, uint32_t)

void lv_obj_add_event_cb(lv_obj_t *object, lv_event_cb_t callback,
                         lv_event_code_t code, void *user_data)
{
  (void)object;
  (void)callback;
  (void)code;
  (void)user_data;
}

void lv_obj_clean(lv_obj_t *object) { (void)object; }
void lv_obj_move_foreground(lv_obj_t *object) { (void)object; }
void lv_obj_invalidate(lv_obj_t *object) { (void)object; }

int lv_obj_is_valid(lv_obj_t *object)
{
  return object != NULL && object->valid;
}

void lv_obj_del(lv_obj_t *object)
{
  if (object != NULL)
    {
      object->valid = false;
    }
}

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
  (void)label;
  (void)text;
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

lv_obj_t *lv_scr_act(void) { return &g_object; }
uint32_t lv_tick_get(void) { return g_tick++; }
uint32_t lv_tick_elaps(uint32_t previous_tick) { return g_tick - previous_tick; }

lv_timer_t *lv_timer_create(lv_timer_cb_t callback, uint32_t period,
                            void *user_data)
{
  (void)period;
  (void)user_data;
  g_timer.callback = callback;
  return &g_timer;
}

void lv_timer_del(lv_timer_t *timer) { (void)timer; }
void lv_refr_now(void *display) { (void)display; }

void lv_anim_init(lv_anim_t *anim)
{
  if (anim != NULL)
    {
      memset(anim, 0, sizeof(*anim));
    }
}

void lv_anim_set_var(lv_anim_t *anim, void *object)
{
  anim->var = object;
}

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

void lv_anim_start(const lv_anim_t *anim) { (void)anim; }

int32_t lv_anim_path_ease_out(const lv_anim_t *anim)
{
  return anim == NULL ? 0 : anim->end_value;
}
