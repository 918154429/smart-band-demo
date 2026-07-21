#ifndef SMART_BAND_POWER_POLICY_H
#define SMART_BAND_POWER_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  SMART_BAND_POWER_STATE_ACTIVE = 0,
  SMART_BAND_POWER_STATE_DIMMED,
  SMART_BAND_POWER_STATE_SCREEN_OFF,
  SMART_BAND_POWER_STATE_COUNT
} smart_band_power_state_t;

typedef enum
{
  SMART_BAND_POWER_WAKE_NONE = 0,
  SMART_BAND_POWER_WAKE_BUTTON,
  SMART_BAND_POWER_WAKE_TOUCH,
  SMART_BAND_POWER_WAKE_NOTIFICATION,
  SMART_BAND_POWER_WAKE_WRIST,
  SMART_BAND_POWER_WAKE_CHARGING,
  SMART_BAND_POWER_WAKE_COUNT
} smart_band_power_wake_reason_t;

typedef enum
{
  SMART_BAND_POWER_EVENT_TICK = 1u << 0,
  SMART_BAND_POWER_EVENT_TOUCH = 1u << 1,
  SMART_BAND_POWER_EVENT_BUTTON = 1u << 2,
  SMART_BAND_POWER_EVENT_WRIST = 1u << 3,
  SMART_BAND_POWER_EVENT_NOTIFICATION = 1u << 4,
  SMART_BAND_POWER_EVENT_CHARGING = 1u << 5,
  SMART_BAND_POWER_EVENT_WORKOUT_START = 1u << 6,
  SMART_BAND_POWER_EVENT_WORKOUT_STOP = 1u << 7
} smart_band_power_event_t;

#define SMART_BAND_POWER_EVENT_ALL ((uint32_t)0xffu)

typedef enum
{
  SMART_BAND_POWER_POLICY_OK = 0,
  SMART_BAND_POWER_POLICY_DUPLICATE_TIMESTAMP,
  SMART_BAND_POWER_POLICY_LARGE_TIME_JUMP,
  SMART_BAND_POWER_POLICY_NULL_ARGUMENT,
  SMART_BAND_POWER_POLICY_INVALID_CONFIG,
  SMART_BAND_POWER_POLICY_NOT_INITIALIZED,
  SMART_BAND_POWER_POLICY_INVALID_EVENT,
  SMART_BAND_POWER_POLICY_TIME_ROLLBACK,
  SMART_BAND_POWER_POLICY_INVALID_STATE
} smart_band_power_policy_result_t;

typedef struct
{
  uint8_t brightness_percent;
  uint32_t render_period_ms;
  uint32_t heart_sampling_period_ms;
  bool allow_motion_sampling;
  bool allow_checkpoint;
  bool allow_sync;
} smart_band_power_state_policy_t;

typedef struct
{
  smart_band_power_state_policy_t states[SMART_BAND_POWER_STATE_COUNT];
} smart_band_power_profile_t;

typedef struct
{
  uint64_t dim_timeout_ms;
  uint64_t off_timeout_ms;
  uint64_t large_time_step_ms;
  smart_band_power_profile_t idle;
  smart_band_power_profile_t workout;
} smart_band_power_policy_config_t;

typedef struct
{
  smart_band_power_policy_config_t config;
  smart_band_power_state_t state;
  smart_band_power_wake_reason_t wake_reason;
  uint64_t last_activity_ms;
  uint64_t last_monotonic_ms;
  uint64_t transition_count;
  bool workout_active;
  bool initialized;
} smart_band_power_policy_t;

typedef struct
{
  smart_band_power_state_t state;
  smart_band_power_wake_reason_t wake_reason;
  uint64_t last_activity_ms;
  uint64_t monotonic_ms;
  uint64_t transition_count;
  uint8_t brightness_percent;
  uint32_t render_period_ms;
  uint32_t heart_sampling_period_ms;
  bool display_enabled;
  bool workout_active;
  bool allow_motion_sampling;
  bool allow_checkpoint;
  bool allow_sync;
} smart_band_power_policy_snapshot_t;

smart_band_power_policy_result_t smart_band_power_policy_default_config(
  smart_band_power_policy_config_t *config);
smart_band_power_policy_result_t smart_band_power_policy_validate_config(
  const smart_band_power_policy_config_t *config);
smart_band_power_policy_result_t smart_band_power_policy_init(
  smart_band_power_policy_t *policy,
  const smart_band_power_policy_config_t *config,
  uint64_t monotonic_ms);
smart_band_power_policy_result_t smart_band_power_policy_reset(
  smart_band_power_policy_t *policy, uint64_t monotonic_ms);
smart_band_power_policy_result_t smart_band_power_policy_handle(
  smart_band_power_policy_t *policy, uint64_t monotonic_ms,
  uint32_t events);
smart_band_power_policy_result_t smart_band_power_policy_snapshot(
  const smart_band_power_policy_t *policy,
  smart_band_power_policy_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif

#endif
