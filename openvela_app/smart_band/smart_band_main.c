#include <nuttx/config.h>

#include <stdio.h>
#include <uv.h>

#include <lvgl/lvgl.h>

#include "app_lvgl.h"

static void smart_band_lv_nuttx_uv_loop(uv_loop_t *loop,
                                        lv_nuttx_result_t *result)
{
  lv_nuttx_uv_t uv_info;
  void *data;

  uv_loop_init(loop);

  lv_memset(&uv_info, 0, sizeof(uv_info));
  uv_info.loop = loop;
  uv_info.disp = result->disp;
  uv_info.indev = result->indev;
#ifdef CONFIG_UINPUT_TOUCH
  uv_info.uindev = result->utouch_indev;
#endif

  data = lv_nuttx_uv_init(&uv_info);
  uv_run(loop, UV_RUN_DEFAULT);
  lv_nuttx_uv_deinit(&data);
}

int main(int argc, char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;
  uv_loop_t ui_loop;

  (void)argc;
  (void)argv;

  lv_memset(&ui_loop, 0, sizeof(uv_loop_t));

  if (lv_is_initialized())
    {
      printf("smart_band: LVGL already initialized\n");
      return 1;
    }

  lv_init();

  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, &result);

  if (result.disp == NULL)
    {
      printf("smart_band: LVGL display initialization failed\n");
      lv_deinit();
      return 1;
    }

  if (smart_band_lvgl_create(NULL) != 0)
    {
      printf("smart_band: failed to create LVGL UI\n");
      lv_nuttx_deinit(&result);
      lv_deinit();
      return 1;
    }

  printf("smart_band: UI ready\n");
  fflush(stdout);
  smart_band_lv_nuttx_uv_loop(&ui_loop, &result);

  smart_band_lvgl_destroy();
  lv_nuttx_deinit(&result);
  lv_deinit();

  return 0;
}
