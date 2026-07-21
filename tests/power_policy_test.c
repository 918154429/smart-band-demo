#include "smart_band_power_policy.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
  do                                                                         \
    {                                                                        \
      if (!(condition))                                                      \
        {                                                                    \
          fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
                  #condition);                                               \
          return 1;                                                          \
        }                                                                    \
    }                                                                        \
  while (0)

static int reset_and_turn_off(smart_band_power_policy_t *policy,
                              uint64_t base_ms)
{
  CHECK(smart_band_power_policy_reset(policy, base_ms) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_handle(
          policy, base_ms + policy->config.off_timeout_ms,
          SMART_BAND_POWER_EVENT_TICK) == SMART_BAND_POWER_POLICY_OK);
  CHECK(policy->state == SMART_BAND_POWER_STATE_SCREEN_OFF);
  return 0;
}

static int test_defaults_and_validation(void)
{
  smart_band_power_policy_config_t config;
  smart_band_power_policy_config_t invalid;
  smart_band_power_policy_t policy;
  smart_band_power_policy_t before;

  CHECK(smart_band_power_policy_default_config(NULL) ==
        SMART_BAND_POWER_POLICY_NULL_ARGUMENT);
  CHECK(smart_band_power_policy_validate_config(NULL) ==
        SMART_BAND_POWER_POLICY_NULL_ARGUMENT);
  CHECK(smart_band_power_policy_default_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(config.dim_timeout_ms == 10000u);
  CHECK(config.off_timeout_ms == 30000u);
  CHECK(config.large_time_step_ms == 300000u);
  CHECK(config.idle.states[SMART_BAND_POWER_STATE_ACTIVE]
          .brightness_percent == 100u);
  CHECK(config.idle.states[SMART_BAND_POWER_STATE_DIMMED]
          .brightness_percent == 25u);
  CHECK(config.idle.states[SMART_BAND_POWER_STATE_SCREEN_OFF]
          .brightness_percent == 0u);
  CHECK(config.workout.states[SMART_BAND_POWER_STATE_SCREEN_OFF]
          .allow_motion_sampling);
  CHECK(config.workout.states[SMART_BAND_POWER_STATE_SCREEN_OFF]
          .allow_checkpoint);
  CHECK(!config.idle.states[SMART_BAND_POWER_STATE_SCREEN_OFF]
           .allow_motion_sampling);
  CHECK(!config.idle.states[SMART_BAND_POWER_STATE_SCREEN_OFF]
           .allow_checkpoint);
  CHECK(smart_band_power_policy_validate_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);

  memset(&policy, 0xa5, sizeof(policy));
  before = policy;
  CHECK(smart_band_power_policy_init(NULL, &config, 0u) ==
        SMART_BAND_POWER_POLICY_NULL_ARGUMENT);
  CHECK(smart_band_power_policy_init(&policy, NULL, 0u) ==
        SMART_BAND_POWER_POLICY_NULL_ARGUMENT);
  CHECK(memcmp(&policy, &before, sizeof(policy)) == 0);

  invalid = config;
  invalid.dim_timeout_ms = 0u;
  CHECK(smart_band_power_policy_validate_config(&invalid) ==
        SMART_BAND_POWER_POLICY_INVALID_CONFIG);
  invalid = config;
  invalid.off_timeout_ms = invalid.dim_timeout_ms;
  CHECK(smart_band_power_policy_validate_config(&invalid) ==
        SMART_BAND_POWER_POLICY_INVALID_CONFIG);
  invalid = config;
  invalid.large_time_step_ms = invalid.off_timeout_ms - 1u;
  CHECK(smart_band_power_policy_validate_config(&invalid) ==
        SMART_BAND_POWER_POLICY_INVALID_CONFIG);
  invalid = config;
  invalid.idle.states[SMART_BAND_POWER_STATE_ACTIVE].brightness_percent =
    101u;
  CHECK(smart_band_power_policy_validate_config(&invalid) ==
        SMART_BAND_POWER_POLICY_INVALID_CONFIG);
  invalid = config;
  invalid.idle.states[SMART_BAND_POWER_STATE_ACTIVE].brightness_percent = 0u;
  CHECK(smart_band_power_policy_validate_config(&invalid) ==
        SMART_BAND_POWER_POLICY_INVALID_CONFIG);
  invalid = config;
  invalid.idle.states[SMART_BAND_POWER_STATE_DIMMED].brightness_percent =
    invalid.idle.states[SMART_BAND_POWER_STATE_ACTIVE].brightness_percent +
    1u;
  CHECK(smart_band_power_policy_validate_config(&invalid) ==
        SMART_BAND_POWER_POLICY_INVALID_CONFIG);
  invalid = config;
  invalid.idle.states[SMART_BAND_POWER_STATE_SCREEN_OFF]
    .brightness_percent = 1u;
  CHECK(smart_band_power_policy_validate_config(&invalid) ==
        SMART_BAND_POWER_POLICY_INVALID_CONFIG);
  invalid = config;
  invalid.workout.states[SMART_BAND_POWER_STATE_ACTIVE].render_period_ms = 0u;
  CHECK(smart_band_power_policy_validate_config(&invalid) ==
        SMART_BAND_POWER_POLICY_INVALID_CONFIG);
  invalid = config;
  invalid.workout.states[SMART_BAND_POWER_STATE_DIMMED]
    .heart_sampling_period_ms = 0u;
  CHECK(smart_band_power_policy_validate_config(&invalid) ==
        SMART_BAND_POWER_POLICY_INVALID_CONFIG);
  invalid = config;
  invalid.workout.states[SMART_BAND_POWER_STATE_SCREEN_OFF] =
    invalid.idle.states[SMART_BAND_POWER_STATE_SCREEN_OFF];
  CHECK(smart_band_power_policy_validate_config(&invalid) ==
        SMART_BAND_POWER_POLICY_INVALID_CONFIG);

  before = policy;
  CHECK(smart_band_power_policy_init(&policy, &invalid, 0u) ==
        SMART_BAND_POWER_POLICY_INVALID_CONFIG);
  CHECK(memcmp(&policy, &before, sizeof(policy)) == 0);
  return 0;
}

static int test_boundaries_and_transitions(void)
{
  smart_band_power_policy_config_t config;
  smart_band_power_policy_t policy;
  smart_band_power_policy_snapshot_t snapshot;

  CHECK(smart_band_power_policy_default_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_init(&policy, &config, 100u) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_snapshot(&policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(snapshot.state == SMART_BAND_POWER_STATE_ACTIVE);
  CHECK(snapshot.display_enabled);
  CHECK(snapshot.brightness_percent == 100u);
  CHECK(snapshot.wake_reason == SMART_BAND_POWER_WAKE_NONE);
  CHECK(snapshot.last_activity_ms == 100u);
  CHECK(snapshot.monotonic_ms == 100u);

  CHECK(smart_band_power_policy_handle(&policy, 10099u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(policy.state == SMART_BAND_POWER_STATE_ACTIVE);
  CHECK(smart_band_power_policy_handle(&policy, 10100u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(policy.state == SMART_BAND_POWER_STATE_DIMMED);
  CHECK(smart_band_power_policy_snapshot(&policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(snapshot.display_enabled);
  CHECK(snapshot.brightness_percent == 25u);

  CHECK(smart_band_power_policy_reset(&policy, 100u) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_handle(&policy, 10101u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(policy.state == SMART_BAND_POWER_STATE_DIMMED);

  CHECK(smart_band_power_policy_reset(&policy, 100u) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_handle(&policy, 30099u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(policy.state == SMART_BAND_POWER_STATE_DIMMED);
  CHECK(smart_band_power_policy_handle(&policy, 30100u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(policy.state == SMART_BAND_POWER_STATE_SCREEN_OFF);
  CHECK(smart_band_power_policy_snapshot(&policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(!snapshot.display_enabled);
  CHECK(snapshot.brightness_percent == 0u);

  CHECK(smart_band_power_policy_reset(&policy, 100u) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_handle(&policy, 30101u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(policy.state == SMART_BAND_POWER_STATE_SCREEN_OFF);

  CHECK(smart_band_power_policy_reset(&policy, 0u) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_handle(&policy, 10000u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_handle(&policy, 30000u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(policy.transition_count == 2u);
  return 0;
}

static int check_wake(smart_band_power_policy_t *policy, uint32_t events,
                      smart_band_power_wake_reason_t expected)
{
  smart_band_power_policy_snapshot_t snapshot;
  uint64_t now;

  CHECK(reset_and_turn_off(policy, 0u) == 0);
  now = policy->last_monotonic_ms + 1u;
  CHECK(smart_band_power_policy_handle(policy, now, events) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_snapshot(policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(snapshot.state == SMART_BAND_POWER_STATE_ACTIVE);
  CHECK(snapshot.wake_reason == expected);
  CHECK(snapshot.last_activity_ms == now);
  return 0;
}

static int test_wake_reasons_and_priority(void)
{
  smart_band_power_policy_config_t config;
  smart_band_power_policy_t policy;

  CHECK(smart_band_power_policy_default_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_init(&policy, &config, 0u) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(check_wake(&policy, SMART_BAND_POWER_EVENT_TOUCH,
                   SMART_BAND_POWER_WAKE_TOUCH) == 0);
  CHECK(check_wake(&policy, SMART_BAND_POWER_EVENT_BUTTON,
                   SMART_BAND_POWER_WAKE_BUTTON) == 0);
  CHECK(check_wake(&policy, SMART_BAND_POWER_EVENT_WRIST,
                   SMART_BAND_POWER_WAKE_WRIST) == 0);
  CHECK(check_wake(&policy, SMART_BAND_POWER_EVENT_NOTIFICATION,
                   SMART_BAND_POWER_WAKE_NOTIFICATION) == 0);
  CHECK(check_wake(&policy, SMART_BAND_POWER_EVENT_CHARGING,
                   SMART_BAND_POWER_WAKE_CHARGING) == 0);
  CHECK(check_wake(
          &policy,
          SMART_BAND_POWER_EVENT_TICK | SMART_BAND_POWER_EVENT_CHARGING |
            SMART_BAND_POWER_EVENT_WRIST |
            SMART_BAND_POWER_EVENT_NOTIFICATION |
            SMART_BAND_POWER_EVENT_TOUCH | SMART_BAND_POWER_EVENT_BUTTON,
          SMART_BAND_POWER_WAKE_BUTTON) == 0);
  CHECK(check_wake(&policy,
                   SMART_BAND_POWER_EVENT_CHARGING |
                     SMART_BAND_POWER_EVENT_WRIST |
                     SMART_BAND_POWER_EVENT_NOTIFICATION |
                     SMART_BAND_POWER_EVENT_TOUCH,
                   SMART_BAND_POWER_WAKE_TOUCH) == 0);
  CHECK(check_wake(&policy,
                   SMART_BAND_POWER_EVENT_CHARGING |
                     SMART_BAND_POWER_EVENT_WRIST |
                     SMART_BAND_POWER_EVENT_NOTIFICATION,
                   SMART_BAND_POWER_WAKE_NOTIFICATION) == 0);
  return 0;
}

static int test_workout_profiles(void)
{
  smart_band_power_policy_config_t config;
  smart_band_power_policy_t policy;
  smart_band_power_policy_snapshot_t snapshot;

  CHECK(smart_band_power_policy_default_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_init(&policy, &config, 0u) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_handle(
          &policy, 1u, SMART_BAND_POWER_EVENT_WORKOUT_START) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_snapshot(&policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(snapshot.state == SMART_BAND_POWER_STATE_ACTIVE);
  CHECK(snapshot.workout_active);
  CHECK(snapshot.heart_sampling_period_ms == 1000u);

  CHECK(smart_band_power_policy_handle(&policy, 10000u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_snapshot(&policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(snapshot.state == SMART_BAND_POWER_STATE_DIMMED);
  CHECK(snapshot.brightness_percent == 35u);
  CHECK(smart_band_power_policy_handle(
          &policy, 10001u, SMART_BAND_POWER_EVENT_WORKOUT_STOP) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_snapshot(&policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(!snapshot.workout_active);
  CHECK(snapshot.state == SMART_BAND_POWER_STATE_DIMMED);
  CHECK(snapshot.brightness_percent == 25u);

  CHECK(reset_and_turn_off(&policy, 0u) == 0);
  CHECK(smart_band_power_policy_snapshot(&policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(!snapshot.allow_motion_sampling);
  CHECK(!snapshot.allow_checkpoint);
  CHECK(!snapshot.allow_sync);
  CHECK(snapshot.render_period_ms >=
        config.idle.states[SMART_BAND_POWER_STATE_ACTIVE].render_period_ms *
          10u);
  CHECK(snapshot.heart_sampling_period_ms >=
        config.idle.states[SMART_BAND_POWER_STATE_ACTIVE]
            .heart_sampling_period_ms *
          5u);

  CHECK(smart_band_power_policy_handle(
          &policy, 30001u, SMART_BAND_POWER_EVENT_WORKOUT_START) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_snapshot(&policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(snapshot.state == SMART_BAND_POWER_STATE_SCREEN_OFF);
  CHECK(snapshot.workout_active);
  CHECK(snapshot.allow_motion_sampling);
  CHECK(snapshot.allow_checkpoint);
  CHECK(!snapshot.allow_sync);
  CHECK(snapshot.heart_sampling_period_ms == 1000u);
  CHECK(smart_band_power_policy_handle(
          &policy, 30002u, SMART_BAND_POWER_EVENT_WORKOUT_STOP) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(policy.state == SMART_BAND_POWER_STATE_SCREEN_OFF);
  CHECK(!policy.workout_active);
  return 0;
}

static int test_time_and_invalid_inputs(void)
{
  smart_band_power_policy_config_t config;
  smart_band_power_policy_t policy;
  smart_band_power_policy_t before;
  smart_band_power_policy_snapshot_t snapshot;

  memset(&policy, 0, sizeof(policy));
  CHECK(smart_band_power_policy_handle(&policy, 0u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_NOT_INITIALIZED);
  CHECK(smart_band_power_policy_reset(&policy, 0u) ==
        SMART_BAND_POWER_POLICY_NOT_INITIALIZED);
  CHECK(smart_band_power_policy_snapshot(&policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_NOT_INITIALIZED);
  CHECK(smart_band_power_policy_snapshot(NULL, &snapshot) ==
        SMART_BAND_POWER_POLICY_NULL_ARGUMENT);
  CHECK(smart_band_power_policy_snapshot(&policy, NULL) ==
        SMART_BAND_POWER_POLICY_NULL_ARGUMENT);
  CHECK(smart_band_power_policy_handle(NULL, 0u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_NULL_ARGUMENT);
  CHECK(smart_band_power_policy_reset(NULL, 0u) ==
        SMART_BAND_POWER_POLICY_NULL_ARGUMENT);

  CHECK(smart_band_power_policy_default_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_init(&policy, &config, 100u) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_handle(&policy, 100u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_DUPLICATE_TIMESTAMP);
  CHECK(policy.state == SMART_BAND_POWER_STATE_ACTIVE);
  CHECK(smart_band_power_policy_handle(&policy, 100u,
                                       SMART_BAND_POWER_EVENT_TOUCH) ==
        SMART_BAND_POWER_POLICY_DUPLICATE_TIMESTAMP);
  CHECK(policy.wake_reason == SMART_BAND_POWER_WAKE_TOUCH);

  before = policy;
  CHECK(smart_band_power_policy_handle(&policy, 99u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_TIME_ROLLBACK);
  CHECK(memcmp(&policy, &before, sizeof(policy)) == 0);
  CHECK(smart_band_power_policy_handle(&policy, 101u, 0u) ==
        SMART_BAND_POWER_POLICY_INVALID_EVENT);
  CHECK(memcmp(&policy, &before, sizeof(policy)) == 0);
  CHECK(smart_band_power_policy_handle(&policy, 101u, 1u << 30) ==
        SMART_BAND_POWER_POLICY_INVALID_EVENT);
  CHECK(memcmp(&policy, &before, sizeof(policy)) == 0);
  CHECK(smart_band_power_policy_handle(
          &policy, 101u, SMART_BAND_POWER_EVENT_WORKOUT_START |
                          SMART_BAND_POWER_EVENT_WORKOUT_STOP) ==
        SMART_BAND_POWER_POLICY_INVALID_EVENT);
  CHECK(memcmp(&policy, &before, sizeof(policy)) == 0);

  CHECK(smart_band_power_policy_reset(&policy, 0u) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(smart_band_power_policy_handle(
          &policy, config.large_time_step_ms + 1u,
          SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_LARGE_TIME_JUMP);
  CHECK(policy.state == SMART_BAND_POWER_STATE_SCREEN_OFF);

  CHECK(smart_band_power_policy_reset(&policy, 0u) ==
        SMART_BAND_POWER_POLICY_OK);
  policy.transition_count = UINT64_MAX;
  CHECK(smart_band_power_policy_handle(&policy, config.dim_timeout_ms,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_OK);
  CHECK(policy.state == SMART_BAND_POWER_STATE_DIMMED);
  CHECK(policy.transition_count == UINT64_MAX);

  policy.state = (smart_band_power_state_t)99;
  CHECK(smart_band_power_policy_snapshot(&policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_INVALID_STATE);
  CHECK(smart_band_power_policy_handle(&policy, policy.last_monotonic_ms + 1u,
                                       SMART_BAND_POWER_EVENT_TICK) ==
        SMART_BAND_POWER_POLICY_INVALID_STATE);
  policy.state = SMART_BAND_POWER_STATE_SCREEN_OFF;
  policy.wake_reason = (smart_band_power_wake_reason_t)99;
  CHECK(smart_band_power_policy_snapshot(&policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_INVALID_STATE);
  policy.wake_reason = SMART_BAND_POWER_WAKE_NONE;
  policy.last_activity_ms = policy.last_monotonic_ms + 1u;
  CHECK(smart_band_power_policy_snapshot(&policy, &snapshot) ==
        SMART_BAND_POWER_POLICY_INVALID_STATE);
  return 0;
}

static int test_accelerated_cycles(void)
{
  smart_band_power_policy_config_t config;
  smart_band_power_policy_t policy;
  uint64_t now = 0u;
  int cycle;

  CHECK(smart_band_power_policy_default_config(&config) ==
        SMART_BAND_POWER_POLICY_OK);
  config.dim_timeout_ms = 2000u;
  config.off_timeout_ms = 4000u;
  config.large_time_step_ms = 10000u;
  CHECK(smart_band_power_policy_init(&policy, &config, now) ==
        SMART_BAND_POWER_POLICY_OK);
  for (cycle = 0; cycle < 300; cycle++)
    {
      CHECK(smart_band_power_policy_handle(
              &policy, now + 2000u, SMART_BAND_POWER_EVENT_TICK) ==
            SMART_BAND_POWER_POLICY_OK);
      CHECK(policy.state == SMART_BAND_POWER_STATE_DIMMED);
      CHECK(smart_band_power_policy_handle(
              &policy, now + 4000u, SMART_BAND_POWER_EVENT_TICK) ==
            SMART_BAND_POWER_POLICY_OK);
      CHECK(policy.state == SMART_BAND_POWER_STATE_SCREEN_OFF);
      now += 6000u;
      CHECK(smart_band_power_policy_handle(
              &policy, now, SMART_BAND_POWER_EVENT_TOUCH) ==
            SMART_BAND_POWER_POLICY_OK);
      CHECK(policy.state == SMART_BAND_POWER_STATE_ACTIVE);
    }

  CHECK(now == 1800000u);
  CHECK(policy.transition_count == 900u);

  config.dim_timeout_ms = 1u;
  config.off_timeout_ms = 2u;
  config.large_time_step_ms = 10u;
  CHECK(smart_band_power_policy_init(&policy, &config, 0u) ==
        SMART_BAND_POWER_POLICY_OK);
  now = 0u;
  for (cycle = 0; cycle < 1000; cycle++)
    {
      CHECK(smart_band_power_policy_handle(
              &policy, now + 1u, SMART_BAND_POWER_EVENT_TICK) ==
            SMART_BAND_POWER_POLICY_OK);
      CHECK(policy.state == SMART_BAND_POWER_STATE_DIMMED);
      CHECK(smart_band_power_policy_handle(
              &policy, now + 2u, SMART_BAND_POWER_EVENT_TICK) ==
            SMART_BAND_POWER_POLICY_OK);
      CHECK(policy.state == SMART_BAND_POWER_STATE_SCREEN_OFF);
      now += 3u;
      CHECK(smart_band_power_policy_handle(
              &policy, now, SMART_BAND_POWER_EVENT_BUTTON) ==
            SMART_BAND_POWER_POLICY_OK);
      CHECK(policy.state == SMART_BAND_POWER_STATE_ACTIVE);
    }

  CHECK(policy.transition_count == 3000u);
  CHECK(policy.wake_reason == SMART_BAND_POWER_WAKE_BUTTON);
  return 0;
}

int main(void)
{
  CHECK(test_defaults_and_validation() == 0);
  CHECK(test_boundaries_and_transitions() == 0);
  CHECK(test_wake_reasons_and_priority() == 0);
  CHECK(test_workout_profiles() == 0);
  CHECK(test_time_and_invalid_inputs() == 0);
  CHECK(test_accelerated_cycles() == 0);
  puts("smart band power policy tests passed");
  return 0;
}
