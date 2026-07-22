#ifndef SMART_BAND_STORAGE_H
#define SMART_BAND_STORAGE_H

#include "smart_band_platform_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  smart_band_platform_result_t (*read)(void *context, uint32_t object_id,
                                       void *buffer, size_t capacity,
                                       size_t *actual_size);
  smart_band_platform_result_t (*write)(void *context, uint32_t object_id,
                                        const void *buffer, size_t size);
  smart_band_platform_result_t (*flush)(void *context);
  bool (*is_permanently_unavailable)(void *context);
} smart_band_storage_ops_t;

typedef struct
{
  const smart_band_storage_ops_t *ops;
  void *context;
} smart_band_storage_t;

static inline bool smart_band_storage_is_permanently_unavailable(
  const smart_band_storage_t *storage)
{
  return storage != NULL && storage->ops != NULL &&
         storage->ops->is_permanently_unavailable != NULL &&
         storage->ops->is_permanently_unavailable(storage->context);
}

#ifdef __cplusplus
}
#endif

#endif
