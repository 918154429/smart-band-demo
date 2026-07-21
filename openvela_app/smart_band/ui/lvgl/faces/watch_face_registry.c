#include "smart_band_watch_face.h"

#include "lotus_face.h"

#include <string.h>

static const smart_band_watch_face_descriptor_t *const g_registry[] =
{
  &smart_band_lotus_face
};

size_t smart_band_watch_face_registry_count(void)
{
  return sizeof(g_registry) / sizeof(g_registry[0]);
}

const smart_band_watch_face_descriptor_t *
smart_band_watch_face_registry_at(size_t index)
{
  return index < smart_band_watch_face_registry_count() ?
         g_registry[index] : NULL;
}

const smart_band_watch_face_descriptor_t *
smart_band_watch_face_registry_find(smart_band_watch_face_id_t id)
{
  size_t index;

  for (index = 0; index < smart_band_watch_face_registry_count(); index++)
    {
      if (g_registry[index]->id == id)
        {
          return g_registry[index];
        }
    }

  return NULL;
}

const smart_band_watch_face_descriptor_t *
smart_band_watch_face_registry_default(void)
{
  return smart_band_watch_face_registry_find(SMART_BAND_WATCH_FACE_LOTUS);
}

int smart_band_watch_face_mount(
  smart_band_watch_face_instance_t *instance,
  const smart_band_watch_face_descriptor_t *descriptor,
  lv_obj_t *parent,
  const smart_band_watch_face_config_t *config)
{
  if (instance == NULL || descriptor == NULL || parent == NULL ||
      config == NULL || descriptor->ops == NULL ||
      descriptor->ops->mount == NULL ||
      descriptor->context_size == 0 ||
      descriptor->context_size > SMART_BAND_WATCH_FACE_CONTEXT_CAPACITY)
    {
      return -1;
    }

  smart_band_watch_face_unmount(instance);
  instance->descriptor = descriptor;
  if (descriptor->ops->mount(instance->storage.bytes, parent, config) != 0)
    {
      if (descriptor->ops->unmount != NULL)
        {
          descriptor->ops->unmount(instance->storage.bytes);
        }

      memset(instance, 0, sizeof(*instance));
      return -1;
    }

  instance->mounted = true;
  return 0;
}

void smart_band_watch_face_render(smart_band_watch_face_instance_t *instance,
                                  const smart_band_state_t *model)
{
  if (instance != NULL && instance->mounted && model != NULL &&
      instance->descriptor != NULL && instance->descriptor->ops != NULL &&
      instance->descriptor->ops->render != NULL)
    {
      instance->descriptor->ops->render(instance->storage.bytes, model);
    }
}

void smart_band_watch_face_set_visible(
  smart_band_watch_face_instance_t *instance, bool visible)
{
  if (instance != NULL && instance->mounted &&
      instance->descriptor != NULL && instance->descriptor->ops != NULL &&
      instance->descriptor->ops->set_visible != NULL)
    {
      instance->descriptor->ops->set_visible(instance->storage.bytes,
                                             visible);
    }
}

lv_obj_t *smart_band_watch_face_root(
  smart_band_watch_face_instance_t *instance)
{
  if (instance != NULL && instance->mounted &&
      instance->descriptor != NULL && instance->descriptor->ops != NULL &&
      instance->descriptor->ops->root != NULL)
    {
      return instance->descriptor->ops->root(instance->storage.bytes);
    }

  return NULL;
}

void smart_band_watch_face_unmount(smart_band_watch_face_instance_t *instance)
{
  if (instance == NULL)
    {
      return;
    }

  if (instance->descriptor != NULL && instance->descriptor->ops != NULL &&
      instance->descriptor->ops->unmount != NULL)
    {
      instance->descriptor->ops->unmount(instance->storage.bytes);
    }

  memset(instance, 0, sizeof(*instance));
}
