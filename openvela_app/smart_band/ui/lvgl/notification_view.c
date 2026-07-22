#include "notification_view.h"

#include <stdio.h>
#include <string.h>

static lv_coord_t sx(const smart_band_notification_view_t *view, int value)
{
  return smart_band_ui_sx(&view->ui, value);
}

static lv_coord_t sy(const smart_band_notification_view_t *view, int value)
{
  return smart_band_ui_sy(&view->ui, value);
}

static lv_coord_t max_coord(lv_coord_t left, lv_coord_t right)
{
  return smart_band_ui_max(left, right);
}

static void set_visible(lv_obj_t *object, bool visible)
{
  if (object == NULL)
    {
      return;
    }

  if (visible)
    {
      lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_text(lv_obj_t *label, const char *text)
{
  smart_band_ui_set_label_text(label, text == NULL ? "" : text);
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, lv_color_t color,
                              lv_text_align_t align, lv_coord_t x,
                              lv_coord_t y, lv_coord_t width,
                              lv_coord_t height)
{
  lv_obj_t *label = smart_band_ui_create_label(parent, text, font, color,
                                                align);

  if (label != NULL)
    {
      smart_band_ui_place_label(label, x, y, width, height);
    }

  return label;
}

static lv_obj_t *create_layer(lv_obj_t *parent, lv_coord_t width,
                              lv_coord_t height, lv_color_t color,
                              lv_opa_t opacity)
{
  lv_obj_t *layer = lv_obj_create(parent);

  if (layer != NULL)
    {
      smart_band_ui_strip_obj(layer);
      lv_obj_set_pos(layer, 0, 0);
      lv_obj_set_size(layer, width, height);
      lv_obj_set_style_bg_color(layer, color, 0);
      lv_obj_set_style_bg_opa(layer, opacity, 0);
    }

  return layer;
}

static void action_cb(lv_event_t *event)
{
  smart_band_notification_view_action_binding_t *binding =
    (smart_band_notification_view_action_binding_t *)
      lv_event_get_user_data(event);
  smart_band_notification_view_t *view;

  if (binding == NULL || binding->view == NULL ||
      binding->notification_id == 0u)
    {
      return;
    }

  view = binding->view;
  if (view->action_cb != NULL)
    {
      view->action_cb(view->action_context, binding->notification_id,
                      binding->command);
    }
}

static void previous_page_cb(lv_event_t *event)
{
  smart_band_notification_view_t *view =
    (smart_band_notification_view_t *)lv_event_get_user_data(event);

  if (view != NULL && view->center_mounted &&
      view->center_page_index > 0u)
    {
      view->center_page_index--;
      smart_band_notification_view_render_center(view, view->center_model);
    }
}

static void next_page_cb(lv_event_t *event)
{
  smart_band_notification_view_t *view =
    (smart_band_notification_view_t *)lv_event_get_user_data(event);

  if (view != NULL && view->center_mounted)
    {
      view->center_page_index++;
      smart_band_notification_view_render_center(view, view->center_model);
    }
}

static void initialize_binding(
  smart_band_notification_view_action_binding_t *binding,
  smart_band_notification_view_t *view,
  smart_band_notification_command_t command)
{
  binding->view = view;
  binding->notification_id = 0u;
  binding->command = command;
}

static int create_overlay(smart_band_notification_view_t *view,
                          lv_coord_t width)
{
  lv_coord_t margin = max_coord(sx(view, 12), 6);
  lv_coord_t card_width = width - margin * 2;
  lv_coord_t card_height = max_coord(sy(view, 150), 104);
  lv_coord_t card_y = max_coord(sy(view, 16), 8);
  lv_coord_t button_height = max_coord(sy(view, 34), 24);
  lv_coord_t button_width = max_coord(sx(view, 78), 52);
  lv_coord_t button_gap = max_coord(sx(view, 8), 4);

  view->overlay_layer = smart_band_ui_create_box(
    view->presentation_root, margin, card_y, card_width, card_height,
    lv_color_hex(0xffffff), max_coord(sx(view, 18), 8));
  if (view->overlay_layer == NULL)
    {
      return -1;
    }

  /* Consume taps inside the card so they cannot activate controls beneath
   * it. The transparent area outside the card intentionally remains
   * non-blocking, including while a workout is active. */
  lv_obj_add_flag(view->overlay_layer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_border_width(view->overlay_layer, 1, 0);
  lv_obj_set_style_border_color(view->overlay_layer,
                                lv_color_hex(0xbfe2dc), 0);
  lv_obj_set_style_shadow_width(view->overlay_layer,
                                max_coord(sx(view, 8), 3), 0);
  lv_obj_set_style_shadow_color(view->overlay_layer,
                                lv_color_hex(0x314856), 0);
  lv_obj_set_style_shadow_opa(view->overlay_layer, LV_OPA_20, 0);
  view->overlay_source = create_label(
    view->overlay_layer, "Notification", smart_band_ui_font_12(),
    lv_color_hex(0x71858d), LV_TEXT_ALIGN_LEFT, margin, sy(view, 8),
    card_width - margin * 2, max_coord(sy(view, 18), 14));
  view->overlay_title = create_label(
    view->overlay_layer, "", smart_band_ui_font_16(),
    lv_color_hex(0x293b53), LV_TEXT_ALIGN_LEFT, margin, sy(view, 30),
    card_width - margin * 2, max_coord(sy(view, 24), 18));
  view->overlay_body = create_label(
    view->overlay_layer, "", smart_band_ui_font_12(),
    lv_color_hex(0x465c65), LV_TEXT_ALIGN_LEFT, margin, sy(view, 58),
    card_width - margin * 2,
    max_coord(card_height - sy(view, 102), 18));
  view->overlay_dismiss_button = smart_band_ui_create_action_button(
    &view->ui, view->overlay_layer, "Dismiss",
    card_width - margin - button_width, card_height - margin - button_height,
    button_width, button_height, lv_color_hex(0x6f8790), action_cb,
    (uintptr_t)&view->overlay_dismiss_binding);
  view->overlay_reject_button = smart_band_ui_create_action_button(
    &view->ui, view->overlay_layer, "Reject",
    card_width - margin - button_width * 2 - button_gap,
    card_height - margin - button_height, button_width, button_height,
    lv_color_hex(0xc45656), action_cb,
    (uintptr_t)&view->overlay_reject_binding);
  view->overlay_accept_button = smart_band_ui_create_action_button(
    &view->ui, view->overlay_layer, "Accept",
    card_width - margin - button_width, card_height - margin - button_height,
    button_width, button_height, lv_color_hex(0x49a89f), action_cb,
    (uintptr_t)&view->overlay_accept_binding);
  if (view->overlay_source == NULL || view->overlay_title == NULL ||
      view->overlay_body == NULL ||
      view->overlay_dismiss_button == NULL ||
      view->overlay_reject_button == NULL ||
      view->overlay_accept_button == NULL)
    {
      return -1;
    }

  set_visible(view->overlay_layer, false);
  return 0;
}

static int create_call(smart_band_notification_view_t *view,
                       lv_coord_t width, lv_coord_t height)
{
  lv_coord_t margin = max_coord(sx(view, 22), 10);
  lv_coord_t button_gap = max_coord(sx(view, 12), 6);
  lv_coord_t button_width = (width - margin * 2 - button_gap) / 2;
  lv_coord_t button_height = max_coord(sy(view, 58), 36);
  lv_coord_t button_y = height - margin - button_height;

  view->call_layer = create_layer(view->presentation_root, width, height,
                                  lv_color_hex(0x17333a), LV_OPA_COVER);
  if (view->call_layer == NULL)
    {
      return -1;
    }

  lv_obj_add_flag(view->call_layer, LV_OBJ_FLAG_CLICKABLE);
  view->call_source = create_label(
    view->call_layer, "Incoming call", smart_band_ui_font_14(),
    lv_color_hex(0xaedbd5), LV_TEXT_ALIGN_CENTER, margin, sy(view, 74),
    width - margin * 2, max_coord(sy(view, 24), 18));
  view->call_title = create_label(
    view->call_layer, "", smart_band_ui_font_32(),
    lv_color_hex(0xffffff), LV_TEXT_ALIGN_CENTER, margin, sy(view, 126),
    width - margin * 2, max_coord(sy(view, 54), 32));
  view->call_body = create_label(
    view->call_layer, "", smart_band_ui_font_14(),
    lv_color_hex(0xd8e9e7), LV_TEXT_ALIGN_CENTER, margin, sy(view, 192),
    width - margin * 2, max_coord(sy(view, 70), 34));
  view->call_reject_button = smart_band_ui_create_action_button(
    &view->ui, view->call_layer, "Reject", margin, button_y, button_width,
    button_height, lv_color_hex(0xc45656), action_cb,
    (uintptr_t)&view->call_reject_binding);
  view->call_accept_button = smart_band_ui_create_action_button(
    &view->ui, view->call_layer, "Accept",
    margin + button_width + button_gap, button_y, button_width,
    button_height, lv_color_hex(0x49a89f), action_cb,
    (uintptr_t)&view->call_accept_binding);
  if (view->call_source == NULL || view->call_title == NULL ||
      view->call_body == NULL || view->call_reject_button == NULL ||
      view->call_accept_button == NULL)
    {
      return -1;
    }

  set_visible(view->call_layer, false);
  return 0;
}

int smart_band_notification_view_mount(
  smart_band_notification_view_t *view, lv_obj_t *presentation_parent,
  const smart_band_ui_components_t *ui,
  smart_band_notification_view_action_cb_t action_handler,
  void *action_context)
{
  lv_coord_t width;
  lv_coord_t height;

  if (view == NULL || presentation_parent == NULL || ui == NULL ||
      ui->screen_w <= 0 || ui->screen_h <= 0)
    {
      return -1;
    }

  smart_band_notification_view_unmount(view);
  view->ui = *ui;
  view->action_cb = action_handler;
  view->action_context = action_context;
  initialize_binding(&view->overlay_dismiss_binding, view,
                     SMART_BAND_NOTIFICATION_COMMAND_DISMISS);
  initialize_binding(&view->overlay_accept_binding, view,
                     SMART_BAND_NOTIFICATION_COMMAND_ACCEPT);
  initialize_binding(&view->overlay_reject_binding, view,
                     SMART_BAND_NOTIFICATION_COMMAND_REJECT);
  initialize_binding(&view->call_accept_binding, view,
                     SMART_BAND_NOTIFICATION_COMMAND_ACCEPT);
  initialize_binding(&view->call_reject_binding, view,
                     SMART_BAND_NOTIFICATION_COMMAND_REJECT);

  width = lv_obj_get_width(presentation_parent);
  height = lv_obj_get_height(presentation_parent);
  if (width <= 0 || height <= 0)
    {
      smart_band_notification_view_unmount(view);
      return -1;
    }

  view->presentation_root = create_layer(
    presentation_parent, width, height, lv_color_hex(0x000000),
    LV_OPA_TRANSP);
  if (view->presentation_root == NULL ||
      create_overlay(view, width) != 0 ||
      create_call(view, width, height) != 0)
    {
      smart_band_notification_view_unmount(view);
      return -1;
    }

  view->presentation_mounted = true;
  return 0;
}

void smart_band_notification_view_unmount_center(
  smart_band_notification_view_t *view)
{
  size_t index;

  if (view == NULL)
    {
      return;
    }

  if (view->center_root != NULL && lv_obj_is_valid(view->center_root))
    {
      lv_obj_del(view->center_root);
    }

  view->center_root = NULL;
  view->center_title = NULL;
  view->center_count = NULL;
  view->center_empty = NULL;
  view->center_page = NULL;
  view->center_previous_button = NULL;
  view->center_next_button = NULL;
  view->center_model = NULL;
  for (index = 0u; index < SMART_BAND_NOTIFICATION_VIEW_ROWS; index++)
    {
      memset(&view->center_rows[index], 0,
             sizeof(view->center_rows[index]));
    }
  view->center_page_index = 0u;
  view->center_mounted = false;
}

void smart_band_notification_view_unmount(
  smart_band_notification_view_t *view)
{
  if (view == NULL)
    {
      return;
    }

  smart_band_notification_view_unmount_center(view);
  if (view->presentation_root != NULL &&
      lv_obj_is_valid(view->presentation_root))
    {
      lv_obj_del(view->presentation_root);
    }

  memset(view, 0, sizeof(*view));
}

static int create_center_row(smart_band_notification_view_t *view,
                             size_t index, lv_coord_t width,
                             lv_coord_t y, lv_coord_t height)
{
  smart_band_notification_view_row_t *row = &view->center_rows[index];
  lv_coord_t margin = max_coord(sx(view, 8), 4);
  lv_coord_t gap = max_coord(sx(view, 5), 3);
  lv_coord_t button_width = max_coord(sx(view, 54), 40);
  lv_coord_t button_height = max_coord(sy(view, 28), 22);
  lv_coord_t text_width = width - margin * 3 - button_width * 2 - gap;

  if (text_width < 40)
    {
      text_width = width - margin * 2;
    }
  initialize_binding(&row->read_binding, view,
                     SMART_BAND_NOTIFICATION_COMMAND_READ);
  initialize_binding(&row->delete_binding, view,
                     SMART_BAND_NOTIFICATION_COMMAND_DELETE);
  row->card = smart_band_ui_create_box(
    view->center_root, margin, y, width - margin * 2, height,
    lv_color_hex(0xf5faf9), max_coord(sx(view, 10), 4));
  if (row->card == NULL)
    {
      return -1;
    }

  lv_obj_set_style_border_width(row->card, 1, 0);
  lv_obj_set_style_border_color(row->card, lv_color_hex(0xdbe9e6), 0);
  row->title = create_label(
    row->card, "", smart_band_ui_font_14(), lv_color_hex(0x293b53),
    LV_TEXT_ALIGN_LEFT, margin, sy(view, 5), text_width,
    max_coord(sy(view, 20), 16));
  row->body = create_label(
    row->card, "", smart_band_ui_font_12(), lv_color_hex(0x71858d),
    LV_TEXT_ALIGN_LEFT, margin, sy(view, 29), text_width,
    max_coord(height - sy(view, 35), 14));
  row->read_button = smart_band_ui_create_action_button(
    &view->ui, row->card, "Read",
    width - margin * 3 - button_width * 2 - gap,
    (height - button_height) / 2, button_width, button_height,
    lv_color_hex(0x49a89f), action_cb,
    (uintptr_t)&row->read_binding);
  row->delete_button = smart_band_ui_create_action_button(
    &view->ui, row->card, "Delete", width - margin * 2 - button_width,
    (height - button_height) / 2, button_width, button_height,
    lv_color_hex(0xc45656), action_cb,
    (uintptr_t)&row->delete_binding);
  if (row->title == NULL || row->body == NULL ||
      row->read_button == NULL || row->delete_button == NULL)
    {
      return -1;
    }

  return 0;
}

int smart_band_notification_view_mount_center(
  smart_band_notification_view_t *view, lv_obj_t *parent)
{
  lv_coord_t width;
  lv_coord_t height;
  lv_coord_t margin;
  lv_coord_t header_height;
  lv_coord_t footer_height;
  lv_coord_t row_gap;
  lv_coord_t row_height;
  lv_coord_t row_y;
  lv_coord_t nav_width;
  lv_coord_t nav_height;
  size_t index;

  if (view == NULL || parent == NULL || !view->presentation_mounted)
    {
      return -1;
    }

  smart_band_notification_view_unmount_center(view);
  width = lv_obj_get_width(parent);
  height = lv_obj_get_height(parent);
  if (width <= 0 || height <= 0)
    {
      return -1;
    }

  view->center_root = create_layer(parent, width, height,
                                   lv_color_hex(0xffffff), LV_OPA_TRANSP);
  if (view->center_root == NULL)
    {
      smart_band_notification_view_unmount_center(view);
      return -1;
    }

  margin = max_coord(sx(view, 8), 4);
  header_height = max_coord(sy(view, 34), 26);
  footer_height = max_coord(sy(view, 38), 28);
  row_gap = max_coord(sy(view, 5), 3);
  row_height = (height - header_height - footer_height -
                row_gap * (SMART_BAND_NOTIFICATION_VIEW_ROWS - 1u)) /
               SMART_BAND_NOTIFICATION_VIEW_ROWS;
  if (row_height < 42)
    {
      smart_band_notification_view_unmount_center(view);
      return -1;
    }

  view->center_title = create_label(
    view->center_root, "Notifications", smart_band_ui_font_16(),
    lv_color_hex(0x293b53), LV_TEXT_ALIGN_LEFT, margin, 0,
    width - margin * 2, header_height);
  view->center_count = create_label(
    view->center_root, "0 items", smart_band_ui_font_12(),
    lv_color_hex(0x71858d), LV_TEXT_ALIGN_RIGHT, margin, 0,
    width - margin * 2, header_height);
  row_y = header_height;
  for (index = 0u; index < SMART_BAND_NOTIFICATION_VIEW_ROWS; index++)
    {
      if (create_center_row(view, index, width, row_y, row_height) != 0)
        {
          smart_band_notification_view_unmount_center(view);
          return -1;
        }
      row_y += row_height + row_gap;
    }

  view->center_empty = create_label(
    view->center_root, "No notifications", smart_band_ui_font_14(),
    lv_color_hex(0x71858d), LV_TEXT_ALIGN_CENTER, margin, header_height,
    width - margin * 2, height - header_height - footer_height);
  nav_width = max_coord(sx(view, 72), 52);
  nav_height = max_coord(sy(view, 30), 24);
  view->center_previous_button = smart_band_ui_create_action_button(
    &view->ui, view->center_root, "Previous", margin,
    height - nav_height, nav_width, nav_height, lv_color_hex(0x6f8790),
    previous_page_cb, (uintptr_t)view);
  view->center_next_button = smart_band_ui_create_action_button(
    &view->ui, view->center_root, "Next", width - margin - nav_width,
    height - nav_height, nav_width, nav_height, lv_color_hex(0x6f8790),
    next_page_cb, (uintptr_t)view);
  view->center_page = create_label(
    view->center_root, "1 / 1", smart_band_ui_font_12(),
    lv_color_hex(0x71858d), LV_TEXT_ALIGN_CENTER,
    margin + nav_width, height - nav_height,
    width - margin * 2 - nav_width * 2, nav_height);
  if (view->center_title == NULL || view->center_count == NULL ||
      view->center_empty == NULL || view->center_previous_button == NULL ||
      view->center_next_button == NULL || view->center_page == NULL)
    {
      smart_band_notification_view_unmount_center(view);
      return -1;
    }

  view->center_mounted = true;
  return 0;
}

static void set_button_text(lv_obj_t *button, const char *text)
{
  lv_obj_t *label = button == NULL ? NULL : lv_obj_get_child(button, 0);

  if (label != NULL)
    {
      lv_label_set_text(label, text);
    }
}

void smart_band_notification_view_render_center(
  smart_band_notification_view_t *view,
  const smart_band_notification_model_t *model)
{
  size_t count;
  size_t page_count;
  size_t index;
  char text[96];

  if (view == NULL || model == NULL || !view->center_mounted)
    {
      return;
    }

  view->center_model = model;
  count = smart_band_notification_count(model);
  page_count = count == 0u ? 1u :
    (count + SMART_BAND_NOTIFICATION_VIEW_ROWS - 1u) /
      SMART_BAND_NOTIFICATION_VIEW_ROWS;
  if (view->center_page_index >= page_count)
    {
      view->center_page_index = page_count - 1u;
    }

  (void)snprintf(text, sizeof(text), "%lu item%s", (unsigned long)count,
                 count == 1u ? "" : "s");
  set_text(view->center_count, text);
  (void)snprintf(text, sizeof(text), "%lu / %lu",
                 (unsigned long)(view->center_page_index + 1u),
                 (unsigned long)page_count);
  set_text(view->center_page, text);
  set_visible(view->center_empty, count == 0u);
  set_visible(view->center_previous_button, view->center_page_index > 0u);
  set_visible(view->center_next_button,
              view->center_page_index + 1u < page_count);

  for (index = 0u; index < SMART_BAND_NOTIFICATION_VIEW_ROWS; index++)
    {
      smart_band_notification_view_row_t *row = &view->center_rows[index];
      size_t offset = view->center_page_index *
                      SMART_BAND_NOTIFICATION_VIEW_ROWS + index;
      const smart_band_notification_t *notification = NULL;

      if (offset < count)
        {
          notification = smart_band_notification_at(model,
                                                     count - 1u - offset);
        }
      if (notification == NULL)
        {
          row->read_binding.notification_id = 0u;
          row->delete_binding.notification_id = 0u;
          set_visible(row->card, false);
          continue;
        }

      row->read_binding.notification_id = notification->id;
      row->delete_binding.notification_id = notification->id;
      (void)snprintf(text, sizeof(text), "%s%s / %s",
                     notification->read ? "" : "New: ",
                     notification->source, notification->title);
      set_text(row->title, text);
      set_text(row->body, notification->body);
      set_button_text(row->read_button,
                      notification->read ? "Read" : "Mark read");
      set_visible(row->read_button, !notification->read);
      set_visible(row->card, true);
    }
}

static void set_presentation_bindings(
  smart_band_notification_view_t *view, uint32_t notification_id)
{
  view->overlay_dismiss_binding.notification_id = notification_id;
  view->overlay_accept_binding.notification_id = notification_id;
  view->overlay_reject_binding.notification_id = notification_id;
  view->call_accept_binding.notification_id = notification_id;
  view->call_reject_binding.notification_id = notification_id;
}

static void hide_presentation(smart_band_notification_view_t *view)
{
  set_visible(view->overlay_layer, false);
  set_visible(view->call_layer, false);
  set_presentation_bindings(view, 0u);
  view->active_notification_id = 0u;
  view->active_generation = 0u;
  view->active_type = SMART_BAND_NOTIFICATION_TYPE_APP;
  view->call_visible = false;
}

static void render_overlay(smart_band_notification_view_t *view,
                           const smart_band_notification_t *notification)
{
  bool is_call = notification->type == SMART_BAND_NOTIFICATION_TYPE_CALL;

  set_text(view->overlay_source,
           is_call ? "Incoming call" : notification->source);
  set_text(view->overlay_title, notification->title);
  set_text(view->overlay_body, notification->body);
  set_visible(view->overlay_dismiss_button, !is_call);
  set_visible(view->overlay_accept_button, is_call);
  set_visible(view->overlay_reject_button, is_call);
  set_visible(view->call_layer, false);
  set_visible(view->overlay_layer, true);
  lv_obj_move_foreground(view->overlay_layer);
  view->call_visible = false;
}

static void render_call(smart_band_notification_view_t *view,
                        const smart_band_notification_t *notification)
{
  set_text(view->call_source, notification->source);
  set_text(view->call_title, notification->title);
  set_text(view->call_body, notification->body);
  set_visible(view->overlay_layer, false);
  set_visible(view->call_layer, true);
  lv_obj_move_foreground(view->call_layer);
  view->call_visible = true;
}

bool smart_band_notification_view_render_presentation(
  smart_band_notification_view_t *view,
  smart_band_notification_service_t *service)
{
  smart_band_notification_presentation_t presentation;
  const smart_band_notification_t *notification;
  uint32_t notification_id;
  uint32_t generation;
  uint32_t unacknowledged_id;
  uint32_t unacknowledged_generation;
  smart_band_notification_presentation_t unacknowledged_presentation;

  if (view == NULL || service == NULL || !view->presentation_mounted)
    {
      return false;
    }

  if (!smart_band_notification_service_get_active_presentation(
        service, &notification_id, &generation, &presentation))
    {
      hide_presentation(view);
      return true;
    }

  notification = smart_band_notification_find(&service->model,
                                               notification_id);
  if (notification == NULL || notification_id == 0u || generation == 0u ||
      presentation.center_only ||
      (!presentation.overlay && !presentation.full_screen))
    {
      hide_presentation(view);
      return false;
    }

  set_presentation_bindings(view, notification_id);
  view->active_notification_id = notification_id;
  view->active_generation = generation;
  view->active_type = notification->type;
  lv_obj_move_foreground(view->presentation_root);
  if (presentation.full_screen)
    {
      render_call(view, notification);
    }
  else
    {
      render_overlay(view, notification);
    }

  if (!smart_band_notification_service_peek_presentation(
        service, &unacknowledged_id, &unacknowledged_generation,
        &unacknowledged_presentation))
    {
      return true;
    }

  if (unacknowledged_id != notification_id ||
      unacknowledged_generation != generation)
    {
      return false;
    }

  return smart_band_notification_service_ack_presentation(
    service, notification_id, generation);
}

bool smart_band_notification_view_captures_input(
  const smart_band_notification_view_t *view)
{
  return view != NULL && view->presentation_mounted && view->call_visible;
}

lv_obj_t *smart_band_notification_view_center_root(
  smart_band_notification_view_t *view)
{
  return view == NULL ? NULL : view->center_root;
}

lv_obj_t *smart_band_notification_view_presentation_root(
  smart_band_notification_view_t *view)
{
  return view == NULL ? NULL : view->presentation_root;
}
