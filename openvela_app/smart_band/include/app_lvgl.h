#ifndef SMART_BAND_APP_LVGL_H
#define SMART_BAND_APP_LVGL_H

#include "smart_band_notification_model.h"

#include <lvgl/lvgl.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int smart_band_lvgl_create(lv_obj_t *parent);
/* Stop and join every external notification producer before destroy. */
void smart_band_lvgl_destroy(void);
/* Thread-safe for concurrent producers only while the created application is
 * alive. This copies the input through the locked external inbox. */
bool smart_band_lvgl_post_notification_external(
  const smart_band_notification_input_t *input, uint32_t monotonic_ms);

#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
typedef struct
{
  uint32_t runtime_ticks;
  uint32_t event_pumps;
} smart_band_lvgl_diagnostics_t;

bool smart_band_lvgl_get_diagnostics(
  smart_band_lvgl_diagnostics_t *diagnostics);
bool smart_band_lvgl_diagnostics_is_idle(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
