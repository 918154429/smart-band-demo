#include "smart_band_capabilities.h"

#include <string.h>

void smart_band_capabilities_init_base(
  smart_band_capabilities_t *capabilities)
{
  if (capabilities == NULL)
    {
      return;
    }

  memset(capabilities, 0, sizeof(*capabilities));
  capabilities->display = true;
  capabilities->backlight = true;
  capabilities->touch = true;
  capabilities->monotonic_clock = true;
}
