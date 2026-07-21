#ifndef SMART_BAND_WATCH_FACE_H
#define SMART_BAND_WATCH_FACE_H

#include "watch_model.h"

#include <lvgl/lvgl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_WATCH_FACE_CONTEXT_CAPACITY 256

typedef enum
{
  SMART_BAND_WATCH_FACE_LOTUS = 0
} smart_band_watch_face_id_t;

typedef struct
{
  lv_coord_t screen_width;
  lv_coord_t screen_height;
  bool compact_band;
} smart_band_watch_face_config_t;

typedef struct
{
  int (*mount)(void *context, lv_obj_t *parent,
               const smart_band_watch_face_config_t *config);
  void (*render)(void *context, const smart_band_state_t *model);
  void (*set_visible)(void *context, bool visible);
  lv_obj_t *(*root)(void *context);
  void (*unmount)(void *context);
} smart_band_watch_face_ops_t;

typedef struct
{
  smart_band_watch_face_id_t id;
  const char *name;
  size_t context_size;
  const smart_band_watch_face_ops_t *ops;
} smart_band_watch_face_descriptor_t;

typedef union
{
  void *pointer_alignment;
  long double long_double_alignment;
  unsigned char bytes[SMART_BAND_WATCH_FACE_CONTEXT_CAPACITY];
} smart_band_watch_face_storage_t;

typedef struct
{
  const smart_band_watch_face_descriptor_t *descriptor;
  smart_band_watch_face_storage_t storage;
  bool mounted;
} smart_band_watch_face_instance_t;

size_t smart_band_watch_face_registry_count(void);
const smart_band_watch_face_descriptor_t *
smart_band_watch_face_registry_at(size_t index);
const smart_band_watch_face_descriptor_t *
smart_band_watch_face_registry_find(smart_band_watch_face_id_t id);
const smart_band_watch_face_descriptor_t *
smart_band_watch_face_registry_default(void);

int smart_band_watch_face_mount(
  smart_band_watch_face_instance_t *instance,
  const smart_band_watch_face_descriptor_t *descriptor,
  lv_obj_t *parent,
  const smart_band_watch_face_config_t *config);
void smart_band_watch_face_render(smart_band_watch_face_instance_t *instance,
                                  const smart_band_state_t *model);
void smart_band_watch_face_set_visible(
  smart_band_watch_face_instance_t *instance, bool visible);
lv_obj_t *smart_band_watch_face_root(
  smart_band_watch_face_instance_t *instance);
void smart_band_watch_face_unmount(smart_band_watch_face_instance_t *instance);

#ifdef __cplusplus
}
#endif

#endif
