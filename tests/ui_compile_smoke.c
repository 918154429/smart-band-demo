#include "app_lvgl.h"
#include "smart_band_apps.h"

#include <stdio.h>

int main(void)
{
  size_t count = 0;
  const smart_band_app_def_t *catalog = smart_band_apps_catalog(&count);

  if (catalog == NULL || count != SMART_BAND_APP_COUNT)
    {
      fprintf(stderr, "smart band app catalog smoke test failed\n");
      return 1;
    }

  return 0;
}
