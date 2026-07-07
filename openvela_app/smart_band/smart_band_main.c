#include "app_lvgl.h"

#include <lvgl.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#ifndef CONFIG_LVX_DEMO_SMART_BAND_BASIC_LOOP_DELAY_US
#  define CONFIG_LVX_DEMO_SMART_BAND_BASIC_LOOP_DELAY_US 5000
#endif

int main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

#ifdef CONFIG_LVX_DEMO_SMART_BAND_BASIC_STANDALONE_INIT
  lv_init();
#endif

  if (smart_band_lvgl_create(NULL) != 0)
    {
      printf("smart_band: failed to create LVGL UI\n");
      return 1;
    }

  while (true)
    {
      lv_timer_handler();
      usleep(CONFIG_LVX_DEMO_SMART_BAND_BASIC_LOOP_DELAY_US);
    }

  smart_band_lvgl_destroy();
  return 0;
}
