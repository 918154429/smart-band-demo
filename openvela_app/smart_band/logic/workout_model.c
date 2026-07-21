#include "smart_band_workout_model.h"

#include <limits.h>
#include <string.h>

static bool workout_add_u64(uint64_t left, uint64_t right, uint64_t *result)
{
  if (right > UINT64_MAX - left)
    {
      return false;
    }

  *result = left + right;
  return true;
}

static bool workout_mul_u64(uint64_t left, uint64_t right, uint64_t *result)
{
  if (left != 0u && right > UINT64_MAX / left)
    {
      return false;
    }

  *result = left * right;
  return true;
}

static bool workout_mode_valid(smart_band_workout_mode_t mode)
{
  return mode >= SMART_BAND_WORKOUT_MODE_WALK &&
         mode < SMART_BAND_WORKOUT_MODE_COUNT;
}

static bool workout_config_valid(const smart_band_workout_config_t *config)
{
  int index;

  if (config == NULL || config->countdown_ms == 0u)
    {
      return false;
    }

  for (index = 0; index < SMART_BAND_WORKOUT_MODE_COUNT; index++)
    {
      const smart_band_workout_mode_config_t *mode = &config->modes[index];

      if (mode->stride_mm == 0u ||
          mode->calories_milli_kcal_per_step == 0u ||
          mode->max_step_delta == 0u ||
          mode->minimum_heart_rate_bpm == 0u ||
          mode->minimum_heart_rate_bpm > mode->maximum_heart_rate_bpm)
        {
          return false;
        }
    }

  return true;
}

static smart_band_workout_result_t workout_add_active_time(
  smart_band_workout_model_t *model, uint64_t duration_ms)
{
  smart_band_workout_snapshot_t *data = &model->data;
  uint64_t weighted;

  if (!workout_add_u64(data->active_duration_ms, duration_ms,
                       &data->active_duration_ms))
    {
      return SMART_BAND_WORKOUT_RESULT_OVERFLOW;
    }

  if (data->heart_rate_current_valid && duration_ms != 0u)
    {
      if (!workout_mul_u64(data->heart_rate_current_bpm, duration_ms,
                           &weighted) ||
          !workout_add_u64(data->heart_rate_weighted_bpm_ms, weighted,
                           &data->heart_rate_weighted_bpm_ms) ||
          !workout_add_u64(data->heart_rate_weighted_duration_ms, duration_ms,
                           &data->heart_rate_weighted_duration_ms))
        {
          return SMART_BAND_WORKOUT_RESULT_OVERFLOW;
        }
    }

  return SMART_BAND_WORKOUT_RESULT_OK;
}

static smart_band_workout_result_t workout_advance(
  smart_band_workout_model_t *model, uint64_t monotonic_ms)
{
  uint64_t elapsed;
  uint64_t remaining;
  uint64_t active_elapsed = 0u;

  if (monotonic_ms < model->last_monotonic_ms)
    {
      return SMART_BAND_WORKOUT_RESULT_TIME_REGRESSION;
    }

  elapsed = monotonic_ms - model->last_monotonic_ms;
  if (model->data.state == SMART_BAND_WORKOUT_STATE_COUNTDOWN)
    {
      remaining = model->config.countdown_ms -
                  model->data.countdown_elapsed_ms;
      if (elapsed >= remaining)
        {
          model->data.countdown_elapsed_ms = model->config.countdown_ms;
          model->data.state = SMART_BAND_WORKOUT_STATE_ACTIVE;
          active_elapsed = elapsed - remaining;
        }
      else
        {
          model->data.countdown_elapsed_ms += elapsed;
        }
    }
  else if (model->data.state == SMART_BAND_WORKOUT_STATE_ACTIVE)
    {
      active_elapsed = elapsed;
    }

  if (workout_add_active_time(model, active_elapsed) !=
      SMART_BAND_WORKOUT_RESULT_OK)
    {
      return SMART_BAND_WORKOUT_RESULT_OVERFLOW;
    }

  model->last_monotonic_ms = monotonic_ms;
  return SMART_BAND_WORKOUT_RESULT_OK;
}

static bool workout_snapshot_valid(
  const smart_band_workout_snapshot_t *snapshot)
{
  if (snapshot == NULL || !workout_mode_valid(snapshot->mode) ||
      (snapshot->state != SMART_BAND_WORKOUT_STATE_COUNTDOWN &&
       snapshot->state != SMART_BAND_WORKOUT_STATE_ACTIVE &&
       snapshot->state != SMART_BAND_WORKOUT_STATE_PAUSED))
    {
      return false;
    }

  if (!snapshot->heart_rate_aggregate_valid)
    {
      return snapshot->heart_rate_weighted_bpm_ms == 0u &&
             snapshot->heart_rate_weighted_duration_ms == 0u;
    }

  return snapshot->heart_rate_min_bpm != 0u &&
         snapshot->heart_rate_min_bpm <= snapshot->heart_rate_max_bpm &&
         (snapshot->heart_rate_weighted_duration_ms != 0u ||
          snapshot->heart_rate_weighted_bpm_ms == 0u);
}

smart_band_workout_result_t smart_band_workout_model_init(
  smart_band_workout_model_t *model,
  const smart_band_workout_config_t *config,
  smart_band_workout_mode_t mode,
  uint64_t monotonic_ms)
{
  if (model == NULL)
    {
      return SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT;
    }

  if (!workout_config_valid(config) || !workout_mode_valid(mode))
    {
      return SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG;
    }

  (void)memset(model, 0, sizeof(*model));
  model->config = *config;
  model->data.mode = mode;
  model->data.state = SMART_BAND_WORKOUT_STATE_IDLE;
  model->last_monotonic_ms = monotonic_ms;
  model->initialized = true;
  return SMART_BAND_WORKOUT_RESULT_OK;
}

smart_band_workout_result_t smart_band_workout_model_command(
  smart_band_workout_model_t *model,
  smart_band_workout_command_t command,
  uint64_t monotonic_ms)
{
  smart_band_workout_model_t next;
  smart_band_workout_result_t result;

  if (model == NULL || !model->initialized)
    {
      return SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT;
    }

  if (command < SMART_BAND_WORKOUT_COMMAND_START ||
      command > SMART_BAND_WORKOUT_COMMAND_CONFIRM_RECOVERY)
    {
      return SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT;
    }

  next = *model;
  result = workout_advance(&next, monotonic_ms);
  if (result != SMART_BAND_WORKOUT_RESULT_OK)
    {
      return result;
    }

  switch (command)
    {
      case SMART_BAND_WORKOUT_COMMAND_START:
        if (next.data.state != SMART_BAND_WORKOUT_STATE_IDLE)
          {
            return SMART_BAND_WORKOUT_RESULT_INVALID_STATE;
          }

        next.data.state = SMART_BAND_WORKOUT_STATE_COUNTDOWN;
        next.data.countdown_elapsed_ms = 0u;
        break;

      case SMART_BAND_WORKOUT_COMMAND_PAUSE:
        if (next.data.state != SMART_BAND_WORKOUT_STATE_ACTIVE)
          {
            return SMART_BAND_WORKOUT_RESULT_INVALID_STATE;
          }

        next.data.state = SMART_BAND_WORKOUT_STATE_PAUSED;
        break;

      case SMART_BAND_WORKOUT_COMMAND_RESUME:
        if (next.data.state != SMART_BAND_WORKOUT_STATE_PAUSED)
          {
            return SMART_BAND_WORKOUT_RESULT_INVALID_STATE;
          }

        next.data.state = SMART_BAND_WORKOUT_STATE_ACTIVE;
        break;

      case SMART_BAND_WORKOUT_COMMAND_FINISH:
        if (next.data.state != SMART_BAND_WORKOUT_STATE_ACTIVE &&
            next.data.state != SMART_BAND_WORKOUT_STATE_PAUSED)
          {
            return SMART_BAND_WORKOUT_RESULT_INVALID_STATE;
          }

        next.data.state = SMART_BAND_WORKOUT_STATE_FINISHED;
        next.data.heart_rate_current_valid = false;
        break;

      case SMART_BAND_WORKOUT_COMMAND_ABORT:
        if (next.data.state != SMART_BAND_WORKOUT_STATE_COUNTDOWN &&
            next.data.state != SMART_BAND_WORKOUT_STATE_ACTIVE &&
            next.data.state != SMART_BAND_WORKOUT_STATE_PAUSED &&
            next.data.state != SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION)
          {
            return SMART_BAND_WORKOUT_RESULT_INVALID_STATE;
          }

        next.data.state = SMART_BAND_WORKOUT_STATE_ABORTED;
        next.data.heart_rate_current_valid = false;
        break;

      case SMART_BAND_WORKOUT_COMMAND_CONFIRM_RECOVERY:
        if (next.data.state !=
            SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION)
          {
            return SMART_BAND_WORKOUT_RESULT_INVALID_STATE;
          }

        next.data.state = SMART_BAND_WORKOUT_STATE_PAUSED;
        next.data.heart_rate_current_valid = false;
        break;
    }

  *model = next;
  return SMART_BAND_WORKOUT_RESULT_OK;
}

smart_band_workout_result_t smart_band_workout_model_update(
  smart_band_workout_model_t *model,
  const smart_band_workout_sample_t *sample)
{
  smart_band_workout_model_t next;
  const smart_band_workout_mode_config_t *mode;
  smart_band_workout_result_t result;
  uint64_t increment;

  if (model == NULL || sample == NULL || !model->initialized)
    {
      return SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT;
    }

  if (model->data.state == SMART_BAND_WORKOUT_STATE_FINISHED ||
      model->data.state == SMART_BAND_WORKOUT_STATE_ABORTED ||
      model->data.state == SMART_BAND_WORKOUT_STATE_IDLE ||
      model->data.state == SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION)
    {
      return SMART_BAND_WORKOUT_RESULT_INVALID_STATE;
    }

  mode = &model->config.modes[model->data.mode];
  if (sample->step_delta > mode->max_step_delta ||
      (sample->heart_rate_valid &&
       (sample->heart_rate_bpm < mode->minimum_heart_rate_bpm ||
        sample->heart_rate_bpm > mode->maximum_heart_rate_bpm)))
    {
      return SMART_BAND_WORKOUT_RESULT_INVALID_SAMPLE;
    }

  next = *model;
  result = workout_advance(&next, sample->monotonic_ms);
  if (result != SMART_BAND_WORKOUT_RESULT_OK)
    {
      return result;
    }

  if (next.data.state == SMART_BAND_WORKOUT_STATE_ACTIVE)
    {
      if (!workout_add_u64(next.data.steps, sample->step_delta,
                           &next.data.steps) ||
          !workout_mul_u64(sample->step_delta, mode->stride_mm, &increment) ||
          !workout_add_u64(next.data.distance_mm, increment,
                           &next.data.distance_mm) ||
          !workout_mul_u64(sample->step_delta,
                           mode->calories_milli_kcal_per_step, &increment) ||
          !workout_add_u64(next.data.calories_milli_kcal, increment,
                           &next.data.calories_milli_kcal))
        {
          return SMART_BAND_WORKOUT_RESULT_OVERFLOW;
        }

      next.data.heart_rate_current_valid = sample->heart_rate_valid;
      if (sample->heart_rate_valid)
        {
          next.data.heart_rate_current_bpm = sample->heart_rate_bpm;
          if (!next.data.heart_rate_aggregate_valid)
            {
              next.data.heart_rate_min_bpm = sample->heart_rate_bpm;
              next.data.heart_rate_max_bpm = sample->heart_rate_bpm;
              next.data.heart_rate_aggregate_valid = true;
            }
          else
            {
              if (sample->heart_rate_bpm < next.data.heart_rate_min_bpm)
                {
                  next.data.heart_rate_min_bpm = sample->heart_rate_bpm;
                }

              if (sample->heart_rate_bpm > next.data.heart_rate_max_bpm)
                {
                  next.data.heart_rate_max_bpm = sample->heart_rate_bpm;
                }
            }
        }
    }

  *model = next;
  return SMART_BAND_WORKOUT_RESULT_OK;
}

smart_band_workout_result_t smart_band_workout_model_snapshot(
  const smart_band_workout_model_t *model,
  smart_band_workout_snapshot_t *snapshot)
{
  if (model == NULL || snapshot == NULL || !model->initialized)
    {
      return SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT;
    }

  *snapshot = model->data;
  return SMART_BAND_WORKOUT_RESULT_OK;
}

smart_band_workout_result_t smart_band_workout_model_restore(
  smart_band_workout_model_t *model,
  const smart_band_workout_config_t *config,
  const smart_band_workout_snapshot_t *snapshot,
  uint64_t monotonic_ms)
{
  const smart_band_workout_mode_config_t *mode;

  if (model == NULL)
    {
      return SMART_BAND_WORKOUT_RESULT_INVALID_ARGUMENT;
    }

  if (!workout_config_valid(config) || !workout_snapshot_valid(snapshot) ||
      snapshot->countdown_elapsed_ms > config->countdown_ms)
    {
      return SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG;
    }

  mode = &config->modes[snapshot->mode];
  if ((snapshot->heart_rate_current_valid &&
       (snapshot->heart_rate_current_bpm < mode->minimum_heart_rate_bpm ||
        snapshot->heart_rate_current_bpm > mode->maximum_heart_rate_bpm)) ||
      (snapshot->heart_rate_aggregate_valid &&
       (snapshot->heart_rate_min_bpm < mode->minimum_heart_rate_bpm ||
        snapshot->heart_rate_max_bpm > mode->maximum_heart_rate_bpm)))
    {
      return SMART_BAND_WORKOUT_RESULT_INVALID_CONFIG;
    }

  (void)memset(model, 0, sizeof(*model));
  model->config = *config;
  model->data = *snapshot;
  model->data.state = SMART_BAND_WORKOUT_STATE_RECOVERY_CONFIRMATION;
  model->data.heart_rate_current_valid = false;
  model->last_monotonic_ms = monotonic_ms;
  model->initialized = true;
  return SMART_BAND_WORKOUT_RESULT_OK;
}

uint64_t smart_band_workout_average_heart_rate_bpm(
  const smart_band_workout_snapshot_t *snapshot)
{
  if (snapshot == NULL || snapshot->heart_rate_weighted_duration_ms == 0u)
    {
      return 0u;
    }

  return snapshot->heart_rate_weighted_bpm_ms /
         snapshot->heart_rate_weighted_duration_ms;
}

uint64_t smart_band_workout_pace_ms_per_km(
  const smart_band_workout_snapshot_t *snapshot)
{
  uint64_t scaled;

  if (snapshot == NULL || snapshot->distance_mm == 0u ||
      !workout_mul_u64(snapshot->active_duration_ms, 1000000u, &scaled))
    {
      return 0u;
    }

  return scaled / snapshot->distance_mm;
}
