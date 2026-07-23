#ifndef SMART_BAND_SYNC_LOOPBACK_H
#define SMART_BAND_SYNC_LOOPBACK_H

#include "smart_band_sync_transport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_SYNC_LOOPBACK_MTU 64
#define SMART_BAND_SYNC_LOOPBACK_CAPACITY 8

typedef struct
{
  uint8_t data[SMART_BAND_SYNC_LOOPBACK_MTU];
  size_t size;
} smart_band_sync_loopback_frame_t;

typedef struct
{
  unsigned int delay_polls;
  bool drop_next;
  bool duplicate_next;
  bool reorder_next_pair;
  bool disconnect_next;
} smart_band_sync_loopback_faults_t;

typedef struct
{
  smart_band_sync_loopback_frame_t frames[SMART_BAND_SYNC_LOOPBACK_CAPACITY];
  size_t head;
  size_t count;
  smart_band_event_sink_t event_sink;
  void *event_context;
  smart_band_sync_loopback_faults_t faults;
  smart_band_sync_loopback_frame_t held_frame;
  size_t dropped_frames;
  size_t duplicated_frames;
  bool holding_reorder_frame;
  bool started;
} smart_band_sync_loopback_t;

void smart_band_sync_loopback_init(smart_band_sync_loopback_t *loopback);
void smart_band_sync_loopback_set_faults(
  smart_band_sync_loopback_t *loopback,
  const smart_band_sync_loopback_faults_t *faults);
smart_band_sync_transport_t
smart_band_sync_loopback_transport(smart_band_sync_loopback_t *loopback);

#ifdef __cplusplus
}
#endif

#endif
