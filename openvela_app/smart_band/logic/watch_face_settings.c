#include "smart_band_watch_face_settings.h"

#include <stddef.h>

static const uint8_t g_default_payload
  [SMART_BAND_WATCH_FACE_SETTINGS_PAYLOAD_SIZE] =
{
  SMART_BAND_WATCH_FACE_LOTUS, 0, 0, 0
};

static int face_id_is_valid(uint8_t value)
{
  return value == SMART_BAND_WATCH_FACE_LOTUS ||
         value == SMART_BAND_WATCH_FACE_ACTIVITY ||
         value == SMART_BAND_WATCH_FACE_MINIMAL;
}

static smart_band_store_migration_result_t migrate_settings(
  void *context, uint16_t source_schema_major,
  uint16_t source_schema_minor, const uint8_t *source, size_t source_size,
  uint16_t target_schema_major, uint16_t target_schema_minor,
  uint8_t *output, size_t output_capacity, size_t *output_size)
{
  (void)context;

  if (output_size == NULL)
    {
      return SMART_BAND_STORE_MIGRATION_INVALID;
    }

  *output_size = 0;
  if (source_schema_major != 1 || source_schema_minor != 0 ||
      target_schema_major != 1 || target_schema_minor != 1 ||
      source == NULL || source_size != 1 || !face_id_is_valid(source[0]))
    {
      return SMART_BAND_STORE_MIGRATION_UNSUPPORTED;
    }

  if (output == NULL ||
      output_capacity < SMART_BAND_WATCH_FACE_SETTINGS_PAYLOAD_SIZE)
    {
      return SMART_BAND_STORE_MIGRATION_BUFFER_TOO_SMALL;
    }

  output[0] = source[0];
  output[1] = 0;
  output[2] = 0;
  output[3] = 0;
  *output_size = SMART_BAND_WATCH_FACE_SETTINGS_PAYLOAD_SIZE;
  return SMART_BAND_STORE_MIGRATION_OK;
}

static const smart_band_store_record_spec_t g_settings_spec =
{
  {SMART_BAND_WATCH_FACE_SETTINGS_SLOT_A,
   SMART_BAND_WATCH_FACE_SETTINGS_SLOT_B},
  SMART_BAND_STORAGE_RECORD_SETTINGS,
  1,
  1,
  g_default_payload,
  sizeof(g_default_payload),
  migrate_settings,
  NULL
};

smart_band_store_result_t smart_band_watch_face_settings_load(
  smart_band_store_t *store, smart_band_watch_face_id_t *selected_face)
{
  uint8_t payload[SMART_BAND_WATCH_FACE_SETTINGS_PAYLOAD_SIZE];
  size_t payload_size = 0;
  smart_band_store_result_t result;

  if (store == NULL || selected_face == NULL)
    {
      return SMART_BAND_STORE_INVALID;
    }

  *selected_face = SMART_BAND_WATCH_FACE_LOTUS;
  result = smart_band_store_load(store, &g_settings_spec, payload,
                                 sizeof(payload), &payload_size, NULL);
  if (result < SMART_BAND_STORE_OK)
    {
      return result;
    }

  if (payload_size != sizeof(payload) || !face_id_is_valid(payload[0]) ||
      payload[1] != 0 || payload[2] != 0 || payload[3] != 0)
    {
      return SMART_BAND_STORE_DEGRADED;
    }

  *selected_face = (smart_band_watch_face_id_t)payload[0];
  return result;
}

smart_band_store_result_t smart_band_watch_face_settings_commit(
  smart_band_store_t *store, smart_band_watch_face_id_t selected_face,
  uint64_t *generation)
{
  uint8_t payload[SMART_BAND_WATCH_FACE_SETTINGS_PAYLOAD_SIZE] = {0, 0, 0, 0};

  if (store == NULL || !face_id_is_valid((uint8_t)selected_face))
    {
      return SMART_BAND_STORE_INVALID;
    }

  payload[0] = (uint8_t)selected_face;
  return smart_band_store_commit(store, &g_settings_spec, payload,
                                 sizeof(payload), generation);
}
