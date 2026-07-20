#include "components.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define DESIGN_W 330
#define DESIGN_H 626

void smart_band_ui_components_init(smart_band_ui_components_t *ui,
                                   lv_coord_t screen_w,
                                   lv_coord_t screen_h)
{
  if (ui != NULL)
    {
      ui->screen_w = screen_w;
      ui->screen_h = screen_h;
    }
}

#if LV_FONT_MONTSERRAT_12
const lv_font_t *smart_band_ui_font_12(void) { return &lv_font_montserrat_12; }
#else
const lv_font_t *smart_band_ui_font_12(void) { return LV_FONT_DEFAULT; }
#endif
#if LV_FONT_MONTSERRAT_14
const lv_font_t *smart_band_ui_font_14(void) { return &lv_font_montserrat_14; }
#else
const lv_font_t *smart_band_ui_font_14(void) { return LV_FONT_DEFAULT; }
#endif
#if LV_FONT_MONTSERRAT_16
const lv_font_t *smart_band_ui_font_16(void) { return &lv_font_montserrat_16; }
#else
const lv_font_t *smart_band_ui_font_16(void) { return LV_FONT_DEFAULT; }
#endif
#if LV_FONT_MONTSERRAT_20
const lv_font_t *smart_band_ui_font_20(void) { return &lv_font_montserrat_20; }
#else
const lv_font_t *smart_band_ui_font_20(void) { return LV_FONT_DEFAULT; }
#endif
#if LV_FONT_MONTSERRAT_32
const lv_font_t *smart_band_ui_font_32(void) { return &lv_font_montserrat_32; }
#else
const lv_font_t *smart_band_ui_font_32(void) { return smart_band_ui_font_20(); }
#endif

const lv_font_t *smart_band_ui_font_time(void)
{
#if LV_FONT_MONTSERRAT_48
  return &lv_font_montserrat_48;
#elif LV_FONT_MONTSERRAT_32
  return &lv_font_montserrat_32;
#else
  return LV_FONT_DEFAULT;
#endif
}

lv_coord_t smart_band_ui_sx(const smart_band_ui_components_t *ui, int value)
{
  return ui == NULL ? 0 : (lv_coord_t)((value * ui->screen_w) / DESIGN_W);
}

lv_coord_t smart_band_ui_sy(const smart_band_ui_components_t *ui, int value)
{
  return ui == NULL ? 0 : (lv_coord_t)((value * ui->screen_h) / DESIGN_H);
}

lv_coord_t smart_band_ui_min(lv_coord_t a, lv_coord_t b) { return a < b ? a : b; }
lv_coord_t smart_band_ui_max(lv_coord_t a, lv_coord_t b) { return a > b ? a : b; }
lv_coord_t smart_band_ui_abs(lv_coord_t value) { return value < 0 ? -value : value; }

void smart_band_ui_strip_obj(lv_obj_t *obj)
{
  lv_obj_remove_style_all(obj);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *smart_band_ui_create_box(lv_obj_t *parent, lv_coord_t x,
                                   lv_coord_t y, lv_coord_t w,
                                   lv_coord_t h, lv_color_t color,
                                   lv_coord_t radius)
{
  lv_obj_t *box = lv_obj_create(parent);
  if (box == NULL) return NULL;
  smart_band_ui_strip_obj(box);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_size(box, w, h);
  lv_obj_set_style_bg_color(box, color, 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(box, radius, 0);
  return box;
}

lv_obj_t *smart_band_ui_create_label(lv_obj_t *parent, const char *text,
                                     const lv_font_t *font,
                                     lv_color_t color,
                                     lv_text_align_t align)
{
  lv_obj_t *label = lv_label_create(parent);
  if (label == NULL) return NULL;
  smart_band_ui_strip_obj(label);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, align, 0);
  return label;
}

void smart_band_ui_place_label(lv_obj_t *label, lv_coord_t x, lv_coord_t y,
                               lv_coord_t w, lv_coord_t h)
{
  if (label != NULL) { lv_obj_set_pos(label, x, y); lv_obj_set_size(label, w, h); }
}

void smart_band_ui_set_label_text(lv_obj_t *label, const char *text)
{
  if (label != NULL) lv_label_set_text(label, text);
}

void smart_band_ui_set_label_text_fmt_int(lv_obj_t *label, const char *fmt,
                                          int value)
{
  char buffer[32];
  snprintf(buffer, sizeof(buffer), fmt, value);
  smart_band_ui_set_label_text(label, buffer);
}

lv_obj_t *smart_band_ui_create_icon_image(lv_obj_t *parent,
                                          const lv_image_dsc_t *src,
                                          lv_coord_t x, lv_coord_t y,
                                          lv_coord_t size)
{
  lv_obj_t *image;
  uint32_t scale;
  if (src == NULL || size <= 0) return NULL;
  image = lv_image_create(parent);
  if (image == NULL) return NULL;
  smart_band_ui_strip_obj(image);
  lv_image_set_src(image, src);
  scale = (uint32_t)((size * LV_SCALE_NONE) / 48);
  if (scale == 0) scale = 1;
  lv_image_set_scale(image, scale);
  lv_obj_set_pos(image, x, y);
  lv_obj_set_size(image, size, size);
  return image;
}

lv_obj_t *smart_band_ui_create_action_button(
  const smart_band_ui_components_t *ui, lv_obj_t *parent, const char *text,
  lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_color_t color,
  lv_event_cb_t cb, uintptr_t data)
{
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_t *label;
  if (button == NULL) return NULL;
  smart_band_ui_strip_obj(button);
  lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, w, h);
  lv_obj_set_style_bg_color(button, color, 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(button, smart_band_ui_sx(ui, 16), 0);
  lv_obj_set_style_shadow_width(button, smart_band_ui_sx(ui, 8), 0);
  lv_obj_set_style_shadow_color(button, lv_color_hex(0x314856), 0);
  lv_obj_set_style_shadow_opa(button, LV_OPA_20, 0);
  lv_obj_set_style_shadow_offset_y(button, smart_band_ui_sy(ui, 4), 0);
  lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, (void *)data);
  label = smart_band_ui_create_label(button, text, smart_band_ui_font_14(),
                                     lv_color_hex(0xffffff),
                                     LV_TEXT_ALIGN_CENTER);
  if (label == NULL) return NULL;
  lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(label, cb, LV_EVENT_CLICKED, (void *)data);
  smart_band_ui_place_label(label, smart_band_ui_sx(ui, 4),
                            (h - smart_band_ui_sy(ui, 20)) / 2,
                            w - smart_band_ui_sx(ui, 8),
                            smart_band_ui_sy(ui, 22));
  return button;
}

const char *smart_band_ui_metric_source_text(const smart_band_state_t *model,
                                             smart_band_metric_t metric)
{
  const smart_band_metric_info_t *info = smart_band_state_metric_info(model, metric);
  if (info == NULL || info->freshness == SMART_BAND_DATA_FRESHNESS_UNAVAILABLE) return "Unavailable";
  if (info->source == SMART_BAND_DATA_SOURCE_SIMULATED) return "Model";
  if (info->source == SMART_BAND_DATA_SOURCE_SENSOR_DERIVED)
    return info->freshness == SMART_BAND_DATA_FRESHNESS_STALE ?
           "Derived stale" : "Derived";
  return info->source == SMART_BAND_DATA_SOURCE_SENSOR ?
         (info->freshness == SMART_BAND_DATA_FRESHNESS_STALE ?
          "Sensor stale" : "Sensor") : "Unavailable";
}

bool smart_band_ui_metric_available(const smart_band_state_t *model,
                                    smart_band_metric_t metric)
{
  const smart_band_metric_info_t *info = smart_band_state_metric_info(model, metric);
  return info != NULL && info->freshness != SMART_BAND_DATA_FRESHNESS_UNAVAILABLE;
}

void smart_band_ui_format_temperature(const smart_band_state_t *model,
                                      char *buffer, size_t size)
{
  const smart_band_metric_info_t *info = smart_band_state_metric_info(model, SMART_BAND_METRIC_TEMPERATURE);
  const char *suffix = "";
  if (info == NULL || info->freshness == SMART_BAND_DATA_FRESHNESS_UNAVAILABLE)
    { snprintf(buffer, size, "--"); return; }
  if (info->source == SMART_BAND_DATA_SOURCE_SIMULATED) suffix = " sim";
  else if (info->freshness == SMART_BAND_DATA_FRESHNESS_STALE) suffix = " stale";
  snprintf(buffer, size, "%d%s%s", model->temperature_c,
           "\xC2\xB0" "C", suffix);
}

void smart_band_ui_format_duration(char *buffer, size_t size, int seconds)
{
  if (seconds < 0) seconds = 0;
  snprintf(buffer, size, "%02d:%02d", seconds / 60, seconds % 60);
}

void smart_band_ui_set_temperature_label(const smart_band_state_t *model,
                                         lv_obj_t *label)
{
  char value[16];
  smart_band_ui_format_temperature(model, value, sizeof(value));
  smart_band_ui_set_label_text(label, value);
}

void smart_band_ui_format_watch_date(const smart_band_state_t *model,
                                     char *buffer, size_t size)
{
  static const char *const weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  static const char *const months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  struct tm local_now;
  time_t now = time(NULL);
  if (!smart_band_display_time(now, &local_now))
    { snprintf(buffer, size, "%s", model->date_text); return; }
  snprintf(buffer, size, "%s %02d %s", weekdays[local_now.tm_wday],
           local_now.tm_mday, months[local_now.tm_mon]);
}

void smart_band_ui_split_time_text(const smart_band_state_t *model,
                                   char *hour, size_t hour_size,
                                   char *minute, size_t minute_size)
{
  if (strlen(model->time_text) >= 5 && model->time_text[2] == ':')
    {
      snprintf(hour, hour_size, "%c%c", model->time_text[0], model->time_text[1]);
      snprintf(minute, minute_size, "%c%c", model->time_text[3], model->time_text[4]);
      return;
    }
  snprintf(hour, hour_size, "--");
  snprintf(minute, minute_size, "--");
}
