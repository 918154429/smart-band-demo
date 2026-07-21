#ifndef SMART_BAND_WATCH_FACE_SETTINGS_H
#define SMART_BAND_WATCH_FACE_SETTINGS_H

#include "smart_band_store.h"
#include "smart_band_watch_face_id.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_WATCH_FACE_SETTINGS_SLOT_A UINT32_C(0x00020000)
#define SMART_BAND_WATCH_FACE_SETTINGS_SLOT_B UINT32_C(0x00020001)
#define SMART_BAND_WATCH_FACE_SETTINGS_PAYLOAD_SIZE 4u

smart_band_store_result_t smart_band_watch_face_settings_load(
  smart_band_store_t *store, smart_band_watch_face_id_t *selected_face);
smart_band_store_result_t smart_band_watch_face_settings_commit(
  smart_band_store_t *store, smart_band_watch_face_id_t selected_face,
  uint64_t *generation);

#ifdef __cplusplus
}
#endif

#endif
