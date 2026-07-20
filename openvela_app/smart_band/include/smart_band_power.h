#ifndef SMART_BAND_POWER_H
#define SMART_BAND_POWER_H

#include "smart_band_platform_result.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  smart_band_platform_result_t (*set_display_enabled)(void *context,
                                                       bool enabled);
  smart_band_platform_result_t (*set_backlight)(void *context,
                                                 uint8_t percent);
  smart_band_platform_result_t (*request_sleep)(void *context);
} smart_band_power_ops_t;

typedef struct
{
  const smart_band_power_ops_t *ops;
  void *context;
} smart_band_power_platform_t;

#ifdef __cplusplus
}
#endif

#endif
