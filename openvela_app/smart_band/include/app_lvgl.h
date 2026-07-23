#ifndef SMART_BAND_APP_LVGL_H
#define SMART_BAND_APP_LVGL_H

#include "smart_band_notification_model.h"
#include "smart_band_haptic.h"

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
  const smart_band_notification_utf8_input_t *input,
  uint32_t monotonic_ms);
/* UI-thread controller hook. External producers must continue to use the
 * locked notification ingress above. */
bool smart_band_lvgl_set_notification_policy(
  const smart_band_notification_policy_t *policy);

#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
typedef struct
{
  uint32_t runtime_ticks;
  uint32_t event_pumps;
  uint32_t haptic_events;
  uint32_t wake_requests;
  uint32_t haptic_retries;
  uint32_t haptic_log_dropped;
  uint32_t wake_log_dropped;
  uint32_t last_haptic_notification_id;
  uint32_t last_haptic_generation;
  uint32_t last_wake_notification_id;
  uint32_t last_wake_generation;
  smart_band_notification_haptic_t last_haptic;
  smart_band_platform_result_t last_haptic_platform_result;
} smart_band_lvgl_diagnostics_t;

typedef bool (*smart_band_lvgl_effect_log_for_test_t)(void *context,
                                                       const char *line);

bool smart_band_lvgl_get_diagnostics(
  smart_band_lvgl_diagnostics_t *diagnostics);
bool smart_band_lvgl_diagnostics_is_idle(void);
bool smart_band_lvgl_set_haptic_adapter_for_test(
  const smart_band_haptic_t *haptic);
bool smart_band_lvgl_set_effect_logger_for_test(
  smart_band_lvgl_effect_log_for_test_t logger, void *context);
#endif

#ifdef __cplusplus
}
#endif

#endif
