#ifndef SMART_BAND_STORAGE_TRANSACTION_H
#define SMART_BAND_STORAGE_TRANSACTION_H

#include "smart_band_store.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_STORAGE_TRANSACTION_MAX_PARTICIPANTS 4u

typedef struct
{
  smart_band_store_record_spec_t target;
  const uint8_t *payload;
  size_t payload_size;
} smart_band_storage_transaction_participant_t;

/*
 * A transaction first copies every target payload into redundant staging
 * records, then publishes a redundant manifest. Recovery replays a published
 * manifest until every target is verified and the manifest is cleared.
 */
smart_band_store_result_t smart_band_storage_transaction_recover(
  smart_band_store_t *store);

smart_band_store_result_t smart_band_storage_transaction_commit(
  smart_band_store_t *store,
  const smart_band_storage_transaction_participant_t *participants,
  size_t participant_count);

#ifdef __cplusplus
}
#endif

#endif
