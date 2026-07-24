#ifndef SMART_BAND_EVENT_MUTEX_H
#define SMART_BAND_EVENT_MUTEX_H

#include "smart_band_event.h"

#include <stdbool.h>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
#if defined(_WIN32)
  CRITICAL_SECTION native;
#else
  pthread_mutex_t native;
#endif
  bool initialized;
} smart_band_event_mutex_t;

int smart_band_event_mutex_init(smart_band_event_mutex_t *mutex);
void smart_band_event_mutex_deinit(smart_band_event_mutex_t *mutex);
bool smart_band_event_mutex_get_lock(smart_band_event_mutex_t *mutex,
                                     smart_band_event_lock_t *lock);

#ifdef __cplusplus
}
#endif

#endif
