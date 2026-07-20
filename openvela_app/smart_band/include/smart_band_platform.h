#ifndef SMART_BAND_PLATFORM_H
#define SMART_BAND_PLATFORM_H

#include "smart_band_event.h"
#include "smart_band_haptic.h"
#include "smart_band_power.h"
#include "smart_band_storage.h"
#include "smart_band_sync_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  smart_band_storage_t storage;
  smart_band_power_platform_t power;
  smart_band_haptic_t haptic;
  smart_band_sync_transport_t sync;
  smart_band_event_lock_t event_lock;
} smart_band_platform_t;

void smart_band_platform_init_noop(smart_band_platform_t *platform);

#ifdef __cplusplus
}
#endif

#endif
