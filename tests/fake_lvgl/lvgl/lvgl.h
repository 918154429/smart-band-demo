#ifndef SMART_BAND_TEST_LVGL_H
#define SMART_BAND_TEST_LVGL_H

#include <stdint.h>

typedef int lv_coord_t;
typedef int lv_color_t;
typedef int lv_text_align_t;
typedef struct lv_obj_s lv_obj_t;
typedef struct lv_event_s lv_event_t;
typedef struct lv_font_s lv_font_t;
typedef void (*lv_event_cb_t)(lv_event_t *event);

#define LV_TEXT_ALIGN_CENTER 0

static inline lv_color_t lv_color_hex(uint32_t value)
{
  return (lv_color_t)value;
}

void *lv_event_get_user_data(lv_event_t *event);
lv_obj_t *lv_obj_get_child(lv_obj_t *parent, int index);
void lv_label_set_text(lv_obj_t *label, const char *text);
uint32_t lv_tick_get(void);

#endif
