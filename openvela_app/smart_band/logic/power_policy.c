#include "smart_band_power_policy.h"

#include <limits.h>
#include <string.h>

static bool smart_band_power_state_policy_valid(
  const smart_band_power_state_policy_t *policy,
  smart_band_power_state_t state)
{
  if (policy->brightness_percent > 100u ||
      policy->render_period_ms == 0u ||
      policy->heart_sampling_period_ms == 0u)
    {
      return false;
    }

  if (state == SMART_BAND_POWER_STATE_SCREEN_OFF)
    {
      return policy->brightness_percent == 0u;
    }

  return policy->brightness_percent > 0u;
}

static bool smart_band_power_profile_valid(
  const smart_band_power_profile_t *profile)
{
  smart_band_power_state_t state;

  for (state = SMART_BAND_POWER_STATE_ACTIVE;
       state < SMART_BAND_POWER_STATE_COUNT; state++)
    {
      if (!smart_band_power_state_policy_valid(&profile->states[state],
                                                state))
        {
          return false;
        }
    }

  return profile->states[SMART_BAND_POWER_STATE_DIMMED].brightness_percent <=
         profile->states[SMART_BAND_POWER_STATE_ACTIVE].brightness_percent;
}

static bool smart_band_power_screen_off_profiles_differ(
  const smart_band_power_policy_config_t *config)
{
  const smart_band_power_state_policy_t *idle =
    &config->idle.states[SMART_BAND_POWER_STATE_SCREEN_OFF];
  const smart_band_power_state_policy_t *workout =
    &config->workout.states[SMART_BAND_POWER_STATE_SCREEN_OFF];

  return idle->brightness_percent != workout->brightness_percent ||
         idle->render_period_ms != workout->render_period_ms ||
         idle->heart_sampling_period_ms !=
           workout->heart_sampling_period_ms ||
         idle->allow_motion_sampling != workout->allow_motion_sampling ||
         idle->allow_checkpoint != workout->allow_checkpoint ||
         idle->allow_sync != workout->allow_sync;
}

static smart_band_power_policy_result_t smart_band_power_policy_model_valid(
  const smart_band_power_policy_t *policy)
{
  smart_band_power_policy_result_t result;

  if (!policy->initialized)
    {
      return SMART_BAND_POWER_POLICY_NOT_INITIALIZED;
    }

  result = smart_band_power_policy_validate_config(&policy->config);
  if (result != SMART_BAND_POWER_POLICY_OK)
    {
      return result;
    }

  if (policy->state < SMART_BAND_POWER_STATE_ACTIVE ||
      policy->state >= SMART_BAND_POWER_STATE_COUNT ||
      policy->wake_reason < SMART_BAND_POWER_WAKE_NONE ||
      policy->wake_reason >= SMART_BAND_POWER_WAKE_COUNT ||
      policy->last_activity_ms > policy->last_monotonic_ms)
    {
      return SMART_BAND_POWER_POLICY_INVALID_STATE;
    }

  return SMART_BAND_POWER_POLICY_OK;
}

static smart_band_power_wake_reason_t smart_band_power_select_wake_reason(
  uint32_t events)
{
  if ((events & SMART_BAND_POWER_EVENT_BUTTON) != 0u)
    {
      return SMART_BAND_POWER_WAKE_BUTTON;
    }

  if ((events & SMART_BAND_POWER_EVENT_TOUCH) != 0u)
    {
      return SMART_BAND_POWER_WAKE_TOUCH;
    }

  if ((events & SMART_BAND_POWER_EVENT_NOTIFICATION) != 0u)
    {
      return SMART_BAND_POWER_WAKE_NOTIFICATION;
    }

  if ((events & SMART_BAND_POWER_EVENT_WRIST) != 0u)
    {
      return SMART_BAND_POWER_WAKE_WRIST;
    }

  if ((events & SMART_BAND_POWER_EVENT_CHARGING) != 0u)
    {
      return SMART_BAND_POWER_WAKE_CHARGING;
    }

  return SMART_BAND_POWER_WAKE_NONE;
}

static void smart_band_power_set_state(smart_band_power_policy_t *policy,
                                       smart_band_power_state_t state)
{
  if (policy->state != state)
    {
      policy->state = state;
      if (policy->transition_count < UINT64_MAX)
        {
          policy->transition_count++;
        }
    }
}

static void smart_band_power_set_default_profile(
  smart_band_power_profile_t *profile, bool workout)
{
  smart_band_power_state_policy_t *active =
    &profile->states[SMART_BAND_POWER_STATE_ACTIVE];
  smart_band_power_state_policy_t *dimmed =
    &profile->states[SMART_BAND_POWER_STATE_DIMMED];
  smart_band_power_state_policy_t *off =
    &profile->states[SMART_BAND_POWER_STATE_SCREEN_OFF];

  active->brightness_percent = 100u;
  active->render_period_ms = 16u;
  active->heart_sampling_period_ms = 1000u;
  active->allow_motion_sampling = true;
  active->allow_checkpoint = true;
  active->allow_sync = true;

  dimmed->brightness_percent = workout ? 35u : 25u;
  dimmed->render_period_ms = 100u;
  dimmed->heart_sampling_period_ms = workout ? 1000u : 2000u;
  dimmed->allow_motion_sampling = true;
  dimmed->allow_checkpoint = true;
  dimmed->allow_sync = true;

  off->brightness_percent = 0u;
  off->render_period_ms = workout ? 500u : 1000u;
  off->heart_sampling_period_ms = workout ? 1000u : 5000u;
  off->allow_motion_sampling = workout;
  off->allow_checkpoint = workout;
  off->allow_sync = false;
}

smart_band_power_policy_result_t smart_band_power_policy_default_config(
  smart_band_power_policy_config_t *config)
{
  if (config == NULL)
    {
      return SMART_BAND_POWER_POLICY_NULL_ARGUMENT;
    }

  memset(config, 0, sizeof(*config));
  config->dim_timeout_ms = 10000u;
  config->off_timeout_ms = 30000u;
  config->large_time_step_ms = 300000u;
  smart_band_power_set_default_profile(&config->idle, false);
  smart_band_power_set_default_profile(&config->workout, true);
  return SMART_BAND_POWER_POLICY_OK;
}

smart_band_power_policy_result_t smart_band_power_policy_validate_config(
  const smart_band_power_policy_config_t *config)
{
  if (config == NULL)
    {
      return SMART_BAND_POWER_POLICY_NULL_ARGUMENT;
    }

  if (config->dim_timeout_ms == 0u ||
      config->off_timeout_ms <= config->dim_timeout_ms ||
      config->large_time_step_ms < config->off_timeout_ms ||
      !smart_band_power_profile_valid(&config->idle) ||
      !smart_band_power_profile_valid(&config->workout) ||
      !smart_band_power_screen_off_profiles_differ(config))
    {
      return SMART_BAND_POWER_POLICY_INVALID_CONFIG;
    }

  return SMART_BAND_POWER_POLICY_OK;
}

smart_band_power_policy_result_t smart_band_power_policy_init(
  smart_band_power_policy_t *policy,
  const smart_band_power_policy_config_t *config,
  uint64_t monotonic_ms)
{
  smart_band_power_policy_result_t result;
  smart_band_power_policy_t initialized;

  if (policy == NULL || config == NULL)
    {
      return SMART_BAND_POWER_POLICY_NULL_ARGUMENT;
    }

  result = smart_band_power_policy_validate_config(config);
  if (result != SMART_BAND_POWER_POLICY_OK)
    {
      return result;
    }

  memset(&initialized, 0, sizeof(initialized));
  initialized.config = *config;
  initialized.state = SMART_BAND_POWER_STATE_ACTIVE;
  initialized.wake_reason = SMART_BAND_POWER_WAKE_NONE;
  initialized.last_activity_ms = monotonic_ms;
  initialized.last_monotonic_ms = monotonic_ms;
  initialized.initialized = true;
  *policy = initialized;
  return SMART_BAND_POWER_POLICY_OK;
}

smart_band_power_policy_result_t smart_band_power_policy_reset(
  smart_band_power_policy_t *policy, uint64_t monotonic_ms)
{
  smart_band_power_policy_result_t result;
  smart_band_power_policy_config_t config;

  if (policy == NULL)
    {
      return SMART_BAND_POWER_POLICY_NULL_ARGUMENT;
    }

  result = smart_band_power_policy_model_valid(policy);
  if (result != SMART_BAND_POWER_POLICY_OK)
    {
      return result;
    }

  config = policy->config;
  return smart_band_power_policy_init(policy, &config, monotonic_ms);
}

smart_band_power_policy_result_t smart_band_power_policy_handle(
  smart_band_power_policy_t *policy, uint64_t monotonic_ms,
  uint32_t events)
{
  smart_band_power_policy_result_t result;
  smart_band_power_policy_result_t time_result =
    SMART_BAND_POWER_POLICY_OK;
  smart_band_power_policy_t next;
  smart_band_power_wake_reason_t wake_reason;
  uint64_t elapsed;

  if (policy == NULL)
    {
      return SMART_BAND_POWER_POLICY_NULL_ARGUMENT;
    }

  result = smart_band_power_policy_model_valid(policy);
  if (result != SMART_BAND_POWER_POLICY_OK)
    {
      return result;
    }

  if (events == 0u || (events & ~SMART_BAND_POWER_EVENT_ALL) != 0u ||
      ((events & SMART_BAND_POWER_EVENT_WORKOUT_START) != 0u &&
       (events & SMART_BAND_POWER_EVENT_WORKOUT_STOP) != 0u))
    {
      return SMART_BAND_POWER_POLICY_INVALID_EVENT;
    }

  if (monotonic_ms < policy->last_monotonic_ms)
    {
      return SMART_BAND_POWER_POLICY_TIME_ROLLBACK;
    }

  if (monotonic_ms == policy->last_monotonic_ms)
    {
      time_result = SMART_BAND_POWER_POLICY_DUPLICATE_TIMESTAMP;
    }
  else if (monotonic_ms - policy->last_monotonic_ms >
           policy->config.large_time_step_ms)
    {
      time_result = SMART_BAND_POWER_POLICY_LARGE_TIME_JUMP;
    }

  next = *policy;
  next.last_monotonic_ms = monotonic_ms;

  if ((events & SMART_BAND_POWER_EVENT_WORKOUT_START) != 0u)
    {
      next.workout_active = true;
    }
  else if ((events & SMART_BAND_POWER_EVENT_WORKOUT_STOP) != 0u)
    {
      next.workout_active = false;
    }

  wake_reason = smart_band_power_select_wake_reason(events);
  if (wake_reason != SMART_BAND_POWER_WAKE_NONE)
    {
      smart_band_power_set_state(&next, SMART_BAND_POWER_STATE_ACTIVE);
      next.wake_reason = wake_reason;
      next.last_activity_ms = monotonic_ms;
    }
  else if ((events & SMART_BAND_POWER_EVENT_TICK) != 0u)
    {
      elapsed = monotonic_ms - next.last_activity_ms;
      if (elapsed >= next.config.off_timeout_ms)
        {
          smart_band_power_set_state(
            &next, SMART_BAND_POWER_STATE_SCREEN_OFF);
        }
      else if (elapsed >= next.config.dim_timeout_ms)
        {
          smart_band_power_set_state(&next,
                                     SMART_BAND_POWER_STATE_DIMMED);
        }
      else
        {
          smart_band_power_set_state(&next,
                                     SMART_BAND_POWER_STATE_ACTIVE);
        }
    }

  *policy = next;
  return time_result;
}

smart_band_power_policy_result_t smart_band_power_policy_snapshot(
  const smart_band_power_policy_t *policy,
  smart_band_power_policy_snapshot_t *snapshot)
{
  const smart_band_power_profile_t *profile;
  const smart_band_power_state_policy_t *state_policy;
  smart_band_power_policy_result_t result;

  if (policy == NULL || snapshot == NULL)
    {
      return SMART_BAND_POWER_POLICY_NULL_ARGUMENT;
    }

  result = smart_band_power_policy_model_valid(policy);
  if (result != SMART_BAND_POWER_POLICY_OK)
    {
      return result;
    }

  profile = policy->workout_active ? &policy->config.workout :
                                     &policy->config.idle;
  state_policy = &profile->states[policy->state];
  snapshot->state = policy->state;
  snapshot->wake_reason = policy->wake_reason;
  snapshot->last_activity_ms = policy->last_activity_ms;
  snapshot->monotonic_ms = policy->last_monotonic_ms;
  snapshot->transition_count = policy->transition_count;
  snapshot->brightness_percent = state_policy->brightness_percent;
  snapshot->render_period_ms = state_policy->render_period_ms;
  snapshot->heart_sampling_period_ms =
    state_policy->heart_sampling_period_ms;
  snapshot->display_enabled =
    policy->state != SMART_BAND_POWER_STATE_SCREEN_OFF;
  snapshot->workout_active = policy->workout_active;
  snapshot->allow_motion_sampling = state_policy->allow_motion_sampling;
  snapshot->allow_checkpoint = state_policy->allow_checkpoint;
  snapshot->allow_sync = state_policy->allow_sync;
  return SMART_BAND_POWER_POLICY_OK;
}
