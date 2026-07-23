#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <uv.h>

#include <lvgl/lvgl.h>

#include "app_lvgl.h"

#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
#  define SMART_BAND_Q4_SCENARIO_PREFIX "--q4-native-scenario="

static const char *q4_native_scenario_from_args(int argc, char *argv[])
{
  size_t prefix_length = strlen(SMART_BAND_Q4_SCENARIO_PREFIX);

  if (argc == 1)
    {
      return NULL;
    }
  if (argc != 2 || argv == NULL || argv[1] == NULL ||
      strncmp(argv[1], SMART_BAND_Q4_SCENARIO_PREFIX, prefix_length) != 0 ||
      argv[1][prefix_length] == '\0')
    {
      return "";
    }
  return argv[1] + prefix_length;
}
#endif

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
#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
  const char *q4_native_scenario = q4_native_scenario_from_args(argc, argv);

  if (q4_native_scenario != NULL && q4_native_scenario[0] == '\0')
    {
      printf("smart_band:q4:inject:v1 scenario=invalid phase=rejected "
             "accepted=0 requested=0\n");
      fflush(stdout);
      return 2;
    }
#else
  (void)argc;
  (void)argv;
#endif

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

#if defined(CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS)
  if (q4_native_scenario != NULL &&
      !smart_band_lvgl_inject_q4_native_scenario_for_test(q4_native_scenario))
    {
      smart_band_lvgl_destroy();
      lv_nuttx_deinit(&result);
      lv_deinit();
      return 2;
    }
#endif

  printf("smart_band: UI ready\n");
  fflush(stdout);
  smart_band_lv_nuttx_uv_loop(&ui_loop, &result);

  smart_band_lvgl_destroy();
  lv_nuttx_deinit(&result);
  lv_deinit();

  return 0;
}
