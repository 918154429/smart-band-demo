#ifndef SMART_BAND_CAPABILITIES_H
#define SMART_BAND_CAPABILITIES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  bool display;
  bool backlight;
  bool touch;
  bool physical_button;
  bool monotonic_clock;
  bool rtc;
  bool storage;
  bool heart_rate;
  bool step_counter;
  bool accelerometer;
  bool temperature;
  bool humidity;
  bool battery;
  bool charging;
  bool wrist_gesture;
  bool haptic;
  bool ble;
  bool sleep;
  bool wake;
} smart_band_capabilities_t;

void smart_band_capabilities_init_base(
  smart_band_capabilities_t *capabilities);

#ifdef __cplusplus
}
#endif

#endif
