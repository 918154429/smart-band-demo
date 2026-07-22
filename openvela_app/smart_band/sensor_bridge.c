#include "sensor_bridge.h"

#include <string.h>
#include <time.h>

#if defined(CONFIG_LVX_DEMO_SMART_BAND_USE_SENSORS)

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nuttx/power/battery_ioctl.h>
#include <nuttx/sensors/ioctl.h>
#include <nuttx/uorb.h>

#define SMART_BAND_HRATE_DEV "/dev/uorb/sensor_hrate0"
#define SMART_BAND_ACCEL_DEV "/dev/uorb/sensor_accel0"
#define SMART_BAND_STEP_DEV "/dev/uorb/sensor_step_counter0"
#define SMART_BAND_TEMP_DEV "/dev/uorb/sensor_ambient_temp0"
#define SMART_BAND_TEMP_FALLBACK_DEV "/dev/uorb/sensor_temp0"
#define SMART_BAND_HUMI_DEV "/dev/uorb/sensor_humi0"
#define SMART_BAND_BATTERY_DEV "/dev/charge/goldfish_battery"
#define SMART_BAND_SENSOR_INTERVAL_US 1000000
#define SMART_BAND_SENSOR_DRAIN_LIMIT 32
#define SMART_BAND_HRATE_MIN_BPM 40
#define SMART_BAND_HRATE_MAX_BPM 500

static int clamp_int(int value, int min_value, int max_value)
{
  if (value < min_value)
    {
      return min_value;
    }

  if (value > max_value)
    {
      return max_value;
    }

  return value;
}

static float abs_float(float value)
{
  return value < 0.0f ? -value : value;
}

static int open_sensor(const char *path)
{
  int fd = open(path, O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    {
      return -1;
    }

  if (ioctl(fd, SNIOC_SET_INTERVAL, SMART_BAND_SENSOR_INTERVAL_US) < 0 &&
      errno != ENOTSUP)
    {
      printf("smart_band: failed to set interval for %s: %d\n", path, errno);
    }

  return fd;
}

static bool read_latest(int fd, void *sample, size_t sample_size)
{
  bool updated = false;
  unsigned int reads = 0;

  if (fd < 0)
    {
      return false;
    }

  while (reads < SMART_BAND_SENSOR_DRAIN_LIMIT &&
         read(fd, sample, sample_size) == (ssize_t)sample_size)
    {
      updated = true;
      reads++;
    }

  return updated;
}

static void update_heart_rate(smart_band_sensor_bridge_t *bridge,
                              smart_band_state_t *state, time_t wall_now,
                              uint64_t monotonic_ms)
{
  struct sensor_hrate sample;

  if (read_latest(bridge->hrate_fd, &sample, sizeof(sample)) &&
      sample.bpm > 0.0f)
    {
      smart_band_state_publish_metric_at(
        state, SMART_BAND_METRIC_HEART_RATE,
        clamp_int((int)(sample.bpm + 0.5f), SMART_BAND_HRATE_MIN_BPM,
                  SMART_BAND_HRATE_MAX_BPM),
        SMART_BAND_DATA_SOURCE_SENSOR, wall_now, monotonic_ms);
    }
}

static bool update_steps_from_counter(smart_band_sensor_bridge_t *bridge,
                                      smart_band_state_t *state,
                                      time_t wall_now,
                                      uint64_t monotonic_ms)
{
  struct sensor_step_counter sample;
  int steps;

  if (!read_latest(bridge->step_fd, &sample, sizeof(sample)))
    {
      return false;
    }

  steps = clamp_int((int)(sample.steps % 100000u), 0, 99999);
  bridge->raw_step_counter = sample.steps;
  bridge->raw_step_source = SMART_BAND_STEP_SOURCE_SENSOR;
  bridge->have_raw_step = true;
  bridge->derived_steps = steps;
  smart_band_state_publish_metric_at(
    state, SMART_BAND_METRIC_STEPS, steps, SMART_BAND_DATA_SOURCE_SENSOR,
    wall_now, monotonic_ms);
  return true;
}

static void update_steps_from_accel(smart_band_sensor_bridge_t *bridge,
                                    smart_band_state_t *state,
                                    time_t wall_now,
                                    uint64_t monotonic_ms)
{
  const smart_band_metric_info_t *step_info;
  struct sensor_accel sample;
  float energy;
  float delta;

  if (!read_latest(bridge->accel_fd, &sample, sizeof(sample)))
    {
      return;
    }

  energy = abs_float(sample.x) + abs_float(sample.y) + abs_float(sample.z);
  if (bridge->have_last_accel)
    {
      delta = abs_float(energy - bridge->last_accel_energy);
      if (delta > 1.2f)
        {
          bridge->derived_steps += delta > 4.0f ? 2 : 1;
          bridge->derived_steps = clamp_int(bridge->derived_steps, 0, 99999);
        }
    }

  bridge->last_accel_energy = energy;
  bridge->have_last_accel = true;
  bridge->raw_step_counter = (uint64_t)bridge->derived_steps;
  bridge->raw_step_source = SMART_BAND_STEP_SOURCE_DERIVED;
  bridge->have_raw_step = true;

  step_info = smart_band_state_metric_info(state, SMART_BAND_METRIC_STEPS);
  if (step_info != NULL &&
      step_info->source == SMART_BAND_DATA_SOURCE_SENSOR)
    {
      /* Keep a recent hardware counter sample during its stale TTL. */

      return;
    }

  smart_band_state_publish_metric_at(
    state, SMART_BAND_METRIC_STEPS, bridge->derived_steps,
    SMART_BAND_DATA_SOURCE_SENSOR_DERIVED, wall_now, monotonic_ms);
}

static void update_battery(smart_band_sensor_bridge_t *bridge,
                           smart_band_state_t *state, time_t wall_now,
                           uint64_t monotonic_ms)
{
  const smart_band_metric_info_t *info;
  int capacity = 0;
  int battery_state = BATTERY_UNKNOWN;

  if (bridge->battery_fd < 0)
    {
      return;
    }

  if (ioctl(bridge->battery_fd, BATIOC_CAPACITY,
            (unsigned long)(uintptr_t)&capacity) == 0)
    {
      smart_band_state_publish_metric_at(
        state, SMART_BAND_METRIC_BATTERY, clamp_int(capacity, 0, 100),
        SMART_BAND_DATA_SOURCE_SENSOR, wall_now, monotonic_ms);
    }

  info = smart_band_state_metric_info(state, SMART_BAND_METRIC_BATTERY);
  if (info != NULL && info->source == SMART_BAND_DATA_SOURCE_SENSOR &&
      ioctl(bridge->battery_fd, BATIOC_STATE,
            (unsigned long)(uintptr_t)&battery_state) == 0)
    {
      state->battery_charging = battery_state == BATTERY_CHARGING;
    }
}

static void update_temperature(smart_band_sensor_bridge_t *bridge,
                               smart_band_state_t *state, time_t wall_now,
                               uint64_t monotonic_ms)
{
  struct sensor_temp sample;
  float rounded;
  int value_c;

  if (!read_latest(bridge->temp_fd, &sample, sizeof(sample)))
    {
      return;
    }

  rounded = sample.temperature >= 0.0f ? sample.temperature + 0.5f :
            sample.temperature - 0.5f;
  value_c = (int)rounded;
  if (!bridge->have_temperature || bridge->last_temperature_c != value_c)
    {
      printf("smart_band: temperature sensor %dC\n", value_c);
    }

  bridge->last_temperature_c = value_c;
  bridge->have_temperature = true;
  smart_band_state_publish_metric_at(
    state, SMART_BAND_METRIC_TEMPERATURE, value_c,
    SMART_BAND_DATA_SOURCE_SENSOR, wall_now, monotonic_ms);
}

static void update_humidity(smart_band_sensor_bridge_t *bridge,
                            smart_band_state_t *state, time_t wall_now,
                            uint64_t monotonic_ms)
{
  struct sensor_humi sample;
  int value;

  if (!read_latest(bridge->humi_fd, &sample, sizeof(sample)))
    {
      return;
    }

  value = clamp_int((int)(sample.humidity + 0.5f), 0, 100);
  if (!bridge->have_humidity || bridge->last_humidity_percent != value)
    {
      printf("smart_band: humidity sensor %d%%\n", value);
    }

  bridge->last_humidity_percent = value;
  bridge->have_humidity = true;
  smart_band_state_publish_metric_at(
    state, SMART_BAND_METRIC_HUMIDITY, value, SMART_BAND_DATA_SOURCE_SENSOR,
    wall_now, monotonic_ms);
}

#endif /* CONFIG_LVX_DEMO_SMART_BAND_USE_SENSORS */

void smart_band_sensor_bridge_init(smart_band_sensor_bridge_t *bridge)
{
  if (bridge == NULL)
    {
      return;
    }

  memset(bridge, 0, sizeof(*bridge));
  bridge->hrate_fd = -1;
  bridge->accel_fd = -1;
  bridge->step_fd = -1;
  bridge->battery_fd = -1;
  bridge->temp_fd = -1;
  bridge->humi_fd = -1;
  bridge->last_temperature_c = 24;
  bridge->last_humidity_percent = 60;

#if defined(CONFIG_LVX_DEMO_SMART_BAND_USE_SENSORS)
  bridge->hrate_fd = open_sensor(SMART_BAND_HRATE_DEV);
  bridge->accel_fd = open_sensor(SMART_BAND_ACCEL_DEV);
  bridge->step_fd = open_sensor(SMART_BAND_STEP_DEV);
  bridge->battery_fd = open(SMART_BAND_BATTERY_DEV, O_RDONLY | O_NONBLOCK);
  bridge->temp_fd = open_sensor(SMART_BAND_TEMP_DEV);
  if (bridge->temp_fd < 0)
    {
      bridge->temp_fd = open_sensor(SMART_BAND_TEMP_FALLBACK_DEV);
    }

  bridge->humi_fd = open_sensor(SMART_BAND_HUMI_DEV);
#endif
}

void smart_band_sensor_bridge_update(smart_band_sensor_bridge_t *bridge,
                                     smart_band_state_t *state)
{
  smart_band_sensor_bridge_update_at(bridge, state, time(NULL));
}

void smart_band_sensor_bridge_update_at(smart_band_sensor_bridge_t *bridge,
                                        smart_band_state_t *state,
                                        time_t now)
{
  smart_band_sensor_bridge_update_clocked(
    bridge, state, now, SMART_BAND_MONOTONIC_INVALID, false);
}

void smart_band_sensor_bridge_update_clocked(
  smart_band_sensor_bridge_t *bridge, smart_band_state_t *state,
  time_t wall_now, uint64_t monotonic_ms, bool wall_rollback)
{
  if (bridge == NULL || state == NULL)
    {
      return;
    }

  smart_band_state_begin_sensor_cycle_at(state, wall_now, monotonic_ms,
                                         wall_rollback);

#if defined(CONFIG_LVX_DEMO_SMART_BAND_USE_SENSORS)
  if (state->data_mode == SMART_BAND_DATA_MODE_SIMULATION)
    {
      return;
    }

  update_heart_rate(bridge, state, wall_now, monotonic_ms);
  if (!update_steps_from_counter(bridge, state, wall_now, monotonic_ms))
    {
      update_steps_from_accel(bridge, state, wall_now, monotonic_ms);
    }

  update_battery(bridge, state, wall_now, monotonic_ms);
  update_temperature(bridge, state, wall_now, monotonic_ms);
  update_humidity(bridge, state, wall_now, monotonic_ms);
#endif
}

void smart_band_sensor_bridge_deinit(smart_band_sensor_bridge_t *bridge)
{
  if (bridge == NULL)
    {
      return;
    }

#if defined(CONFIG_LVX_DEMO_SMART_BAND_USE_SENSORS)
  if (bridge->hrate_fd >= 0)
    {
      close(bridge->hrate_fd);
    }

  if (bridge->accel_fd >= 0)
    {
      close(bridge->accel_fd);
    }

  if (bridge->step_fd >= 0)
    {
      close(bridge->step_fd);
    }

  if (bridge->battery_fd >= 0)
    {
      close(bridge->battery_fd);
    }

  if (bridge->temp_fd >= 0)
    {
      close(bridge->temp_fd);
    }

  if (bridge->humi_fd >= 0)
    {
      close(bridge->humi_fd);
    }
#endif

  bridge->hrate_fd = -1;
  bridge->accel_fd = -1;
  bridge->step_fd = -1;
  bridge->battery_fd = -1;
  bridge->temp_fd = -1;
  bridge->humi_fd = -1;
  bridge->have_last_accel = false;
  bridge->have_temperature = false;
  bridge->have_humidity = false;
  bridge->have_raw_step = false;
}

bool smart_band_sensor_bridge_step_sample(
  const smart_band_sensor_bridge_t *bridge, uint64_t monotonic_ms,
  bool fresh, smart_band_step_sample_t *sample)
{
  if (bridge == NULL || sample == NULL)
    {
      return false;
    }

  memset(sample, 0, sizeof(*sample));
  sample->monotonic_ms = monotonic_ms;
  if (!bridge->have_raw_step)
    {
      return true;
    }

  sample->source = bridge->raw_step_source;
  sample->raw_counter = bridge->raw_step_counter;
  sample->available = true;
  sample->fresh = fresh;
  return true;
}
