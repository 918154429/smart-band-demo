#ifndef SMART_BAND_SENSOR_BRIDGE_H
#define SMART_BAND_SENSOR_BRIDGE_H

#include "watch_model.h"

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
  bool have_last_accel;
  bool have_temperature;
  bool have_humidity;
} smart_band_sensor_bridge_t;

void smart_band_sensor_bridge_init(smart_band_sensor_bridge_t *bridge);
void smart_band_sensor_bridge_update(smart_band_sensor_bridge_t *bridge,
                                     smart_band_state_t *state);
void smart_band_sensor_bridge_deinit(smart_band_sensor_bridge_t *bridge);

#ifdef __cplusplus
}
#endif

#endif
