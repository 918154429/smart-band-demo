#ifndef SMART_BAND_APP_LVGL_H
#define SMART_BAND_APP_LVGL_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

int smart_band_lvgl_create(lv_obj_t *parent);
void smart_band_lvgl_destroy(void);

#ifdef __cplusplus
}
#endif

#endif
