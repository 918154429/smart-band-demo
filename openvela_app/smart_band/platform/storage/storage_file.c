#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#  define _POSIX_C_SOURCE 200809L
#endif

#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#  define _CRT_SECURE_NO_WARNINGS
#endif

#include "smart_band_storage_backend.h"

#include "storage_fault_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#  include <io.h>
#  define SB_CLOSE _close
#  define SB_COMMIT _commit
#  define SB_OPEN _open
#  define SB_READ _read
#  define SB_SEEK _lseeki64
#  define SB_TRUNCATE _chsize_s
#  define SB_WRITE _write
#  define SB_BINARY _O_BINARY
#  define SB_STAT _stat64
#  define SB_STAT_STRUCT struct _stat64
#  define SB_IS_DIRECTORY(mode) (((mode) & _S_IFMT) == _S_IFDIR)
typedef __int64 sb_offset_t;
#else
#  include <unistd.h>
#  define SB_CLOSE close
#  define SB_COMMIT fsync
#  define SB_OPEN open
#  define SB_READ read
#  define SB_SEEK lseek
#  define SB_TRUNCATE ftruncate
#  define SB_WRITE write
#  define SB_BINARY 0
#  define SB_STAT stat
#  define SB_STAT_STRUCT struct stat
#  define SB_IS_DIRECTORY(mode) S_ISDIR(mode)
typedef off_t sb_offset_t;
#endif

static smart_band_platform_result_t map_errno(int error)
{
  if (error == ENOENT)
    {
      return SMART_BAND_PLATFORM_NOT_FOUND;
    }

  if (error == ENOSPC)
    {
      return SMART_BAND_PLATFORM_NO_SPACE;
    }

#ifdef EROFS
  if (error == EROFS)
    {
      return SMART_BAND_PLATFORM_READ_ONLY;
    }
#endif

  if (error == EACCES || error == EPERM)
    {
      return SMART_BAND_PLATFORM_READ_ONLY;
    }

  return SMART_BAND_PLATFORM_IO;
}

static smart_band_platform_result_t injected_result(
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

static bool path_has_parent_segment(const char *path)
{
  const char *segment = path;
  const char *cursor;

  for (cursor = path; ; cursor++)
    {
      if (*cursor == '/' || *cursor == '\\' || *cursor == '\0')
        {
          if (cursor - segment == 2 && segment[0] == '.' &&
              segment[1] == '.')
            {
              return true;
            }

          if (*cursor == '\0')
            {
              return false;
            }

          segment = cursor + 1;
        }
    }
}

static smart_band_platform_result_t object_path(
  const smart_band_storage_file_t *context, uint32_t object_id, char *path,
  size_t capacity)
{
  size_t length;
  const char *separator;
  int written;

  length = strlen(context->base_directory);
  separator = length > 0 &&
                      (context->base_directory[length - 1] == '/' ||
                       context->base_directory[length - 1] == '\\')
                ? ""
                : "/";
  written = snprintf(path, capacity, "%s%sobject-%08lx.bin",
                     context->base_directory, separator,
                     (unsigned long)object_id);
  if (written < 0 || (size_t)written >= capacity)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  return SMART_BAND_PLATFORM_OK;
}

static bool dirty_contains(const smart_band_storage_file_t *context,
                           uint32_t object_id)
{
  size_t index;

  for (index = 0; index < context->dirty_count; index++)
    {
      if (context->dirty_objects[index] == object_id)
        {
          return true;
        }
    }

  return false;
}

static smart_band_platform_result_t reserve_dirty(
  smart_band_storage_file_t *context, uint32_t object_id, bool *added)
{
  *added = false;
  if (dirty_contains(context, object_id))
    {
      return SMART_BAND_PLATFORM_OK;
    }

  if (context->dirty_count >= SMART_BAND_STORAGE_FILE_MAX_DIRTY_OBJECTS)
    {
      return SMART_BAND_PLATFORM_NO_SPACE;
    }

  context->dirty_objects[context->dirty_count++] = object_id;
  *added = true;
  return SMART_BAND_PLATFORM_OK;
}

static int open_read(const char *path)
{
  int descriptor;

  do
    {
      descriptor = SB_OPEN(path, O_RDONLY | SB_BINARY);
    }
  while (descriptor < 0 && errno == EINTR);

  return descriptor;
}

static int open_write(const char *path, bool truncate)
{
  int flags = O_WRONLY | O_CREAT | SB_BINARY;
  int descriptor;

  if (truncate)
    {
      flags |= O_TRUNC;
    }

#ifdef _WIN32
  do
    {
      descriptor = SB_OPEN(path, flags, _S_IREAD | _S_IWRITE);
    }
  while (descriptor < 0 && errno == EINTR);
#else
  do
    {
      descriptor = SB_OPEN(path, flags, 0600);
    }
  while (descriptor < 0 && errno == EINTR);
#endif

  return descriptor;
}

static int open_update(const char *path)
{
  int descriptor;

  do
    {
      descriptor = SB_OPEN(path, O_RDWR | SB_BINARY);
    }
  while (descriptor < 0 && errno == EINTR);

  return descriptor;
}

static smart_band_platform_result_t write_all(int descriptor,
                                               const uint8_t *buffer,
                                               size_t size)
{
  size_t offset = 0;

  while (offset < size)
    {
      size_t remaining = size - offset;
#ifdef _WIN32
      unsigned int chunk = remaining > UINT_MAX ? UINT_MAX :
                                                    (unsigned int)remaining;
      int written = SB_WRITE(descriptor, buffer + offset, chunk);
#else
      ssize_t written = SB_WRITE(descriptor, buffer + offset, remaining);
#endif
      if (written < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          return map_errno(errno);
        }

      if (written == 0)
        {
          return SMART_BAND_PLATFORM_IO;
        }

      offset += (size_t)written;
    }

  return SMART_BAND_PLATFORM_OK;
}

static smart_band_platform_result_t read_all(int descriptor, uint8_t *buffer,
                                              size_t size)
{
  size_t offset = 0;

  while (offset < size)
    {
      size_t remaining = size - offset;
#ifdef _WIN32
      unsigned int chunk = remaining > UINT_MAX ? UINT_MAX :
                                                    (unsigned int)remaining;
      int received = SB_READ(descriptor, buffer + offset, chunk);
#else
      ssize_t received = SB_READ(descriptor, buffer + offset, remaining);
#endif
      if (received < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          return map_errno(errno);
        }

      if (received == 0)
        {
          return SMART_BAND_PLATFORM_IO;
        }

      offset += (size_t)received;
    }

  return SMART_BAND_PLATFORM_OK;
}

static smart_band_platform_result_t truncate_file(const char *path,
                                                   size_t size)
{
  smart_band_platform_result_t result = SMART_BAND_PLATFORM_OK;
  sb_offset_t current_size;
  int descriptor = open_update(path);

  if (descriptor < 0)
    {
      return map_errno(errno);
    }

  current_size = SB_SEEK(descriptor, 0, SEEK_END);
  if (current_size < 0)
    {
      result = map_errno(errno);
    }
  else if ((uint64_t)size < (uint64_t)current_size &&
           SB_TRUNCATE(descriptor, (sb_offset_t)size) != 0)
    {
      result = map_errno(errno);
    }

  if (SB_CLOSE(descriptor) != 0 && result == SMART_BAND_PLATFORM_OK)
    {
      result = map_errno(errno);
    }

  return result;
}

static smart_band_platform_result_t corrupt_file(const char *path,
                                                  size_t byte_index,
                                                  uint8_t xor_mask)
{
  smart_band_platform_result_t result = SMART_BAND_PLATFORM_OK;
  uint8_t value;
  int descriptor = open_update(path);

  if (descriptor < 0)
    {
      return map_errno(errno);
    }

  if (SB_SEEK(descriptor, (sb_offset_t)byte_index, SEEK_SET) < 0)
    {
      result = map_errno(errno);
    }
  else
    {
#ifdef _WIN32
      int received;
#else
      ssize_t received;
#endif
      do
        {
          received = SB_READ(descriptor, &value, 1);
        }
      while (received < 0 && errno == EINTR);

      if (received < 0)
        {
          result = map_errno(errno);
        }
      else if (received == 1)
        {
          value ^= xor_mask;
          if (SB_SEEK(descriptor, (sb_offset_t)byte_index, SEEK_SET) < 0)
            {
              result = map_errno(errno);
            }
          else
            {
              result = write_all(descriptor, &value, 1);
            }
        }
    }

  if (SB_CLOSE(descriptor) != 0 && result == SMART_BAND_PLATFORM_OK)
    {
      result = map_errno(errno);
    }

  return result;
}

static smart_band_platform_result_t storage_file_read(
  void *opaque, uint32_t object_id, void *buffer, size_t capacity,
  size_t *actual_size)
{
  smart_band_storage_file_t *context = opaque;
  smart_band_storage_fault_kind_t kind = SMART_BAND_STORAGE_FAULT_NONE;
  smart_band_platform_result_t result;
  char path[SMART_BAND_STORAGE_FILE_PATH_CAPACITY];
  size_t byte_index = 0;
  uint8_t xor_mask = 0;
  sb_offset_t file_size;
  int descriptor;
  bool triggered;

  if (actual_size != NULL)
    {
      *actual_size = 0;
    }

  if (context == NULL || actual_size == NULL ||
      (capacity > 0 && buffer == NULL))
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  triggered = smart_band_storage_fault_take(
    &context->fault, SMART_BAND_STORAGE_OPERATION_READ, &kind,
    &byte_index, &xor_mask);
  if (triggered && kind != SMART_BAND_STORAGE_FAULT_TRUNCATE &&
      kind != SMART_BAND_STORAGE_FAULT_CORRUPT)
    {
      return injected_result(kind);
    }

  result = object_path(context, object_id, path, sizeof(path));
  if (result != SMART_BAND_PLATFORM_OK)
    {
      return result;
    }

  if (triggered && kind == SMART_BAND_STORAGE_FAULT_TRUNCATE)
    {
      result = truncate_file(path, byte_index);
      if (result != SMART_BAND_PLATFORM_OK)
        {
          return result;
        }
    }
  else if (triggered && kind == SMART_BAND_STORAGE_FAULT_CORRUPT)
    {
      result = corrupt_file(path, byte_index, xor_mask);
      if (result != SMART_BAND_PLATFORM_OK)
        {
          return result;
        }
    }

  descriptor = open_read(path);
  if (descriptor < 0)
    {
      return map_errno(errno);
    }

  file_size = SB_SEEK(descriptor, 0, SEEK_END);
  if (file_size < 0 || SB_SEEK(descriptor, 0, SEEK_SET) < 0)
    {
      result = map_errno(errno);
    }
  else if ((uint64_t)file_size > SMART_BAND_STORAGE_FILE_OBJECT_CAPACITY ||
           (uint64_t)file_size > capacity)
    {
      *actual_size = (size_t)file_size;
      result = SMART_BAND_PLATFORM_INVALID;
    }
  else
    {
      *actual_size = (size_t)file_size;
      result = read_all(descriptor, buffer, *actual_size);
    }

  if (SB_CLOSE(descriptor) != 0 && result == SMART_BAND_PLATFORM_OK)
    {
      result = map_errno(errno);
    }

  return result;
}

static smart_band_platform_result_t storage_file_write(
  void *opaque, uint32_t object_id, const void *buffer, size_t size)
{
  smart_band_storage_file_t *context = opaque;
  smart_band_storage_fault_kind_t kind = SMART_BAND_STORAGE_FAULT_NONE;
  smart_band_platform_result_t result;
  char path[SMART_BAND_STORAGE_FILE_PATH_CAPACITY];
  size_t byte_index = 0;
  size_t prefix;
  uint8_t xor_mask = 0;
  int descriptor;
  bool dirty_added;
  bool partial;
  bool triggered;

  if (context == NULL || (size > 0 && buffer == NULL) ||
      size > SMART_BAND_STORAGE_FILE_OBJECT_CAPACITY)
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
      return injected_result(kind);
    }

  result = object_path(context, object_id, path, sizeof(path));
  if (result != SMART_BAND_PLATFORM_OK)
    {
      return result;
    }

  result = reserve_dirty(context, object_id, &dirty_added);
  if (result != SMART_BAND_PLATFORM_OK)
    {
      return result;
    }

  partial = triggered &&
            (kind == SMART_BAND_STORAGE_FAULT_SHORT_WRITE ||
             kind == SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION);
  descriptor = open_write(
    path, !partial || kind == SMART_BAND_STORAGE_FAULT_SHORT_WRITE);
  if (descriptor < 0)
    {
      if (dirty_added)
        {
          context->dirty_count--;
        }

      return map_errno(errno);
    }

  prefix = partial && byte_index < size ? byte_index : size;
  result = write_all(descriptor, buffer, prefix);
  if (SB_CLOSE(descriptor) != 0 && result == SMART_BAND_PLATFORM_OK)
    {
      result = map_errno(errno);
    }

  if (result != SMART_BAND_PLATFORM_OK)
    {
      return result;
    }

  if (partial)
    {
      return SMART_BAND_PLATFORM_IO;
    }

  if (triggered && kind == SMART_BAND_STORAGE_FAULT_TRUNCATE)
    {
      return byte_index < size ? truncate_file(path, byte_index) :
                                 SMART_BAND_PLATFORM_OK;
    }

  if (triggered && kind == SMART_BAND_STORAGE_FAULT_CORRUPT &&
      byte_index < size)
    {
      return corrupt_file(path, byte_index, xor_mask);
    }

  return SMART_BAND_PLATFORM_OK;
}

static smart_band_platform_result_t storage_file_flush(void *opaque)
{
  smart_band_storage_file_t *context = opaque;
  smart_band_storage_fault_kind_t kind;
  smart_band_platform_result_t result;
  char path[SMART_BAND_STORAGE_FILE_PATH_CAPACITY];
  size_t index;
  int descriptor;

  if (context == NULL)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  if (smart_band_storage_fault_take(
        &context->fault, SMART_BAND_STORAGE_OPERATION_FLUSH, &kind, NULL,
        NULL))
    {
      return injected_result(kind);
    }

  for (index = 0; index < context->dirty_count; index++)
    {
      result = object_path(context, context->dirty_objects[index], path,
                           sizeof(path));
      if (result != SMART_BAND_PLATFORM_OK)
        {
          return result;
        }

      descriptor = open_update(path);
      if (descriptor < 0)
        {
          return map_errno(errno);
        }

      result = SB_COMMIT(descriptor) == 0 ? SMART_BAND_PLATFORM_OK :
                                           map_errno(errno);
      if (SB_CLOSE(descriptor) != 0 && result == SMART_BAND_PLATFORM_OK)
        {
          result = map_errno(errno);
        }

      if (result != SMART_BAND_PLATFORM_OK)
        {
          return result;
        }
    }

  context->dirty_count = 0;
  return SMART_BAND_PLATFORM_OK;
}

static const smart_band_storage_ops_t g_file_ops =
{
  storage_file_read,
  storage_file_write,
  storage_file_flush
};

smart_band_platform_result_t smart_band_storage_file_init(
  smart_band_storage_file_t *context, const char *base_directory,
  smart_band_storage_t *storage)
{
  SB_STAT_STRUCT status;
  size_t length;

  if (context == NULL || base_directory == NULL || storage == NULL)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  length = strlen(base_directory);
  if (length == 0 || length >= sizeof(context->base_directory) - 21 ||
      path_has_parent_segment(base_directory))
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  if (SB_STAT(base_directory, &status) != 0)
    {
      return map_errno(errno);
    }

  if (!SB_IS_DIRECTORY(status.st_mode))
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  memset(context, 0, sizeof(*context));
  memcpy(context->base_directory, base_directory, length + 1);
  storage->ops = &g_file_ops;
  storage->context = context;
  return SMART_BAND_PLATFORM_OK;
}
