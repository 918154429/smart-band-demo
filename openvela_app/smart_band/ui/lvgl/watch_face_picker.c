#include "watch_face_picker.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
  lv_color_t primary;
  lv_color_t secondary;
  lv_color_t accent;
} smart_band_watch_face_picker_palette_t;

static const smart_band_watch_face_descriptor_t *find_initial_face(
  smart_band_watch_face_id_t id)
{
  const smart_band_watch_face_descriptor_t *descriptor =
    smart_band_watch_face_registry_find(id);

  if (descriptor == NULL)
    {
      descriptor = smart_band_watch_face_registry_default();
    }

  if (descriptor == NULL)
    {
      descriptor = smart_band_watch_face_registry_at(0);
    }

  return descriptor;
}

static smart_band_watch_face_picker_palette_t face_palette(
  smart_band_watch_face_id_t id)
{
  smart_band_watch_face_picker_palette_t palette;

  switch (id)
    {
      case SMART_BAND_WATCH_FACE_ACTIVITY:
        palette.primary = lv_color_hex(0x111619);
        palette.secondary = lv_color_hex(0x7ed5ca);
        palette.accent = lv_color_hex(0xffb85c);
        break;

      case SMART_BAND_WATCH_FACE_MINIMAL:
        palette.primary = lv_color_hex(0xf7f9f8);
        palette.secondary = lv_color_hex(0x3a6d75);
        palette.accent = lv_color_hex(0xf3cf54);
        break;

      case SMART_BAND_WATCH_FACE_LOTUS:
      default:
        palette.primary = lv_color_hex(0xfffcf6);
        palette.secondary = lv_color_hex(0x79c5be);
        palette.accent = lv_color_hex(0xe5a5a8);
        break;
    }

  return palette;
}

static size_t descriptor_index(
  const smart_band_watch_face_descriptor_t *descriptor)
{
  size_t index;

  for (index = 0; index < smart_band_watch_face_registry_count(); index++)
    {
      if (smart_band_watch_face_registry_at(index) == descriptor)
        {
          return index;
        }
    }

  return 0;
}

static void update_preview(smart_band_watch_face_picker_t *picker)
{
  smart_band_watch_face_picker_palette_t palette;
  size_t count;
  size_t index;
  char position[32];

  if (picker == NULL || picker->preview == NULL)
    {
      return;
    }

  count = smart_band_watch_face_registry_count();
  index = descriptor_index(picker->preview);
  palette = face_palette(picker->preview->id);
  snprintf(position, sizeof(position), "Face %u of %u",
           (unsigned int)(index + 1), (unsigned int)count);

  smart_band_ui_set_label_text(picker->preview_name,
                               picker->preview->name != NULL ?
                               picker->preview->name : "Watch face");
  smart_band_ui_set_label_text(picker->preview_position, position);
  lv_obj_set_style_bg_color(picker->preview_card, palette.primary, 0);
  lv_obj_set_style_bg_color(picker->preview_swatch_primary,
                            palette.primary, 0);
  lv_obj_set_style_bg_color(picker->preview_swatch_secondary,
                            palette.secondary, 0);
  lv_obj_set_style_bg_color(picker->preview_swatch_accent,
                            palette.accent, 0);
  lv_obj_set_style_text_color(
    picker->preview_name,
    picker->preview->id == SMART_BAND_WATCH_FACE_ACTIVITY ?
    lv_color_hex(0xf4f7f8) : lv_color_hex(0x182225), 0);
  lv_obj_set_style_text_color(
    picker->preview_position,
    picker->preview->id == SMART_BAND_WATCH_FACE_ACTIVITY ?
    lv_color_hex(0xa7b2b8) : lv_color_hex(0x526167), 0);
}

void smart_band_watch_face_picker_set_status_message(
  smart_band_watch_face_picker_t *picker, const char *message)
{
  if (picker != NULL)
    {
      smart_band_ui_set_label_text(picker->status,
                                   message != NULL ? message : "");
    }
}

void smart_band_watch_face_picker_preview_next(
  smart_band_watch_face_picker_t *picker)
{
  size_t count;
  size_t index;

  if (picker == NULL || picker->root == NULL || picker->preview == NULL)
    {
      return;
    }

  count = smart_band_watch_face_registry_count();
  if (count == 0)
    {
      return;
    }

  index = (descriptor_index(picker->preview) + 1) % count;
  picker->preview = smart_band_watch_face_registry_at(index);
  smart_band_watch_face_picker_set_status_message(picker, NULL);
  update_preview(picker);
}

void smart_band_watch_face_picker_preview_previous(
  smart_band_watch_face_picker_t *picker)
{
  size_t count;
  size_t index;

  if (picker == NULL || picker->root == NULL || picker->preview == NULL)
    {
      return;
    }

  count = smart_band_watch_face_registry_count();
  if (count == 0)
    {
      return;
    }

  index = descriptor_index(picker->preview);
  index = index == 0 ? count - 1 : index - 1;
  picker->preview = smart_band_watch_face_registry_at(index);
  smart_band_watch_face_picker_set_status_message(picker, NULL);
  update_preview(picker);
}

static void previous_cb(lv_event_t *event)
{
  smart_band_watch_face_picker_preview_previous(
    (smart_band_watch_face_picker_t *)lv_event_get_user_data(event));
}

static void next_cb(lv_event_t *event)
{
  smart_band_watch_face_picker_preview_next(
    (smart_band_watch_face_picker_t *)lv_event_get_user_data(event));
}

static void apply_cb(lv_event_t *event)
{
  smart_band_watch_face_picker_t *picker =
    (smart_band_watch_face_picker_t *)lv_event_get_user_data(event);

  if (picker != NULL && picker->preview != NULL &&
      picker->apply_cb != NULL)
    {
      picker->apply_cb(picker->apply_context, picker->preview->id);
    }
}

static void cancel_cb(lv_event_t *event)
{
  smart_band_watch_face_picker_hide(
    (smart_band_watch_face_picker_t *)lv_event_get_user_data(event));
}

static int create_picker_content(smart_band_watch_face_picker_t *picker)
{
  const smart_band_ui_components_t *ui = &picker->ui;
  lv_coord_t margin = smart_band_ui_sx(ui, 20);
  lv_coord_t gap = smart_band_ui_sx(ui, 10);
  lv_coord_t content_width = ui->screen_w - margin * 2;
  lv_coord_t half_width = (content_width - gap) / 2;
  lv_coord_t preview_y = smart_band_ui_sy(ui, 70);
  lv_coord_t preview_h = smart_band_ui_sy(ui, 260);
  lv_coord_t navigation_y = smart_band_ui_sy(ui, 350);
  lv_coord_t button_h = smart_band_ui_sy(ui, 52);
  lv_coord_t action_y = ui->screen_h - smart_band_ui_sy(ui, 72);
  lv_coord_t swatch_size = smart_band_ui_sx(ui, 38);
  lv_coord_t swatch_y = preview_y + preview_h - smart_band_ui_sy(ui, 68);
  lv_coord_t swatch_gap = smart_band_ui_sx(ui, 12);
  lv_coord_t swatch_start =
    (ui->screen_w - swatch_size * 3 - swatch_gap * 2) / 2;
  lv_obj_t *title;
  lv_obj_t *caption;
  lv_obj_t *previous;
  lv_obj_t *next;
  lv_obj_t *apply;
  lv_obj_t *cancel;

  title = smart_band_ui_create_label(
    picker->root, "Watch faces", smart_band_ui_font_20(),
    lv_color_hex(0xf5f7f7), LV_TEXT_ALIGN_CENTER);
  picker->preview_card = smart_band_ui_create_box(
    picker->root, margin, preview_y, content_width, preview_h,
    lv_color_hex(0xfffcf6), smart_band_ui_sx(ui, 12));
  if (title == NULL || picker->preview_card == NULL)
    {
      return -1;
    }

  caption = smart_band_ui_create_label(
    picker->preview_card, "PREVIEW", smart_band_ui_font_12(),
    lv_color_hex(0x647176), LV_TEXT_ALIGN_CENTER);
  picker->preview_name = smart_band_ui_create_label(
    picker->preview_card, "Watch face", smart_band_ui_font_32(),
    lv_color_hex(0x182225), LV_TEXT_ALIGN_CENTER);
  picker->preview_position = smart_band_ui_create_label(
    picker->preview_card, "Face 1 of 1", smart_band_ui_font_14(),
    lv_color_hex(0x526167), LV_TEXT_ALIGN_CENTER);
  picker->preview_swatch_primary = smart_band_ui_create_box(
    picker->root, swatch_start, swatch_y, swatch_size, swatch_size,
    lv_color_hex(0xffffff), smart_band_ui_sx(ui, 8));
  picker->preview_swatch_secondary = smart_band_ui_create_box(
    picker->root, swatch_start + swatch_size + swatch_gap, swatch_y,
    swatch_size, swatch_size, lv_color_hex(0x79c5be),
    smart_band_ui_sx(ui, 8));
  picker->preview_swatch_accent = smart_band_ui_create_box(
    picker->root, swatch_start + (swatch_size + swatch_gap) * 2, swatch_y,
    swatch_size, swatch_size, lv_color_hex(0xe5a5a8),
    smart_band_ui_sx(ui, 8));
  previous = smart_band_ui_create_action_button(
    ui, picker->root, "Previous", margin, navigation_y, half_width,
    button_h, lv_color_hex(0x43535a), previous_cb, (uintptr_t)picker);
  next = smart_band_ui_create_action_button(
    ui, picker->root, "Next", margin + half_width + gap, navigation_y,
    half_width, button_h, lv_color_hex(0x43535a), next_cb,
    (uintptr_t)picker);
  picker->status = smart_band_ui_create_label(
    picker->root, "", smart_band_ui_font_12(), lv_color_hex(0xffd27a),
    LV_TEXT_ALIGN_CENTER);
  cancel = smart_band_ui_create_action_button(
    ui, picker->root, "Cancel", margin, action_y, half_width, button_h,
    lv_color_hex(0x43535a), cancel_cb, (uintptr_t)picker);
  apply = smart_band_ui_create_action_button(
    ui, picker->root, "Apply", margin + half_width + gap, action_y,
    half_width, button_h, lv_color_hex(0x2e8d82), apply_cb,
    (uintptr_t)picker);

  if (caption == NULL || picker->preview_name == NULL ||
      picker->preview_position == NULL ||
      picker->preview_swatch_primary == NULL ||
      picker->preview_swatch_secondary == NULL ||
      picker->preview_swatch_accent == NULL || previous == NULL ||
      next == NULL || picker->status == NULL || cancel == NULL ||
      apply == NULL)
    {
      return -1;
    }

  smart_band_ui_place_label(title, margin, smart_band_ui_sy(ui, 22),
                            content_width, smart_band_ui_sy(ui, 32));
  smart_band_ui_place_label(caption, smart_band_ui_sx(ui, 12),
                            smart_band_ui_sy(ui, 18),
                            content_width - smart_band_ui_sx(ui, 24),
                            smart_band_ui_sy(ui, 20));
  smart_band_ui_place_label(picker->preview_name,
                            smart_band_ui_sx(ui, 12),
                            smart_band_ui_sy(ui, 68),
                            content_width - smart_band_ui_sx(ui, 24),
                            smart_band_ui_sy(ui, 50));
  smart_band_ui_place_label(picker->preview_position,
                            smart_band_ui_sx(ui, 12),
                            smart_band_ui_sy(ui, 128),
                            content_width - smart_band_ui_sx(ui, 24),
                            smart_band_ui_sy(ui, 28));
  smart_band_ui_place_label(picker->status, margin,
                            smart_band_ui_sy(ui, 416), content_width,
                            smart_band_ui_sy(ui, 32));
  return 0;
}

int smart_band_watch_face_picker_mount(
  smart_band_watch_face_picker_t *picker, lv_obj_t *parent,
  const smart_band_ui_components_t *ui,
  smart_band_watch_face_id_t selected_id,
  smart_band_watch_face_picker_apply_cb_t callback,
  void *apply_context)
{
  const smart_band_watch_face_descriptor_t *initial;

  if (picker == NULL || parent == NULL || ui == NULL || ui->screen_w <= 0 ||
      ui->screen_h <= 0)
    {
      return -1;
    }

  initial = find_initial_face(selected_id);
  if (initial == NULL)
    {
      return -1;
    }

  smart_band_watch_face_picker_unmount(picker);
  picker->ui = *ui;
  picker->preview = initial;
  picker->apply_cb = callback;
  picker->apply_context = apply_context;
  picker->root = lv_obj_create(parent);
  if (picker->root == NULL)
    {
      memset(picker, 0, sizeof(*picker));
      return -1;
    }

  smart_band_ui_strip_obj(picker->root);
  lv_obj_set_pos(picker->root, 0, 0);
  lv_obj_set_size(picker->root, ui->screen_w, ui->screen_h);
  lv_obj_set_style_bg_color(picker->root, lv_color_hex(0x101719), 0);
  lv_obj_set_style_bg_opa(picker->root, LV_OPA_COVER, 0);
  lv_obj_add_flag(picker->root, LV_OBJ_FLAG_CLICKABLE);

  if (create_picker_content(picker) != 0)
    {
      smart_band_watch_face_picker_unmount(picker);
      return -1;
    }

  update_preview(picker);
  smart_band_watch_face_picker_hide(picker);
  return 0;
}

void smart_band_watch_face_picker_unmount(
  smart_band_watch_face_picker_t *picker)
{
  if (picker == NULL)
    {
      return;
    }

  if (picker->root != NULL && lv_obj_is_valid(picker->root))
    {
      lv_obj_del(picker->root);
    }

  memset(picker, 0, sizeof(*picker));
}

void smart_band_watch_face_picker_show(
  smart_band_watch_face_picker_t *picker,
  smart_band_watch_face_id_t selected_id)
{
  const smart_band_watch_face_descriptor_t *selected;

  if (picker == NULL || picker->root == NULL ||
      !lv_obj_is_valid(picker->root))
    {
      return;
    }

  selected = find_initial_face(selected_id);
  if (selected == NULL)
    {
      return;
    }

  picker->preview = selected;
  smart_band_watch_face_picker_set_status_message(picker, NULL);
  update_preview(picker);
  lv_obj_clear_flag(picker->root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(picker->root);
  picker->visible = true;
}

void smart_band_watch_face_picker_hide(
  smart_band_watch_face_picker_t *picker)
{
  if (picker == NULL || picker->root == NULL ||
      !lv_obj_is_valid(picker->root))
    {
      return;
    }

  lv_obj_add_flag(picker->root, LV_OBJ_FLAG_HIDDEN);
  picker->visible = false;
}

bool smart_band_watch_face_picker_is_visible(
  const smart_band_watch_face_picker_t *picker)
{
  return picker != NULL && picker->root != NULL && picker->visible &&
         lv_obj_is_valid(picker->root);
}

lv_obj_t *smart_band_watch_face_picker_root(
  smart_band_watch_face_picker_t *picker)
{
  return picker != NULL ? picker->root : NULL;
}

smart_band_watch_face_id_t smart_band_watch_face_picker_selected_id(
  const smart_band_watch_face_picker_t *picker)
{
  const smart_band_watch_face_descriptor_t *fallback;

  if (picker != NULL && picker->preview != NULL)
    {
      return picker->preview->id;
    }

  fallback = smart_band_watch_face_registry_default();
  return fallback != NULL ? fallback->id : SMART_BAND_WATCH_FACE_LOTUS;
}
