#ifndef SMART_BAND_POWER_MANAGER_H
#define SMART_BAND_POWER_MANAGER_H

#include "smart_band_power.h"
#include "smart_band_power_policy.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  smart_band_power_policy_snapshot_t policy;
  smart_band_platform_result_t last_display_result;
  smart_band_platform_result_t last_backlight_result;
  smart_band_platform_result_t last_sleep_result;
  uint64_t next_render_ms;
  uint64_t next_heart_sample_ms;
  uint64_t apply_attempts;
  uint64_t apply_failures;
  bool platform_pending;
  bool platform_degraded;
} smart_band_power_manager_snapshot_t;

typedef struct
{
  smart_band_power_policy_t policy;
  smart_band_power_platform_t platform;
  smart_band_power_policy_snapshot_t desired;
  smart_band_platform_result_t last_display_result;
  smart_band_platform_result_t last_backlight_result;
  smart_band_platform_result_t last_sleep_result;
  uint64_t next_render_ms;
  uint64_t next_heart_sample_ms;
  uint64_t apply_attempts;
  uint64_t apply_failures;
  uint8_t applied_brightness_percent;
  bool applied_display_enabled;
  bool display_known;
  bool backlight_known;
  bool sleep_requested;
  bool platform_pending;
  bool platform_degraded;
  bool render_saturated_fired;
  bool heart_saturated_fired;
  bool initialized;
} smart_band_power_manager_t;

/* The runtime owns and serializes this service. UNAVAILABLE platform hooks are
 * recorded as a deliberate simulator/noop degradation; transient errors remain
 * pending and are retried on the next handle call. The *_due calls consume one
 * scheduling opportunity and must therefore have a single application owner. */
int smart_band_power_manager_init(
  smart_band_power_manager_t *manager,
  const smart_band_power_policy_config_t *config,
  const smart_band_power_platform_t *platform, uint64_t monotonic_ms);
void smart_band_power_manager_reset(smart_band_power_manager_t *manager);
void smart_band_power_manager_deinit(smart_band_power_manager_t *manager);
smart_band_power_policy_result_t smart_band_power_manager_handle(
  smart_band_power_manager_t *manager, uint64_t monotonic_ms,
  uint32_t events);
smart_band_power_policy_result_t smart_band_power_manager_wake(
  smart_band_power_manager_t *manager, uint64_t monotonic_ms,
  smart_band_power_wake_reason_t reason);
bool smart_band_power_manager_render_due(
  smart_band_power_manager_t *manager, uint64_t monotonic_ms,
  bool urgent);
bool smart_band_power_manager_heart_sample_due(
  smart_band_power_manager_t *manager, uint64_t monotonic_ms);
bool smart_band_power_manager_snapshot(
  const smart_band_power_manager_t *manager,
  smart_band_power_manager_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif

#endif
