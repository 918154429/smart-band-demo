#include "smart_band_event_mutex.h"

#include <string.h>

static bool event_mutex_lock(void *context)
{
  smart_band_event_mutex_t *mutex = context;

  if (mutex == NULL || !mutex->initialized)
    {
      return false;
    }

#if defined(_WIN32)
  EnterCriticalSection(&mutex->native);
  return true;
#else
  return pthread_mutex_lock(&mutex->native) == 0;
#endif
}

static void event_mutex_unlock(void *context)
{
  smart_band_event_mutex_t *mutex = context;

  if (mutex == NULL || !mutex->initialized)
    {
      return;
    }

#if defined(_WIN32)
  LeaveCriticalSection(&mutex->native);
#else
  (void)pthread_mutex_unlock(&mutex->native);
#endif
}

int smart_band_event_mutex_init(smart_band_event_mutex_t *mutex)
{
  if (mutex == NULL)
    {
      return -1;
    }

  (void)memset(mutex, 0, sizeof(*mutex));
#if defined(_WIN32)
  InitializeCriticalSection(&mutex->native);
#else
  if (pthread_mutex_init(&mutex->native, NULL) != 0)
    {
      return -1;
    }
#endif
  mutex->initialized = true;
  return 0;
}

void smart_band_event_mutex_deinit(smart_band_event_mutex_t *mutex)
{
  if (mutex == NULL || !mutex->initialized)
    {
      return;
    }

  mutex->initialized = false;
#if defined(_WIN32)
  DeleteCriticalSection(&mutex->native);
#else
  (void)pthread_mutex_destroy(&mutex->native);
#endif
  (void)memset(mutex, 0, sizeof(*mutex));
}

bool smart_band_event_mutex_get_lock(smart_band_event_mutex_t *mutex,
                                     smart_band_event_lock_t *lock)
{
  if (mutex == NULL || lock == NULL || !mutex->initialized)
    {
      return false;
    }

  lock->context = mutex;
  lock->lock = event_mutex_lock;
  lock->unlock = event_mutex_unlock;
  return true;
}
