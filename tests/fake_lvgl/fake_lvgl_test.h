#ifndef SMART_BAND_FAKE_LVGL_TEST_H
#define SMART_BAND_FAKE_LVGL_TEST_H

#include <lvgl/lvgl.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void fake_lvgl_reset(void);
size_t fake_lvgl_live_object_count(void);
size_t fake_lvgl_live_event_count(void);
size_t fake_lvgl_live_timer_count(void);
size_t fake_lvgl_object_create_attempts(void);
size_t fake_lvgl_timer_create_attempts(void);
void fake_lvgl_fail_object_create_at(size_t nth);
void fake_lvgl_fail_timer_create_at(size_t nth);
const char *fake_lvgl_obj_text(const lv_obj_t *object);
bool fake_lvgl_obj_has_flag(const lv_obj_t *object, uint32_t flag);
lv_obj_t *fake_lvgl_obj_parent(const lv_obj_t *object);
lv_obj_t *fake_lvgl_find_text(lv_obj_t *subtree, const char *text,
                              size_t occurrence);
void fake_lvgl_set_pointer(lv_coord_t x, lv_coord_t y);
void fake_lvgl_send_event(lv_obj_t *object, lv_event_code_t code);
void fake_lvgl_set_tick(uint32_t tick);
size_t fake_lvgl_advance_tick(uint32_t delta_ms);

#endif
