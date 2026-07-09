#include "sensor_bridge.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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
#define SMART_BAND_BATTERY_DEV "/dev/charge/goldfish_battery"
#define SMART_BAND_SENSOR_INTERVAL_US 1000000
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

  if (fd < 0)
    {
      return false;
    }

  while (read(fd, sample, sample_size) == (ssize_t)sample_size)
    {
      updated = true;
    }

  return updated;
}

static void update_heart_rate(smart_band_sensor_bridge_t *bridge,
                              smart_band_state_t *state)
{
  struct sensor_hrate sample;

  if (read_latest(bridge->hrate_fd, &sample, sizeof(sample)) &&
      sample.bpm > 0.0f)
    {
      state->heart_rate = clamp_int((int)(sample.bpm + 0.5f),
                                    SMART_BAND_HRATE_MIN_BPM,
                                    SMART_BAND_HRATE_MAX_BPM);
      state->heart_sensor_active = true;
    }
}

static void update_steps_from_counter(smart_band_sensor_bridge_t *bridge,
                                      smart_band_state_t *state)
{
  struct sensor_step_counter sample;

  if (read_latest(bridge->step_fd, &sample, sizeof(sample)))
    {
      state->steps = clamp_int((int)(sample.steps % 100000u), 0, 99999);
      state->step_sensor_active = true;
    }
}

static void update_steps_from_accel(smart_band_sensor_bridge_t *bridge,
                                    smart_band_state_t *state)
{
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
          state->steps = clamp_int(state->steps + bridge->derived_steps,
                                   0, 99999);
          bridge->derived_steps = 0;
        }
    }

  bridge->last_accel_energy = energy;
  bridge->have_last_accel = true;
  state->step_sensor_active = true;
}

static void update_battery(smart_band_sensor_bridge_t *bridge,
                           smart_band_state_t *state)
{
  int capacity = 0;
  int battery_state = BATTERY_UNKNOWN;

  if (bridge->battery_fd < 0)
    {
      return;
    }

  if (ioctl(bridge->battery_fd, BATIOC_CAPACITY,
            (unsigned long)(uintptr_t)&capacity) == 0)
    {
      state->battery_percent = clamp_int(capacity, 0, 100);
      state->battery_sensor_active = true;
    }

  if (ioctl(bridge->battery_fd, BATIOC_STATE,
            (unsigned long)(uintptr_t)&battery_state) == 0)
    {
      state->battery_charging = battery_state == BATTERY_CHARGING;
      state->battery_sensor_active = true;
    }
}

static void update_temperature(smart_band_sensor_bridge_t *bridge,
                               smart_band_state_t *state)
{
  struct sensor_temp sample;
  float rounded;

  if (read_latest(bridge->temp_fd, &sample, sizeof(sample)))
    {
      int value_c;

      rounded = sample.temperature >= 0.0f ? sample.temperature + 0.5f :
                sample.temperature - 0.5f;
      value_c = (int)rounded;
      if (!bridge->have_temperature ||
          bridge->last_temperature_c != value_c)
        {
          printf("smart_band: temperature sensor %dC\n", value_c);
        }

      bridge->last_temperature_c = value_c;
      bridge->have_temperature = true;
    }

  if (bridge->have_temperature)
    {
      state->temperature_c = bridge->last_temperature_c;
      state->temperature_sensor_active = true;
    }
}

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
  bridge->last_temperature_c = 24;

  bridge->hrate_fd = open_sensor(SMART_BAND_HRATE_DEV);
  bridge->accel_fd = open_sensor(SMART_BAND_ACCEL_DEV);
  bridge->step_fd = open_sensor(SMART_BAND_STEP_DEV);
  bridge->battery_fd = open(SMART_BAND_BATTERY_DEV, O_RDONLY | O_NONBLOCK);
  bridge->temp_fd = open_sensor(SMART_BAND_TEMP_DEV);
  if (bridge->temp_fd < 0)
    {
      bridge->temp_fd = open_sensor(SMART_BAND_TEMP_FALLBACK_DEV);
    }
}

void smart_band_sensor_bridge_update(smart_band_sensor_bridge_t *bridge,
                                     smart_band_state_t *state)
{
  if (bridge == NULL || state == NULL)
    {
      return;
    }

  state->heart_sensor_active = false;
  state->step_sensor_active = false;
  state->battery_sensor_active = false;
  state->battery_charging = false;
  state->temperature_sensor_active = false;

  update_heart_rate(bridge, state);
  update_steps_from_counter(bridge, state);
  if (!state->step_sensor_active)
    {
      update_steps_from_accel(bridge, state);
    }

  update_battery(bridge, state);
  update_temperature(bridge, state);
}

void smart_band_sensor_bridge_deinit(smart_band_sensor_bridge_t *bridge)
{
  if (bridge == NULL)
    {
      return;
    }

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

  bridge->hrate_fd = -1;
  bridge->accel_fd = -1;
  bridge->step_fd = -1;
  bridge->battery_fd = -1;
  bridge->temp_fd = -1;
  bridge->have_temperature = false;
}
