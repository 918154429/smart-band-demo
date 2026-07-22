#ifndef SMART_BAND_UI_LVGL_NOTIFICATION_VIEW_H
#define SMART_BAND_UI_LVGL_NOTIFICATION_VIEW_H

#include "components.h"
#include "smart_band_notification_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_NOTIFICATION_VIEW_ROWS 4u

typedef void (*smart_band_notification_view_action_cb_t)(
  void *context, uint32_t notification_id,
  smart_band_notification_command_t command);

struct smart_band_notification_view_s;

typedef struct
{
  struct smart_band_notification_view_s *view;
  uint32_t notification_id;
  smart_band_notification_command_t command;
} smart_band_notification_view_action_binding_t;

typedef struct
{
  lv_obj_t *card;
  lv_obj_t *title;
  lv_obj_t *body;
  lv_obj_t *read_button;
  lv_obj_t *delete_button;
  smart_band_notification_view_action_binding_t read_binding;
  smart_band_notification_view_action_binding_t delete_binding;
} smart_band_notification_view_row_t;

typedef struct smart_band_notification_view_s
{
  smart_band_ui_components_t ui;
  smart_band_notification_view_action_cb_t action_cb;
  void *action_context;

  lv_obj_t *presentation_root;
  lv_obj_t *overlay_layer;
  lv_obj_t *overlay_source;
  lv_obj_t *overlay_title;
  lv_obj_t *overlay_body;
  lv_obj_t *overlay_dismiss_button;
  lv_obj_t *overlay_accept_button;
  lv_obj_t *overlay_reject_button;
  lv_obj_t *call_layer;
  lv_obj_t *call_source;
  lv_obj_t *call_title;
  lv_obj_t *call_body;
  lv_obj_t *call_accept_button;
  lv_obj_t *call_reject_button;

  lv_obj_t *center_root;
  lv_obj_t *center_title;
  lv_obj_t *center_count;
  lv_obj_t *center_empty;
  lv_obj_t *center_page;
  lv_obj_t *center_previous_button;
  lv_obj_t *center_next_button;
  const smart_band_notification_model_t *center_model;
  smart_band_notification_view_row_t
    center_rows[SMART_BAND_NOTIFICATION_VIEW_ROWS];

  smart_band_notification_view_action_binding_t overlay_dismiss_binding;
  smart_band_notification_view_action_binding_t overlay_accept_binding;
  smart_band_notification_view_action_binding_t overlay_reject_binding;
  smart_band_notification_view_action_binding_t call_accept_binding;
  smart_band_notification_view_action_binding_t call_reject_binding;

  uint32_t active_notification_id;
  uint32_t active_generation;
  size_t center_page_index;
  smart_band_notification_type_t active_type;
  bool presentation_mounted;
  bool center_mounted;
  bool call_visible;
} smart_band_notification_view_t;

/* The view must be zero-initialized before its first mount. */
int smart_band_notification_view_mount(
  smart_band_notification_view_t *view, lv_obj_t *presentation_parent,
  const smart_band_ui_components_t *ui,
  smart_band_notification_view_action_cb_t action_cb, void *action_context);
void smart_band_notification_view_unmount(
  smart_band_notification_view_t *view);

int smart_band_notification_view_mount_center(
  smart_band_notification_view_t *view, lv_obj_t *parent);
void smart_band_notification_view_unmount_center(
  smart_band_notification_view_t *view);
void smart_band_notification_view_render_center(
  smart_band_notification_view_t *view,
  const smart_band_notification_model_t *model);

bool smart_band_notification_view_render_presentation(
  smart_band_notification_view_t *view,
  smart_band_notification_service_t *service);
bool smart_band_notification_view_captures_input(
  const smart_band_notification_view_t *view);

lv_obj_t *smart_band_notification_view_center_root(
  smart_band_notification_view_t *view);
lv_obj_t *smart_band_notification_view_presentation_root(
  smart_band_notification_view_t *view);

#ifdef __cplusplus
}
#endif

#endif
