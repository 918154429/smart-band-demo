#ifndef SMART_BAND_TEST_LVGL_H
#define SMART_BAND_TEST_LVGL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int lv_coord_t;
typedef int lv_color_t;
typedef int lv_text_align_t;
typedef int lv_event_code_t;
typedef uint8_t lv_opa_t;

typedef struct lv_obj_s lv_obj_t;
typedef struct lv_event_s lv_event_t;
typedef struct lv_font_s lv_font_t;
typedef struct lv_indev_s lv_indev_t;
typedef struct lv_timer_s lv_timer_t;
typedef struct lv_anim_s lv_anim_t;

typedef struct
{
  lv_coord_t x;
  lv_coord_t y;
} lv_point_t;

typedef struct
{
  uint32_t magic;
  uint32_t cf;
  uint32_t flags;
  uint32_t w;
  uint32_t h;
  uint32_t stride;
} lv_image_header_t;

typedef struct
{
  lv_image_header_t header;
  size_t data_size;
  const uint8_t *data;
} lv_image_dsc_t;

typedef void (*lv_event_cb_t)(lv_event_t *event);
typedef void (*lv_timer_cb_t)(lv_timer_t *timer);
typedef void (*lv_anim_exec_xcb_t)(void *object, int32_t value);
typedef void (*lv_anim_ready_cb_t)(lv_anim_t *anim);
typedef int32_t (*lv_anim_path_cb_t)(const lv_anim_t *anim);

struct lv_anim_s
{
  void *var;
  lv_anim_exec_xcb_t exec_cb;
  lv_anim_ready_cb_t ready_cb;
  lv_anim_path_cb_t path_cb;
  int32_t start_value;
  int32_t end_value;
  uint32_t duration;
};

#define LV_TEXT_ALIGN_CENTER 0
#define LV_TEXT_ALIGN_LEFT 1
#define LV_TEXT_ALIGN_RIGHT 2
#define LV_EVENT_CLICKED 1
#define LV_EVENT_PRESSED 2
#define LV_EVENT_RELEASED 3
#define LV_OBJ_FLAG_CLICKABLE (1u << 0)
#define LV_OBJ_FLAG_HIDDEN (1u << 1)
#define LV_OBJ_FLAG_SCROLLABLE (1u << 2)
#define LV_PART_INDICATOR (1u << 0)
#define LV_STATE_PRESSED (1u << 1)
#define LV_ALIGN_BOTTOM_MID 0
#define LV_GRAD_DIR_HOR 1
#define LV_GRAD_DIR_VER 2
#define LV_LABEL_LONG_CLIP 0
#define LV_ANIM_ON 1
#define LV_OPA_TRANSP ((lv_opa_t)0)
#define LV_OPA_10 ((lv_opa_t)26)
#define LV_OPA_20 ((lv_opa_t)51)
#define LV_OPA_30 ((lv_opa_t)77)
#define LV_OPA_40 ((lv_opa_t)102)
#define LV_OPA_50 ((lv_opa_t)128)
#define LV_OPA_COVER ((lv_opa_t)255)
#define LV_RADIUS_CIRCLE 0x7fff
#define LV_SCALE_NONE 256
#define LV_SYMBOL_CHARGE "+"
#define LV_IMAGE_HEADER_MAGIC 0x19
#define LV_COLOR_FORMAT_ARGB8888 1

#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_48 0

extern const lv_font_t smart_band_test_default_font;
#define LV_FONT_DEFAULT (&smart_band_test_default_font)

static inline lv_color_t lv_color_hex(uint32_t value)
{
  return (lv_color_t)value;
}

void *lv_event_get_user_data(lv_event_t *event);
lv_event_code_t lv_event_get_code(lv_event_t *event);
lv_obj_t *lv_obj_create(lv_obj_t *parent);
lv_obj_t *lv_btn_create(lv_obj_t *parent);
lv_obj_t *lv_label_create(lv_obj_t *parent);
lv_obj_t *lv_image_create(lv_obj_t *parent);
lv_obj_t *lv_bar_create(lv_obj_t *parent);
lv_obj_t *lv_obj_get_child(lv_obj_t *parent, int32_t index);
uint32_t lv_obj_get_child_count(lv_obj_t *parent);
lv_coord_t lv_obj_get_width(lv_obj_t *object);
lv_coord_t lv_obj_get_height(lv_obj_t *object);
lv_coord_t lv_obj_get_y(lv_obj_t *object);
void lv_obj_remove_style_all(lv_obj_t *object);
void lv_obj_set_pos(lv_obj_t *object, lv_coord_t x, lv_coord_t y);
void lv_obj_set_y(lv_obj_t *object, lv_coord_t y);
void lv_obj_set_size(lv_obj_t *object, lv_coord_t width, lv_coord_t height);
void lv_obj_align(lv_obj_t *object, int align, lv_coord_t x, lv_coord_t y);
void lv_obj_center(lv_obj_t *object);
void lv_obj_update_layout(lv_obj_t *object);
void lv_obj_add_flag(lv_obj_t *object, uint32_t flag);
void lv_obj_clear_flag(lv_obj_t *object, uint32_t flag);
void lv_obj_add_event_cb(lv_obj_t *object, lv_event_cb_t callback,
                         lv_event_code_t code, void *user_data);
void lv_obj_clean(lv_obj_t *object);
void lv_obj_move_foreground(lv_obj_t *object);
void lv_obj_invalidate(lv_obj_t *object);
int lv_obj_is_valid(lv_obj_t *object);
void lv_obj_del(lv_obj_t *object);
void lv_obj_set_style_bg_color(lv_obj_t *object, lv_color_t color,
                               uint32_t selector);
void lv_obj_set_style_bg_grad_color(lv_obj_t *object, lv_color_t color,
                                    uint32_t selector);
void lv_obj_set_style_bg_grad_dir(lv_obj_t *object, int direction,
                                  uint32_t selector);
void lv_obj_set_style_bg_opa(lv_obj_t *object, lv_opa_t opacity,
                             uint32_t selector);
void lv_obj_set_style_border_color(lv_obj_t *object, lv_color_t color,
                                   uint32_t selector);
void lv_obj_set_style_border_width(lv_obj_t *object, lv_coord_t width,
                                   uint32_t selector);
void lv_obj_set_style_clip_corner(lv_obj_t *object, bool enabled,
                                  uint32_t selector);
void lv_obj_set_style_pad_all(lv_obj_t *object, lv_coord_t padding,
                              uint32_t selector);
void lv_obj_set_style_radius(lv_obj_t *object, lv_coord_t radius,
                             uint32_t selector);
void lv_obj_set_style_shadow_color(lv_obj_t *object, lv_color_t color,
                                   uint32_t selector);
void lv_obj_set_style_shadow_offset_y(lv_obj_t *object, lv_coord_t offset,
                                      uint32_t selector);
void lv_obj_set_style_shadow_opa(lv_obj_t *object, lv_opa_t opacity,
                                 uint32_t selector);
void lv_obj_set_style_shadow_width(lv_obj_t *object, lv_coord_t width,
                                   uint32_t selector);
void lv_obj_set_style_text_align(lv_obj_t *object, lv_text_align_t align,
                                 uint32_t selector);
void lv_obj_set_style_text_color(lv_obj_t *object, lv_color_t color,
                                 uint32_t selector);
void lv_obj_set_style_text_font(lv_obj_t *object, const lv_font_t *font,
                                uint32_t selector);
void lv_obj_set_style_text_opa(lv_obj_t *object, lv_opa_t opacity,
                               uint32_t selector);
void lv_obj_set_style_transform_rotation(lv_obj_t *object, int32_t rotation,
                                         uint32_t selector);
void lv_label_set_text(lv_obj_t *label, const char *text);
void lv_label_set_long_mode(lv_obj_t *label, int mode);
void lv_image_set_src(lv_obj_t *image, const void *source);
void lv_image_set_scale(lv_obj_t *image, uint32_t scale);
void lv_bar_set_range(lv_obj_t *bar, int32_t minimum, int32_t maximum);
void lv_bar_set_value(lv_obj_t *bar, int32_t value, int animated);
lv_indev_t *lv_indev_get_act(void);
void lv_indev_get_point(lv_indev_t *indev, lv_point_t *point);
lv_obj_t *lv_scr_act(void);
uint32_t lv_tick_get(void);
uint32_t lv_tick_elaps(uint32_t previous_tick);
lv_timer_t *lv_timer_create(lv_timer_cb_t callback, uint32_t period,
                            void *user_data);
void lv_timer_del(lv_timer_t *timer);
void lv_refr_now(void *display);

void lv_anim_init(lv_anim_t *anim);
void lv_anim_set_var(lv_anim_t *anim, void *object);
void lv_anim_set_values(lv_anim_t *anim, int32_t start, int32_t end);
void lv_anim_set_exec_cb(lv_anim_t *anim, lv_anim_exec_xcb_t callback);
void lv_anim_set_time(lv_anim_t *anim, uint32_t duration);
void lv_anim_set_path_cb(lv_anim_t *anim, lv_anim_path_cb_t callback);
void lv_anim_set_ready_cb(lv_anim_t *anim, lv_anim_ready_cb_t callback);
void lv_anim_start(const lv_anim_t *anim);
int32_t lv_anim_path_ease_out(const lv_anim_t *anim);

#endif
