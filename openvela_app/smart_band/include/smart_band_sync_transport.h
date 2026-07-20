#ifndef SMART_BAND_SYNC_TRANSPORT_H
#define SMART_BAND_SYNC_TRANSPORT_H

#include "smart_band_event.h"
#include "smart_band_platform_result.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*smart_band_event_sink_t)(void *context,
                                        const smart_band_event_t *event);

typedef enum
{
  SMART_BAND_SYNC_STOPPED = 0,
  SMART_BAND_SYNC_STARTED
} smart_band_sync_transport_state_t;

typedef struct
{
  smart_band_platform_result_t (*start)(void *context,
                                        smart_band_event_sink_t event_sink,
                                        void *event_context);
  smart_band_platform_result_t (*stop)(void *context);
  smart_band_platform_result_t (*send)(void *context, const void *buffer,
                                       size_t size);
  smart_band_platform_result_t (*poll)(void *context, void *buffer,
                                       size_t capacity, size_t *actual_size);
  smart_band_sync_transport_state_t (*status)(void *context);
  size_t (*mtu)(void *context);
} smart_band_sync_transport_ops_t;

typedef struct
{
  const smart_band_sync_transport_ops_t *ops;
  void *context;
} smart_band_sync_transport_t;

#ifdef __cplusplus
}
#endif

#endif
