#include "smart_band_platform.h"

#include <string.h>

static smart_band_platform_result_t
noop_storage_read(void *context, uint32_t object_id, void *buffer,
                  size_t capacity, size_t *actual_size)
{
  (void)context;
  (void)object_id;
  (void)buffer;
  (void)capacity;
  if (actual_size != NULL)
    {
      *actual_size = 0;
    }

  return SMART_BAND_PLATFORM_UNAVAILABLE;
}

static smart_band_platform_result_t
noop_storage_write(void *context, uint32_t object_id, const void *buffer,
                   size_t size)
{
  (void)context;
  (void)object_id;
  (void)buffer;
  (void)size;
  return SMART_BAND_PLATFORM_UNAVAILABLE;
}

static smart_band_platform_result_t noop_unavailable(void *context)
{
  (void)context;
  return SMART_BAND_PLATFORM_UNAVAILABLE;
}

static smart_band_platform_result_t
noop_display(void *context, bool enabled)
{
  (void)context;
  (void)enabled;
  return SMART_BAND_PLATFORM_UNAVAILABLE;
}

static smart_band_platform_result_t
noop_backlight(void *context, uint8_t percent)
{
  (void)context;
  (void)percent;
  return SMART_BAND_PLATFORM_UNAVAILABLE;
}

static smart_band_platform_result_t
noop_haptic_play(void *context, const smart_band_haptic_pulse_t *pulses,
                 size_t pulse_count)
{
  (void)context;
  (void)pulses;
  (void)pulse_count;
  return SMART_BAND_PLATFORM_UNAVAILABLE;
}

static smart_band_platform_result_t
noop_sync_start(void *context, smart_band_event_sink_t event_sink,
                void *event_context)
{
  (void)context;
  (void)event_sink;
  (void)event_context;
  return SMART_BAND_PLATFORM_UNAVAILABLE;
}

static smart_band_platform_result_t
noop_sync_send(void *context, const void *buffer, size_t size)
{
  (void)context;
  (void)buffer;
  (void)size;
  return SMART_BAND_PLATFORM_UNAVAILABLE;
}

static smart_band_platform_result_t
noop_sync_poll(void *context, void *buffer, size_t capacity,
               size_t *actual_size)
{
  (void)context;
  (void)buffer;
  (void)capacity;
  if (actual_size != NULL)
    {
      *actual_size = 0;
    }

  return SMART_BAND_PLATFORM_UNAVAILABLE;
}

static smart_band_sync_transport_state_t noop_sync_status(void *context)
{
  (void)context;
  return SMART_BAND_SYNC_STOPPED;
}

static size_t noop_sync_mtu(void *context)
{
  (void)context;
  return 0;
}

static const smart_band_storage_ops_t g_noop_storage_ops =
{
  noop_storage_read,
  noop_storage_write,
  noop_unavailable
};

static const smart_band_power_ops_t g_noop_power_ops =
{
  noop_display,
  noop_backlight,
  noop_unavailable
};

static const smart_band_haptic_ops_t g_noop_haptic_ops =
{
  noop_haptic_play,
  noop_unavailable
};

static const smart_band_sync_transport_ops_t g_noop_sync_ops =
{
  noop_sync_start,
  noop_unavailable,
  noop_sync_send,
  noop_sync_poll,
  noop_sync_status,
  noop_sync_mtu
};

void smart_band_platform_init_noop(smart_band_platform_t *platform)
{
  if (platform == NULL)
    {
      return;
    }

  memset(platform, 0, sizeof(*platform));
  platform->storage.ops = &g_noop_storage_ops;
  platform->power.ops = &g_noop_power_ops;
  platform->haptic.ops = &g_noop_haptic_ops;
  platform->sync.ops = &g_noop_sync_ops;
}
