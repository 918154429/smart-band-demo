#include "smart_band_apps.h"

#include "icon_assets.h"

#include <stdio.h>

static lv_obj_t *g_temp;
static lv_obj_t *g_source;
static lv_obj_t *g_condition;
static lv_obj_t *g_sky;
static lv_obj_t *g_range;
static lv_obj_t *g_humidity;
static lv_obj_t *g_wind;

static lv_obj_t *weather_label(lv_obj_t *parent, const char *text,
                               const smart_band_app_host_t *host,
                               const lv_font_t *font, uint32_t color,
                               lv_text_align_t align,
                               lv_coord_t x, lv_coord_t y,
                               lv_coord_t w, lv_coord_t h)
{
  lv_obj_t *label = lv_label_create(parent);

  if (label == NULL)
    {
      return NULL;
    }

  lv_obj_remove_style_all(label);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(label, align, 0);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_size(label, w, h);
  return label;
}

static lv_obj_t *weather_icon(lv_obj_t *parent,
                              const smart_band_app_host_t *host,
                              const lv_image_dsc_t *src, lv_coord_t x,
                              lv_coord_t y, lv_coord_t size)
{
  lv_obj_t *image;
  uint32_t scale;

  if (src == NULL || size <= 0)
    {
      return NULL;
    }

  image = lv_image_create(parent);
  if (image == NULL)
    {
      return NULL;
    }

  lv_obj_remove_style_all(image);
  lv_obj_clear_flag(image, LV_OBJ_FLAG_SCROLLABLE);
  lv_image_set_src(image, src);
  scale = (uint32_t)((size * LV_SCALE_NONE) / 48);
  if (scale == 0)
    {
      scale = 1;
    }

  lv_image_set_scale(image, scale);
  lv_obj_set_pos(image, x, y);
  lv_obj_set_size(image, size, size);
  return image;
}

static int weather_metric(lv_obj_t *parent, const smart_band_app_host_t *host,
                          lv_coord_t x, lv_coord_t y, lv_coord_t w,
                          const char *title, uint32_t accent,
                          lv_obj_t **value_out)
{
  lv_obj_t *card;
  lv_obj_t *dot;
  lv_obj_t *title_label;

  card = host->create_box(parent, x, y, w, host->sy(58),
                          lv_color_hex(0xffffff), host->sx(16));
  if (card == NULL)
    {
      return -1;
    }

  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0xe3eef0), 0);
  lv_obj_set_style_shadow_width(card, host->sx(5), 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x314856), 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
  lv_obj_set_style_shadow_offset_y(card, host->sy(2), 0);

  dot = host->create_box(card, host->sx(10), host->sy(12),
                         host->sx(7), host->sx(7), lv_color_hex(accent),
                         LV_RADIUS_CIRCLE);
  title_label = weather_label(card, title, host, host->font_12(), 0x81939a,
                              LV_TEXT_ALIGN_LEFT, host->sx(22),
                              host->sy(7), w - host->sx(32), host->sy(18));
  *value_out = weather_label(card, "--", host, host->font_16(), 0x293b53,
                             LV_TEXT_ALIGN_LEFT, host->sx(22),
                             host->sy(27), w - host->sx(32), host->sy(26));
  if (dot == NULL || title_label == NULL || *value_out == NULL)
    {
      return -1;
    }

  return 0;
}

void smart_band_weather_app_update(const smart_band_app_host_t *host)
{
  char temp[32];
  char source[40];
  char humidity[16];
  char wind[16];
  const char *condition;

  if (host == NULL || host->model == NULL)
    {
      return;
    }

  host->format_temperature(temp, sizeof(temp));
  snprintf(source, sizeof(source), "%s",
           host->model->temperature_sensor_active ? "ambient_temp0" :
           "model fallback");
  snprintf(humidity, sizeof(humidity), "%d%%",
           host->model->humidity_percent);
  snprintf(wind, sizeof(wind), "E%d", 2 + (int)(host->model->ticks % 3u));
  condition = host->model->temperature_c >= 30 ? "Sunny" : "Cloudy";

  host->set_label_text(g_temp, temp);
  host->set_label_text(g_source, source);
  host->set_label_text(g_condition, condition);
  host->set_label_text(g_sky, condition);
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
  lv_obj_t *hero_icon;
  lv_obj_t *caption;
  lv_obj_t *sensor_pill;
  lv_coord_t margin = host->sx(18);
  lv_coord_t hero_w = host->screen_w - margin * 2;
  lv_coord_t card_w = (host->screen_w - margin * 2 - host->sx(10)) / 2;
  lv_coord_t stats_y = host->sy(150);

  g_temp = NULL;
  g_source = NULL;
  g_condition = NULL;
  g_sky = NULL;
  g_range = NULL;
  g_humidity = NULL;
  g_wind = NULL;

  hero = host->create_box(parent, margin, host->sy(6), hero_w, host->sy(132),
                          lv_color_hex(0xeefbf8), host->sx(24));
  if (hero == NULL)
    {
      return -1;
    }

  lv_obj_set_style_bg_grad_color(hero, lv_color_hex(0xfff7e8), 0);
  lv_obj_set_style_bg_grad_dir(hero, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_border_width(hero, 1, 0);
  lv_obj_set_style_border_color(hero, lv_color_hex(0xd9efec), 0);

  hero_icon = weather_icon(hero, host, &smart_band_icon_weather,
                           host->sx(18), host->sy(22), host->sx(56));
  g_temp = weather_label(hero, "--", host, host->font_32(), 0x293b53,
                         LV_TEXT_ALIGN_LEFT, host->sx(88), host->sy(16),
                         hero_w - host->sx(108), host->sy(44));
  g_condition = weather_label(hero, "--", host, host->font_16(), 0x3b8880,
                              LV_TEXT_ALIGN_LEFT, host->sx(90),
                              host->sy(60), hero_w - host->sx(110),
                              host->sy(24));
  sensor_pill = host->create_box(hero, host->sx(88), host->sy(92),
                                 hero_w - host->sx(108), host->sy(28),
                                 lv_color_hex(0xffffff), host->sx(14));
  caption = weather_label(sensor_pill, "Sensor", host, host->font_12(),
                          0x81939a, LV_TEXT_ALIGN_LEFT, host->sx(10),
                          host->sy(6), host->sx(54), host->sy(16));
  g_source = weather_label(sensor_pill, "--", host, host->font_12(),
                           0xe4a840, LV_TEXT_ALIGN_RIGHT, host->sx(62),
                           host->sy(6), hero_w - host->sx(180),
                           host->sy(16));
  if (hero_icon == NULL || g_temp == NULL ||
      g_condition == NULL || sensor_pill == NULL || caption == NULL ||
      g_source == NULL)
    {
      return -1;
    }

  if (weather_metric(parent, host, margin, stats_y, card_w, "Sky", 0x80cbc3,
                     &g_sky) != 0 ||
      weather_metric(parent, host, margin + card_w + host->sx(10), stats_y,
                     card_w, "Range", 0xf5c66e, &g_range) != 0 ||
      weather_metric(parent, host, margin, stats_y + host->sy(68), card_w,
                     "Humidity", 0x8aa8d8, &g_humidity) != 0 ||
      weather_metric(parent, host, margin + card_w + host->sx(10),
                     stats_y + host->sy(68), card_w, "Wind", 0xf08d88,
                     &g_wind) != 0)
    {
      return -1;
    }

  smart_band_weather_app_update(host);
  return 0;
}
