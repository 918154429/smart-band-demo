#include "smart_band_power_manager.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
  do                                                                         \
    {                                                                        \
      if (!(condition))                                                      \
        {                                                                    \
          fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__,         \
                  __LINE__, #condition);                                     \
          return 1;                                                          \
        }                                                                    \
    }                                                                        \
  while (0)

typedef struct
{
  char calls[64];
  size_t call_count;
  unsigned int display_calls;
  unsigned int backlight_calls;
  unsigned int sleep_calls;
  unsigned int failures_remaining;
  uint8_t brightness;
  bool display_enabled;
} fake_power_t;

static void record_call(fake_power_t *fake, char call)
{
  if (fake->call_count < sizeof(fake->calls))
    {
      fake->calls[fake->call_count++] = call;
    }
}

static smart_band_platform_result_t maybe_fail(fake_power_t *fake)
{
  if (fake->failures_remaining > 0u)
    {
      fake->failures_remaining--;
      return SMART_BAND_PLATFORM_BUSY;
    }
  return SMART_BAND_PLATFORM_OK;
}

static smart_band_platform_result_t fake_display(void *context, bool enabled)
{
  fake_power_t *fake = context;
  smart_band_platform_result_t result;

  fake->display_calls++;
  record_call(fake, enabled ? 'D' : 'd');
  result = maybe_fail(fake);
  if (result == SMART_BAND_PLATFORM_OK)
    {
      fake->display_enabled = enabled;
    }
  return result;
}

static smart_band_platform_result_t fake_backlight(void *context,
                                                    uint8_t percent)
{
  fake_power_t *fake = context;
  smart_band_platform_result_t result;

  fake->backlight_calls++;
  record_call(fake, percent == 0u ? 'b' : 'B');
  result = maybe_fail(fake);
  if (result == SMART_BAND_PLATFORM_OK)
    {
      fake->brightness = percent;
    }
  return result;
}

static smart_band_platform_result_t fake_sleep(void *context)
{
  fake_power_t *fake = context;

  fake->sleep_calls++;
  record_call(fake, 'S');
  return maybe_fail(fake);
}

static const smart_band_power_ops_t g_fake_ops =
{
  fake_display,
  fake_backlight,
  fake_sleep
};

static int init_manager(smart_band_power_manager_t *manager,
                        fake_power_t *fake,
                        smart_band_power_policy_config_t *config,
                        uint64_t now)
{
  smart_band_power_platform_t platform;

  memset(fake, 0, sizeof(*fake));
  platform.ops = &g_fake_ops;
  platform.context = fake;
  return smart_band_power_manager_init(manager, config, &platform, now);
}

static int test_platform_order_and_boundaries(void)
{
  smart_band_power_policy_config_t config;
  smart_band_power_manager_snapshot_t snapshot;
  smart_band_power_manager_t manager;
  fake_power_t fake;

  CHECK(smart_band_power_policy_default_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(init_manager(&manager, &fake, &config, 0u) == 0);
  CHECK(fake.call_count == 2u);
  CHECK(memcmp(fake.calls, "DB", 2u) == 0);
  CHECK(fake.display_enabled && fake.brightness == 100u);

  CHECK(smart_band_power_manager_handle(
          &manager, config.dim_timeout_ms - 1u,
          SMART_BAND_POWER_EVENT_TICK) == SMART_BAND_POWER_POLICY_OK);
  CHECK(fake.call_count == 2u);
  CHECK(smart_band_power_manager_handle(
          &manager, config.dim_timeout_ms,
          SMART_BAND_POWER_EVENT_TICK) == SMART_BAND_POWER_POLICY_OK);
  CHECK(fake.calls[fake.call_count - 1u] == 'B');
  CHECK(fake.brightness == 25u);
  CHECK(smart_band_power_manager_handle(
          &manager, config.off_timeout_ms,
          SMART_BAND_POWER_EVENT_TICK) == SMART_BAND_POWER_POLICY_OK);
  CHECK(fake.call_count >= 6u);
  CHECK(memcmp(&fake.calls[fake.call_count - 3u], "bdS", 3u) == 0);
  CHECK(!fake.display_enabled && fake.brightness == 0u);

  CHECK(smart_band_power_manager_wake(
          &manager, config.off_timeout_ms + 1u,
          SMART_BAND_POWER_WAKE_TOUCH) == SMART_BAND_POWER_POLICY_OK);
  CHECK(memcmp(&fake.calls[fake.call_count - 2u], "DB", 2u) == 0);
  CHECK(smart_band_power_manager_snapshot(&manager, &snapshot));
  CHECK(snapshot.policy.state == SMART_BAND_POWER_STATE_ACTIVE);
  CHECK(snapshot.policy.wake_reason == SMART_BAND_POWER_WAKE_TOUCH);
  CHECK(!snapshot.platform_pending);
  CHECK(!snapshot.platform_degraded);
  return 0;
}

static int test_failure_retry_and_noop(void)
{
  smart_band_power_policy_config_t config;
  smart_band_power_manager_snapshot_t snapshot;
  smart_band_power_manager_t manager;
  smart_band_power_platform_t noop = {0};
  fake_power_t fake;

  CHECK(smart_band_power_policy_default_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(init_manager(&manager, &fake, &config, 0u) == 0);
  fake.failures_remaining = 1u;
  CHECK(smart_band_power_manager_handle(
          &manager, config.dim_timeout_ms,
          SMART_BAND_POWER_EVENT_TICK) == SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_manager_snapshot(&manager, &snapshot));
  CHECK(snapshot.policy.state == SMART_BAND_POWER_STATE_DIMMED);
  CHECK(snapshot.platform_pending);
  CHECK(snapshot.apply_failures == 1u);
  CHECK(fake.brightness == 100u);

  CHECK(smart_band_power_manager_handle(
          &manager, config.dim_timeout_ms,
          SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_DUPLICATE_TIMESTAMP);
  CHECK(smart_band_power_manager_snapshot(&manager, &snapshot));
  CHECK(!snapshot.platform_pending);
  CHECK(fake.brightness == 25u);

  CHECK(smart_band_power_manager_init(&manager, &config, &noop, 0u) == 0);
  CHECK(smart_band_power_manager_snapshot(&manager, &snapshot));
  CHECK(snapshot.platform_degraded);
  CHECK(!snapshot.platform_pending);
  CHECK(smart_band_power_manager_handle(
          &manager, config.off_timeout_ms,
          SMART_BAND_POWER_EVENT_TICK) == SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_manager_snapshot(&manager, &snapshot));
  CHECK(snapshot.platform_degraded);
  CHECK(!snapshot.platform_pending);
  return 0;
}

static int test_schedule_and_workout_contract(void)
{
  smart_band_power_policy_config_t config;
  smart_band_power_manager_snapshot_t snapshot;
  smart_band_power_manager_t manager;
  fake_power_t fake;

  CHECK(smart_band_power_policy_default_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(init_manager(&manager, &fake, &config, 0u) == 0);
  CHECK(smart_band_power_manager_render_due(&manager, 0u, false));
  CHECK(!smart_band_power_manager_render_due(&manager, 15u, false));
  CHECK(smart_band_power_manager_render_due(&manager, 15u, true));
  CHECK(!smart_band_power_manager_render_due(&manager, 30u, false));
  CHECK(smart_band_power_manager_render_due(&manager, 31u, false));
  CHECK(smart_band_power_manager_heart_sample_due(&manager, 0u));
  CHECK(!smart_band_power_manager_heart_sample_due(&manager, 999u));
  CHECK(smart_band_power_manager_heart_sample_due(&manager, 1000u));

  CHECK(smart_band_power_manager_handle(
          &manager, config.off_timeout_ms,
          SMART_BAND_POWER_EVENT_TICK) == SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_manager_snapshot(&manager, &snapshot));
  CHECK(!snapshot.policy.display_enabled);
  CHECK(!snapshot.policy.allow_motion_sampling);
  CHECK(!snapshot.policy.allow_checkpoint);
  CHECK(snapshot.policy.heart_sampling_period_ms == 5000u);
  CHECK(smart_band_power_manager_heart_sample_due(
          &manager, config.off_timeout_ms));
  CHECK(!smart_band_power_manager_heart_sample_due(
          &manager, config.off_timeout_ms + 4999u));

  CHECK(smart_band_power_manager_handle(
          &manager, config.off_timeout_ms + 5000u,
          SMART_BAND_POWER_EVENT_WORKOUT_START) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_manager_snapshot(&manager, &snapshot));
  CHECK(snapshot.policy.state == SMART_BAND_POWER_STATE_SCREEN_OFF);
  CHECK(snapshot.policy.workout_active);
  CHECK(snapshot.policy.allow_motion_sampling);
  CHECK(snapshot.policy.allow_checkpoint);
  CHECK(snapshot.policy.heart_sampling_period_ms == 1000u);
  CHECK(smart_band_power_manager_heart_sample_due(
          &manager, config.off_timeout_ms + 5000u));
  return 0;
}

static int test_typed_wakes_and_invalid_inputs(void)
{
  const smart_band_power_wake_reason_t reasons[] =
  {
    SMART_BAND_POWER_WAKE_BUTTON,
    SMART_BAND_POWER_WAKE_TOUCH,
    SMART_BAND_POWER_WAKE_NOTIFICATION,
    SMART_BAND_POWER_WAKE_WRIST,
    SMART_BAND_POWER_WAKE_CHARGING
  };
  smart_band_power_policy_config_t config;
  smart_band_power_manager_snapshot_t snapshot;
  smart_band_power_manager_t manager;
  fake_power_t fake;
  size_t index;

  memset(&manager, 0, sizeof(manager));
  CHECK(smart_band_power_manager_handle(
          NULL, 0u, SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_NULL_ARGUMENT);
  CHECK(smart_band_power_manager_handle(
          &manager, 0u, SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_NOT_INITIALIZED);
  CHECK(smart_band_power_manager_init(NULL, NULL, NULL, 0u) != 0);
  CHECK(!smart_band_power_manager_snapshot(NULL, &snapshot));
  CHECK(!smart_band_power_manager_snapshot(&manager, &snapshot));
  CHECK(!smart_band_power_manager_render_due(NULL, 0u, false));
  CHECK(!smart_band_power_manager_heart_sample_due(NULL, 0u));

  CHECK(smart_band_power_policy_default_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(init_manager(&manager, &fake, &config, 0u) == 0);
  CHECK(smart_band_power_manager_wake(
          &manager, 1u, SMART_BAND_POWER_WAKE_NONE) ==
        SMART_BAND_POWER_POLICY_INVALID_EVENT);
  CHECK(smart_band_power_manager_wake(
          &manager, 1u, SMART_BAND_POWER_WAKE_COUNT) ==
        SMART_BAND_POWER_POLICY_INVALID_EVENT);
  for (index = 0u; index < sizeof(reasons) / sizeof(reasons[0]); index++)
    {
      uint64_t off_at = (uint64_t)(index + 1u) * 40000u;

      CHECK(smart_band_power_manager_handle(
              &manager, off_at, SMART_BAND_POWER_EVENT_TICK) ==
            SMART_BAND_POWER_POLICY_OK);
      CHECK(smart_band_power_manager_wake(&manager, off_at + 1u,
                                          reasons[index]) ==
            SMART_BAND_POWER_POLICY_OK);
      CHECK(smart_band_power_manager_snapshot(&manager, &snapshot));
      CHECK(snapshot.policy.state == SMART_BAND_POWER_STATE_ACTIVE);
      CHECK(snapshot.policy.wake_reason == reasons[index]);
    }
  smart_band_power_manager_reset(&manager);
  CHECK(!smart_band_power_manager_snapshot(&manager, &snapshot));
  return 0;
}

static int test_deinit_restores_display(void)
{
  smart_band_power_policy_config_t config;
  smart_band_power_manager_snapshot_t snapshot;
  smart_band_power_manager_t manager;
  fake_power_t fake;

  CHECK(smart_band_power_policy_default_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(init_manager(&manager, &fake, &config, 0u) == 0);
  CHECK(smart_band_power_manager_handle(
          &manager, config.off_timeout_ms,
          SMART_BAND_POWER_EVENT_TICK) == SMART_BAND_POWER_POLICY_OK);
  CHECK(!fake.display_enabled && fake.brightness == 0u);
  smart_band_power_manager_deinit(&manager);
  CHECK(fake.display_enabled && fake.brightness == 100u);
  CHECK(!smart_band_power_manager_snapshot(&manager, &snapshot));
  return 0;
}

static int test_pressure_and_saturation(void)
{
  smart_band_power_policy_config_t config;
  smart_band_power_manager_snapshot_t snapshot;
  smart_band_power_manager_t manager;
  fake_power_t fake;
  uint64_t now = 0u;
  unsigned int cycle;
  unsigned int pumps = 0u;
  unsigned int renders = 0u;

  CHECK(smart_band_power_policy_default_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);
  config.dim_timeout_ms = 1u;
  config.off_timeout_ms = 2u;
  config.large_time_step_ms = 10u;
  CHECK(init_manager(&manager, &fake, &config, now) == 0);
  for (cycle = 0u; cycle < 1000u; cycle++)
    {
      CHECK(smart_band_power_manager_handle(
              &manager, now + 1u, SMART_BAND_POWER_EVENT_TICK) ==
            SMART_BAND_POWER_POLICY_OK);
      CHECK(smart_band_power_manager_handle(
              &manager, now + 2u, SMART_BAND_POWER_EVENT_TICK) ==
            SMART_BAND_POWER_POLICY_OK);
      now += 3u;
      CHECK(smart_band_power_manager_wake(
              &manager, now, SMART_BAND_POWER_WAKE_TOUCH) ==
            SMART_BAND_POWER_POLICY_OK);
    }
  CHECK(smart_band_power_manager_snapshot(&manager, &snapshot));
  CHECK(snapshot.policy.transition_count == 3000u);
  CHECK(!snapshot.platform_pending);

  CHECK(init_manager(&manager, &fake, &config, 0u) == 0);
  CHECK(smart_band_power_manager_handle(
          &manager, config.off_timeout_ms,
          SMART_BAND_POWER_EVENT_TICK) == SMART_BAND_POWER_POLICY_OK);
  for (now = config.off_timeout_ms;
       now <= config.off_timeout_ms + 60000u; now += 50u)
    {
      pumps++;
      if (smart_band_power_manager_render_due(&manager, now, false))
        {
          renders++;
        }
    }
  CHECK(renders * 10u < pumps);

  CHECK(init_manager(&manager, &fake, &config, UINT64_MAX - 2u) == 0);
  CHECK(smart_band_power_manager_render_due(
          &manager, UINT64_MAX - 2u, false));
  CHECK(smart_band_power_manager_snapshot(&manager, &snapshot));
  CHECK(snapshot.next_render_ms == UINT64_MAX);
  CHECK(!smart_band_power_manager_render_due(
          &manager, UINT64_MAX - 1u, false));
  CHECK(smart_band_power_manager_render_due(&manager, UINT64_MAX, false));
  CHECK(!smart_band_power_manager_render_due(&manager, UINT64_MAX, false));
  CHECK(smart_band_power_manager_heart_sample_due(
          &manager, UINT64_MAX - 2u));
  CHECK(smart_band_power_manager_heart_sample_due(
          &manager, UINT64_MAX));
  CHECK(!smart_band_power_manager_heart_sample_due(
          &manager, UINT64_MAX));
  return 0;
}

int main(void)
{
  CHECK(test_platform_order_and_boundaries() == 0);
  CHECK(test_failure_retry_and_noop() == 0);
  CHECK(test_schedule_and_workout_contract() == 0);
  CHECK(test_typed_wakes_and_invalid_inputs() == 0);
  CHECK(test_deinit_restores_display() == 0);
  CHECK(test_pressure_and_saturation() == 0);
  puts("smart band power manager tests passed");
  return 0;
}
