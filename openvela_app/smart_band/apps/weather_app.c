#include "smart_band_apps.h"

#include <stdio.h>

static lv_obj_t *g_temp;
static lv_obj_t *g_source;
static lv_obj_t *g_sky;
static lv_obj_t *g_range;
static lv_obj_t *g_humidity;
static lv_obj_t *g_wind;

void smart_band_weather_app_update(const smart_band_app_host_t *host)
{
  char temp[32];
  char source[40];
  char humidity[16];
  char wind[16];

  if (host == NULL || host->model == NULL)
    {
      return;
    }

  host->format_temperature(temp, sizeof(temp));
  snprintf(source, sizeof(source), "%s",
           host->model->temperature_sensor_active ? "ambient_temp0" :
           "model fallback");
  snprintf(humidity, sizeof(humidity), "%d%%",
           54 + (int)(host->model->ticks % 10u));
  snprintf(wind, sizeof(wind), "E%d", 2 + (int)(host->model->ticks % 3u));

  host->set_label_text(g_temp, temp);
  host->set_label_text(g_source, source);
  host->set_label_text(g_sky,
                       host->model->temperature_c >= 30 ? "Sunny" :
                       "Cloudy");
  host->set_label_text(g_range,
                       host->model->temperature_c >= 30 ? "32/26" :
                       "28/22");
  host->set_label_text(g_humidity, humidity);
  host->set_label_text(g_wind, wind);
}

int smart_band_weather_app_build(lv_obj_t *parent,
                                 const smart_band_app_host_t *host)
{
  lv_obj_t *hero;
  lv_obj_t *caption;

  g_temp = NULL;
  g_source = NULL;
  g_sky = NULL;
  g_range = NULL;
  g_humidity = NULL;
  g_wind = NULL;

  hero = host->create_box(parent, host->sx(22), host->sy(8),
                          host->screen_w - host->sx(44), host->sy(128),
                          lv_color_hex(0xfff6e2), host->sx(24));
  if (hero == NULL)
    {
      return -1;
    }

  g_temp = host->create_label(hero, "--", host->font_32(),
                              lv_color_hex(0x293b53),
                              LV_TEXT_ALIGN_CENTER);
  caption = host->create_label(hero, "Temperature sensor", host->font_12(),
                               lv_color_hex(0x81939a),
                               LV_TEXT_ALIGN_CENTER);
  g_source = host->create_label(hero, "--", host->font_14(),
                                lv_color_hex(0xe4a840),
                                LV_TEXT_ALIGN_CENTER);
  if (g_temp == NULL || caption == NULL || g_source == NULL)
    {
      return -1;
    }

  host->place_label(g_temp, host->sx(12), host->sy(22),
                    host->screen_w - host->sx(68), host->sy(44));
  host->place_label(caption, host->sx(12), host->sy(72),
                    host->screen_w - host->sx(68), host->sy(20));
  host->place_label(g_source, host->sx(12), host->sy(94),
                    host->screen_w - host->sx(68), host->sy(22));

  if (host->create_app_stat(parent, 0, 0, "Sky", &g_sky) != 0 ||
      host->create_app_stat(parent, 1, 0, "Range", &g_range) != 0 ||
      host->create_app_stat(parent, 0, 1, "Humidity", &g_humidity) != 0 ||
      host->create_app_stat(parent, 1, 1, "Wind", &g_wind) != 0)
    {
      return -1;
    }

  smart_band_weather_app_update(host);
  return 0;
}
