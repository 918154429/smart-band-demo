#ifndef SMART_BAND_UI_LVGL_WATCH_FACE_PICKER_H
#define SMART_BAND_UI_LVGL_WATCH_FACE_PICKER_H

#include "components.h"
#include "smart_band_watch_face.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*smart_band_watch_face_picker_apply_cb_t)(
  void *context, smart_band_watch_face_id_t id);

typedef struct
{
  smart_band_ui_components_t ui;
  lv_obj_t *root;
  lv_obj_t *preview_card;
  lv_obj_t *preview_name;
  lv_obj_t *preview_position;
  lv_obj_t *preview_swatch_primary;
  lv_obj_t *preview_swatch_secondary;
  lv_obj_t *preview_swatch_accent;
  lv_obj_t *status;
  const smart_band_watch_face_descriptor_t *preview;
  smart_band_watch_face_picker_apply_cb_t apply_cb;
  void *apply_context;
  bool visible;
} smart_band_watch_face_picker_t;

/* The picker object must be zero-initialized before its first mount. */
int smart_band_watch_face_picker_mount(
  smart_band_watch_face_picker_t *picker, lv_obj_t *parent,
  const smart_band_ui_components_t *ui,
  smart_band_watch_face_id_t selected_id,
  smart_band_watch_face_picker_apply_cb_t apply_cb,
  void *apply_context);
void smart_band_watch_face_picker_unmount(
  smart_band_watch_face_picker_t *picker);
void smart_band_watch_face_picker_show(
  smart_band_watch_face_picker_t *picker,
  smart_band_watch_face_id_t selected_id);
void smart_band_watch_face_picker_hide(
  smart_band_watch_face_picker_t *picker);
bool smart_band_watch_face_picker_is_visible(
  const smart_band_watch_face_picker_t *picker);
lv_obj_t *smart_band_watch_face_picker_root(
  smart_band_watch_face_picker_t *picker);
void smart_band_watch_face_picker_preview_next(
  smart_band_watch_face_picker_t *picker);
void smart_band_watch_face_picker_preview_previous(
  smart_band_watch_face_picker_t *picker);
smart_band_watch_face_id_t smart_band_watch_face_picker_selected_id(
  const smart_band_watch_face_picker_t *picker);
void smart_band_watch_face_picker_set_status_message(
  smart_band_watch_face_picker_t *picker, const char *message);

#ifdef __cplusplus
}
#endif

#endif
