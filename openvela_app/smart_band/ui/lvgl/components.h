#ifndef SMART_BAND_UI_LVGL_COMPONENTS_H
#define SMART_BAND_UI_LVGL_COMPONENTS_H

#include "watch_model.h"

#include <lvgl/lvgl.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
  lv_coord_t screen_w;
  lv_coord_t screen_h;
} smart_band_ui_components_t;

void smart_band_ui_components_init(smart_band_ui_components_t *ui,
                                   lv_coord_t screen_w,
                                   lv_coord_t screen_h);
const lv_font_t *smart_band_ui_font_12(void);
const lv_font_t *smart_band_ui_font_14(void);
const lv_font_t *smart_band_ui_font_16(void);
const lv_font_t *smart_band_ui_font_20(void);
const lv_font_t *smart_band_ui_font_32(void);
const lv_font_t *smart_band_ui_font_time(void);
lv_coord_t smart_band_ui_sx(const smart_band_ui_components_t *ui, int value);
lv_coord_t smart_band_ui_sy(const smart_band_ui_components_t *ui, int value);
lv_coord_t smart_band_ui_min(lv_coord_t a, lv_coord_t b);
lv_coord_t smart_band_ui_max(lv_coord_t a, lv_coord_t b);
lv_coord_t smart_band_ui_abs(lv_coord_t value);
void smart_band_ui_strip_obj(lv_obj_t *obj);
lv_obj_t *smart_band_ui_create_box(lv_obj_t *parent, lv_coord_t x,
                                   lv_coord_t y, lv_coord_t w,
                                   lv_coord_t h, lv_color_t color,
                                   lv_coord_t radius);
lv_obj_t *smart_band_ui_create_label(lv_obj_t *parent, const char *text,
                                     const lv_font_t *font,
                                     lv_color_t color,
                                     lv_text_align_t align);
void smart_band_ui_place_label(lv_obj_t *label, lv_coord_t x, lv_coord_t y,
                               lv_coord_t w, lv_coord_t h);
void smart_band_ui_set_label_text(lv_obj_t *label, const char *text);
void smart_band_ui_set_label_text_fmt_int(lv_obj_t *label, const char *fmt,
                                          int value);
lv_obj_t *smart_band_ui_create_icon_image(lv_obj_t *parent,
                                          const lv_image_dsc_t *src,
                                          lv_coord_t x, lv_coord_t y,
                                          lv_coord_t size);
lv_obj_t *smart_band_ui_create_action_button(
  const smart_band_ui_components_t *ui, lv_obj_t *parent, const char *text,
  lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_color_t color,
  lv_event_cb_t cb, uintptr_t data);
const char *smart_band_ui_metric_source_text(const smart_band_state_t *model,
                                             smart_band_metric_t metric);
bool smart_band_ui_metric_available(const smart_band_state_t *model,
                                    smart_band_metric_t metric);
void smart_band_ui_format_temperature(const smart_band_state_t *model,
                                      char *buffer, size_t size);
void smart_band_ui_format_duration(char *buffer, size_t size, int seconds);
void smart_band_ui_set_temperature_label(const smart_band_state_t *model,
                                         lv_obj_t *label);
void smart_band_ui_format_watch_date(const smart_band_state_t *model,
                                     char *buffer, size_t size);
void smart_band_ui_split_time_text(const smart_band_state_t *model,
                                   char *hour, size_t hour_size,
                                   char *minute, size_t minute_size);

#endif
