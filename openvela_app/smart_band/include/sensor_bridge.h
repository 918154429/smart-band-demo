#ifndef SMART_BAND_SENSOR_BRIDGE_H
#define SMART_BAND_SENSOR_BRIDGE_H

#include "watch_model.h"
#include "smart_band_step_normalizer.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  int hrate_fd;
  int accel_fd;
  int step_fd;
  int battery_fd;
  int temp_fd;
  int humi_fd;
  int last_temperature_c;
  int last_humidity_percent;
  int derived_steps;
  float last_accel_energy;
  uint64_t raw_step_counter;
  smart_band_step_source_t raw_step_source;
  bool have_last_accel;
  bool have_temperature;
  bool have_humidity;
  bool have_raw_step;
} smart_band_sensor_bridge_t;

void smart_band_sensor_bridge_init(smart_band_sensor_bridge_t *bridge);
void smart_band_sensor_bridge_update(smart_band_sensor_bridge_t *bridge,
                                     smart_band_state_t *state);
void smart_band_sensor_bridge_update_at(smart_band_sensor_bridge_t *bridge,
                                        smart_band_state_t *state,
                                        time_t now);
void smart_band_sensor_bridge_update_clocked(
  smart_band_sensor_bridge_t *bridge, smart_band_state_t *state,
  time_t wall_now, uint64_t monotonic_ms, bool wall_rollback);
void smart_band_sensor_bridge_deinit(smart_band_sensor_bridge_t *bridge);
bool smart_band_sensor_bridge_step_sample(
  const smart_band_sensor_bridge_t *bridge, uint64_t monotonic_ms,
  bool fresh, smart_band_step_sample_t *sample);

#ifdef __cplusplus
}
#endif

#endif
