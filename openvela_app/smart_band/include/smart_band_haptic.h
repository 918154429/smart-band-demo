#ifndef SMART_BAND_HAPTIC_H
#define SMART_BAND_HAPTIC_H

#include "smart_band_platform_result.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint16_t on_ms;
  uint16_t off_ms;
  uint8_t strength;
} smart_band_haptic_pulse_t;

typedef struct
{
  smart_band_platform_result_t (*play)(
    void *context, const smart_band_haptic_pulse_t *pulses,
    size_t pulse_count);
  smart_band_platform_result_t (*stop)(void *context);
} smart_band_haptic_ops_t;

typedef struct
{
  const smart_band_haptic_ops_t *ops;
  void *context;
} smart_band_haptic_t;

#ifdef __cplusplus
}
#endif

#endif
