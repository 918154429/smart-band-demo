#include "smart_band_storage_backend.h"

#include "storage_fault_internal.h"

#include <string.h>

static smart_band_storage_memory_object_t *memory_find(
  smart_band_storage_memory_t *context, uint32_t object_id)
{
  size_t index;

  for (index = 0; index < SMART_BAND_STORAGE_MEMORY_MAX_OBJECTS; index++)
    {
      if (context->objects[index].used &&
          context->objects[index].object_id == object_id)
        {
          return &context->objects[index];
        }
    }

  return NULL;
}

static smart_band_storage_memory_object_t *memory_find_or_allocate(
  smart_band_storage_memory_t *context, uint32_t object_id)
{
  smart_band_storage_memory_object_t *object;
  size_t index;

  object = memory_find(context, object_id);
  if (object != NULL)
    {
      return object;
    }

  for (index = 0; index < SMART_BAND_STORAGE_MEMORY_MAX_OBJECTS; index++)
    {
      if (!context->objects[index].used)
        {
          object = &context->objects[index];
          memset(object, 0, sizeof(*object));
          object->used = true;
          object->object_id = object_id;
          return object;
        }
    }

  return NULL;
}

static smart_band_platform_result_t fault_result(
  smart_band_storage_fault_kind_t kind)
{
  if (kind == SMART_BAND_STORAGE_FAULT_NO_SPACE)
    {
      return SMART_BAND_PLATFORM_NO_SPACE;
    }

  if (kind == SMART_BAND_STORAGE_FAULT_READ_ONLY)
    {
      return SMART_BAND_PLATFORM_READ_ONLY;
    }

  return SMART_BAND_PLATFORM_IO;
}

static smart_band_platform_result_t memory_read(
  void *opaque, uint32_t object_id, void *buffer, size_t capacity,
  size_t *actual_size)
{
  smart_band_storage_memory_t *context = opaque;
  smart_band_storage_memory_object_t *object;
  smart_band_storage_fault_kind_t kind = SMART_BAND_STORAGE_FAULT_NONE;
  size_t byte_index = 0;
  uint8_t xor_mask = 0;

  if (actual_size != NULL)
    {
      *actual_size = 0;
    }

  if (context == NULL || actual_size == NULL ||
      (capacity > 0 && buffer == NULL))
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  if (smart_band_storage_fault_take(
        &context->fault, SMART_BAND_STORAGE_OPERATION_READ, &kind,
        &byte_index, &xor_mask) &&
      kind != SMART_BAND_STORAGE_FAULT_TRUNCATE &&
      kind != SMART_BAND_STORAGE_FAULT_CORRUPT)
    {
      return fault_result(kind);
    }

  object = memory_find(context, object_id);
  if (object == NULL)
    {
      return SMART_BAND_PLATFORM_NOT_FOUND;
    }

  if (kind == SMART_BAND_STORAGE_FAULT_TRUNCATE && byte_index < object->size)
    {
      object->size = byte_index;
    }
  else if (kind == SMART_BAND_STORAGE_FAULT_CORRUPT &&
           byte_index < object->size)
    {
      object->data[byte_index] ^= xor_mask;
    }

  *actual_size = object->size;
  if (capacity < object->size)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  if (object->size > 0)
    {
      memcpy(buffer, object->data, object->size);
    }

  return SMART_BAND_PLATFORM_OK;
}

static smart_band_platform_result_t memory_write(
  void *opaque, uint32_t object_id, const void *buffer, size_t size)
{
  smart_band_storage_memory_t *context = opaque;
  smart_band_storage_memory_object_t *object;
  smart_band_storage_fault_kind_t kind = SMART_BAND_STORAGE_FAULT_NONE;
  size_t byte_index = 0;
  size_t prefix;
  uint8_t xor_mask = 0;
  bool triggered;

  if (context == NULL || (size > 0 && buffer == NULL) ||
      size > SMART_BAND_STORAGE_MEMORY_OBJECT_CAPACITY)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  triggered = smart_band_storage_fault_take(
    &context->fault, SMART_BAND_STORAGE_OPERATION_WRITE, &kind,
    &byte_index, &xor_mask);
  if (triggered && kind != SMART_BAND_STORAGE_FAULT_SHORT_WRITE &&
      kind != SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION &&
      kind != SMART_BAND_STORAGE_FAULT_TRUNCATE &&
      kind != SMART_BAND_STORAGE_FAULT_CORRUPT)
    {
      return fault_result(kind);
    }

  object = memory_find_or_allocate(context, object_id);
  if (object == NULL)
    {
      return SMART_BAND_PLATFORM_NO_SPACE;
    }

  if (triggered && (kind == SMART_BAND_STORAGE_FAULT_SHORT_WRITE ||
                    kind == SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION))
    {
      prefix = byte_index < size ? byte_index : size;
      if (prefix > 0)
        {
          memcpy(object->data, buffer, prefix);
        }

      if (kind == SMART_BAND_STORAGE_FAULT_SHORT_WRITE)
        {
          object->size = prefix;
        }
      else if (object->size < prefix)
        {
          object->size = prefix;
        }

      return SMART_BAND_PLATFORM_IO;
    }

  if (size > 0)
    {
      memcpy(object->data, buffer, size);
    }

  object->size = size;
  if (triggered && kind == SMART_BAND_STORAGE_FAULT_TRUNCATE &&
      byte_index < object->size)
    {
      object->size = byte_index;
    }
  else if (triggered && kind == SMART_BAND_STORAGE_FAULT_CORRUPT &&
           byte_index < object->size)
    {
      object->data[byte_index] ^= xor_mask;
    }

  return SMART_BAND_PLATFORM_OK;
}

static smart_band_platform_result_t memory_flush(void *opaque)
{
  smart_band_storage_memory_t *context = opaque;
  smart_band_storage_fault_kind_t kind;

  if (context == NULL)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  if (smart_band_storage_fault_take(
        &context->fault, SMART_BAND_STORAGE_OPERATION_FLUSH, &kind, NULL,
        NULL))
    {
      return fault_result(kind);
    }

  return SMART_BAND_PLATFORM_OK;
}

static const smart_band_storage_ops_t g_memory_ops =
{
  memory_read,
  memory_write,
  memory_flush,
  NULL
};

smart_band_platform_result_t smart_band_storage_memory_init(
  smart_band_storage_memory_t *context, smart_band_storage_t *storage)
{
  if (context == NULL || storage == NULL)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  memset(context, 0, sizeof(*context));
  storage->ops = &g_memory_ops;
  storage->context = context;
  return SMART_BAND_PLATFORM_OK;
}

void smart_band_storage_memory_clear(smart_band_storage_memory_t *context)
{
  if (context != NULL)
    {
      memset(context->objects, 0, sizeof(context->objects));
    }
}
