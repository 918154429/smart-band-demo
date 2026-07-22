#ifndef SMART_BAND_NOTIFICATION_DEMO_H
#define SMART_BAND_NOTIFICATION_DEMO_H

#include "smart_band_notification_model.h"

#include <stdbool.h>
#include <stdint.h>

bool smart_band_notification_demo_build(
  uint32_t seed, uint32_t sequence,
  smart_band_notification_t *notification);
smart_band_notification_put_result_t smart_band_notification_demo_inject(
  smart_band_notification_model_t *model, uint32_t seed, uint32_t sequence);

#endif
