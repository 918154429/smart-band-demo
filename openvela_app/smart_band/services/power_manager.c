#include "smart_band_power_manager.h"

#include <limits.h>
#include <string.h>

static uint64_t add_period(uint64_t now, uint32_t period_ms)
{
  if (UINT64_MAX - now < period_ms)
    {
      return UINT64_MAX;
    }

  return now + period_ms;
}

static bool platform_result_accepted(smart_band_power_manager_t *manager,
                                     smart_band_platform_result_t result)
{
  if (result == SMART_BAND_PLATFORM_UNAVAILABLE)
    {
      manager->platform_degraded = true;
      return true;
    }

  if (result != SMART_BAND_PLATFORM_OK)
    {
      if (manager->apply_failures < UINT64_MAX)
        {
          manager->apply_failures++;
        }
      return false;
    }

  return true;
}

static smart_band_platform_result_t set_display(
  smart_band_power_manager_t *manager, bool enabled)
{
  smart_band_platform_result_t result = SMART_BAND_PLATFORM_UNAVAILABLE;

  if (manager->platform.ops != NULL &&
      manager->platform.ops->set_display_enabled != NULL)
    {
      result = manager->platform.ops->set_display_enabled(
        manager->platform.context, enabled);
    }
  manager->last_display_result = result;
  return result;
}

static smart_band_platform_result_t set_backlight(
  smart_band_power_manager_t *manager, uint8_t percent)
{
  smart_band_platform_result_t result = SMART_BAND_PLATFORM_UNAVAILABLE;

  if (manager->platform.ops != NULL &&
      manager->platform.ops->set_backlight != NULL)
    {
      result = manager->platform.ops->set_backlight(
        manager->platform.context, percent);
    }
  manager->last_backlight_result = result;
  return result;
}

static smart_band_platform_result_t request_sleep(
  smart_band_power_manager_t *manager)
{
  smart_band_platform_result_t result = SMART_BAND_PLATFORM_UNAVAILABLE;

  if (manager->platform.ops != NULL &&
      manager->platform.ops->request_sleep != NULL)
    {
      result = manager->platform.ops->request_sleep(
        manager->platform.context);
    }
  manager->last_sleep_result = result;
  return result;
}

static void apply_platform_state(smart_band_power_manager_t *manager)
{
  bool accepted;

  manager->platform_pending = false;
  if (manager->apply_attempts < UINT64_MAX)
    {
      manager->apply_attempts++;
    }

  /* Wake order prevents a backlight update targeting a disabled display.
   * Sleep order prevents a visible flash while the display is disabled. */
  if (manager->desired.display_enabled)
    {
      if (!manager->display_known || !manager->applied_display_enabled)
        {
          accepted = platform_result_accepted(manager,
                                               set_display(manager, true));
          if (!accepted)
            {
              manager->platform_pending = true;
              return;
            }
          manager->display_known = true;
          manager->applied_display_enabled = true;
        }

      if (!manager->backlight_known ||
          manager->applied_brightness_percent !=
            manager->desired.brightness_percent)
        {
          accepted = platform_result_accepted(
            manager, set_backlight(manager,
                                   manager->desired.brightness_percent));
          if (!accepted)
            {
              manager->platform_pending = true;
              return;
            }
          manager->backlight_known = true;
          manager->applied_brightness_percent =
            manager->desired.brightness_percent;
        }
      manager->sleep_requested = false;
      return;
    }

  if (!manager->backlight_known || manager->applied_brightness_percent != 0u)
    {
      accepted = platform_result_accepted(manager, set_backlight(manager, 0u));
      if (!accepted)
        {
          manager->platform_pending = true;
          return;
        }
      manager->backlight_known = true;
      manager->applied_brightness_percent = 0u;
    }

  if (!manager->display_known || manager->applied_display_enabled)
    {
      accepted = platform_result_accepted(manager,
                                           set_display(manager, false));
      if (!accepted)
        {
          manager->platform_pending = true;
          return;
        }
      manager->display_known = true;
      manager->applied_display_enabled = false;
    }

  if (!manager->sleep_requested)
    {
      accepted = platform_result_accepted(manager, request_sleep(manager));
      if (!accepted)
        {
          manager->platform_pending = true;
          return;
        }
      manager->sleep_requested = true;
    }
}

static uint32_t wake_event(smart_band_power_wake_reason_t reason)
{
  switch (reason)
    {
      case SMART_BAND_POWER_WAKE_BUTTON:
        return SMART_BAND_POWER_EVENT_BUTTON;
      case SMART_BAND_POWER_WAKE_TOUCH:
        return SMART_BAND_POWER_EVENT_TOUCH;
      case SMART_BAND_POWER_WAKE_NOTIFICATION:
        return SMART_BAND_POWER_EVENT_NOTIFICATION;
      case SMART_BAND_POWER_WAKE_WRIST:
        return SMART_BAND_POWER_EVENT_WRIST;
      case SMART_BAND_POWER_WAKE_CHARGING:
        return SMART_BAND_POWER_EVENT_CHARGING;
      case SMART_BAND_POWER_WAKE_NONE:
      case SMART_BAND_POWER_WAKE_COUNT:
      default:
        return 0u;
    }
}

int smart_band_power_manager_init(
  smart_band_power_manager_t *manager,
  const smart_band_power_policy_config_t *config,
  const smart_band_power_platform_t *platform, uint64_t monotonic_ms)
{
  smart_band_power_policy_config_t default_config;
  smart_band_power_policy_result_t result;

  if (manager == NULL)
    {
      return -1;
    }

  memset(manager, 0, sizeof(*manager));
  if (config == NULL)
    {
      if (smart_band_power_policy_default_config(&default_config) !=
          SMART_BAND_POWER_POLICY_OK)
        {
          return -1;
        }
      config = &default_config;
    }
  if (platform != NULL)
    {
      manager->platform = *platform;
    }
  result = smart_band_power_policy_init(&manager->policy, config,
                                        monotonic_ms);
  if (result != SMART_BAND_POWER_POLICY_OK ||
      smart_band_power_policy_snapshot(&manager->policy,
                                       &manager->desired) !=
        SMART_BAND_POWER_POLICY_OK)
    {
      memset(manager, 0, sizeof(*manager));
      return -1;
    }

  manager->last_display_result = SMART_BAND_PLATFORM_UNAVAILABLE;
  manager->last_backlight_result = SMART_BAND_PLATFORM_UNAVAILABLE;
  manager->last_sleep_result = SMART_BAND_PLATFORM_UNAVAILABLE;
  manager->next_render_ms = monotonic_ms;
  manager->next_heart_sample_ms = monotonic_ms;
  manager->initialized = true;
  apply_platform_state(manager);
  return 0;
}

void smart_band_power_manager_reset(smart_band_power_manager_t *manager)
{
  if (manager != NULL)
    {
      memset(manager, 0, sizeof(*manager));
    }
}

void smart_band_power_manager_deinit(smart_band_power_manager_t *manager)
{
  if (manager != NULL && manager->initialized)
    {
      (void)smart_band_power_manager_wake(
        manager, manager->desired.monotonic_ms,
        SMART_BAND_POWER_WAKE_BUTTON);
      smart_band_power_manager_reset(manager);
    }
}

smart_band_power_policy_result_t smart_band_power_manager_handle(
  smart_band_power_manager_t *manager, uint64_t monotonic_ms,
  uint32_t events)
{
  smart_band_power_policy_snapshot_t before;
  smart_band_power_policy_result_t result;

  if (manager == NULL)
    {
      return SMART_BAND_POWER_POLICY_NULL_ARGUMENT;
    }
  if (!manager->initialized)
    {
      return SMART_BAND_POWER_POLICY_NOT_INITIALIZED;
    }

  before = manager->desired;
  result = smart_band_power_policy_handle(&manager->policy, monotonic_ms,
                                          events);
  if (result != SMART_BAND_POWER_POLICY_OK &&
      result != SMART_BAND_POWER_POLICY_DUPLICATE_TIMESTAMP &&
      result != SMART_BAND_POWER_POLICY_LARGE_TIME_JUMP)
    {
      return result;
    }
  if (smart_band_power_policy_snapshot(&manager->policy,
                                       &manager->desired) !=
      SMART_BAND_POWER_POLICY_OK)
    {
      return SMART_BAND_POWER_POLICY_INVALID_STATE;
    }

  if (before.state != manager->desired.state ||
      before.workout_active != manager->desired.workout_active)
    {
      manager->next_render_ms = monotonic_ms;
      manager->next_heart_sample_ms = monotonic_ms;
      manager->render_saturated_fired = false;
      manager->heart_saturated_fired = false;
    }
  if (manager->platform_pending ||
      before.display_enabled != manager->desired.display_enabled ||
      before.brightness_percent != manager->desired.brightness_percent)
    {
      apply_platform_state(manager);
    }
  return result;
}

smart_band_power_policy_result_t smart_band_power_manager_wake(
  smart_band_power_manager_t *manager, uint64_t monotonic_ms,
  smart_band_power_wake_reason_t reason)
{
  uint32_t event = wake_event(reason);

  if (event == 0u)
    {
      return SMART_BAND_POWER_POLICY_INVALID_EVENT;
    }
  return smart_band_power_manager_handle(manager, monotonic_ms, event);
}

bool smart_band_power_manager_render_due(
  smart_band_power_manager_t *manager, uint64_t monotonic_ms, bool urgent)
{
  if (manager == NULL || !manager->initialized ||
      monotonic_ms < manager->desired.monotonic_ms)
    {
      return false;
    }
  if (!urgent && monotonic_ms < manager->next_render_ms)
    {
      return false;
    }
  if (!urgent && manager->next_render_ms == UINT64_MAX &&
      manager->render_saturated_fired)
    {
      return false;
    }

  manager->next_render_ms = add_period(
    monotonic_ms, manager->desired.render_period_ms);
  manager->render_saturated_fired =
    manager->next_render_ms == UINT64_MAX && monotonic_ms == UINT64_MAX;
  return true;
}

bool smart_band_power_manager_heart_sample_due(
  smart_band_power_manager_t *manager, uint64_t monotonic_ms)
{
  if (manager == NULL || !manager->initialized ||
      monotonic_ms < manager->desired.monotonic_ms ||
      monotonic_ms < manager->next_heart_sample_ms)
    {
      return false;
    }
  if (manager->next_heart_sample_ms == UINT64_MAX &&
      manager->heart_saturated_fired)
    {
      return false;
    }

  manager->next_heart_sample_ms = add_period(
    monotonic_ms, manager->desired.heart_sampling_period_ms);
  manager->heart_saturated_fired =
    manager->next_heart_sample_ms == UINT64_MAX &&
    monotonic_ms == UINT64_MAX;
  return true;
}

bool smart_band_power_manager_snapshot(
  const smart_band_power_manager_t *manager,
  smart_band_power_manager_snapshot_t *snapshot)
{
  if (manager == NULL || snapshot == NULL || !manager->initialized)
    {
      return false;
    }

  memset(snapshot, 0, sizeof(*snapshot));
  snapshot->policy = manager->desired;
  snapshot->last_display_result = manager->last_display_result;
  snapshot->last_backlight_result = manager->last_backlight_result;
  snapshot->last_sleep_result = manager->last_sleep_result;
  snapshot->next_render_ms = manager->next_render_ms;
  snapshot->next_heart_sample_ms = manager->next_heart_sample_ms;
  snapshot->apply_attempts = manager->apply_attempts;
  snapshot->apply_failures = manager->apply_failures;
  snapshot->platform_pending = manager->platform_pending;
  snapshot->platform_degraded = manager->platform_degraded;
  return true;
}
