#include "smart_band_storage_transaction.h"

#include <string.h>

#define TRANSACTION_MAGIC UINT32_C(0x31584253)
#define TRANSACTION_STATE_EMPTY 0u
#define TRANSACTION_STATE_PREPARED 1u
#define TRANSACTION_MANIFEST_HEADER_SIZE 8u
#define TRANSACTION_MANIFEST_ENTRY_SIZE 24u
#define TRANSACTION_MANIFEST_MAX_SIZE                                    \
  (TRANSACTION_MANIFEST_HEADER_SIZE +                                   \
   SMART_BAND_STORAGE_TRANSACTION_MAX_PARTICIPANTS *                    \
     TRANSACTION_MANIFEST_ENTRY_SIZE)

#define TRANSACTION_STAGE_OBJECT_BASE UINT32_C(0x00050000)
#define TRANSACTION_MANIFEST_SLOT_A UINT32_C(0x00050008)
#define TRANSACTION_MANIFEST_SLOT_B UINT32_C(0x00050009)

typedef struct
{
  uint32_t slot_object_ids[SMART_BAND_STORE_SLOT_COUNT];
  uint16_t record_type;
  uint16_t schema_major;
  uint16_t schema_minor;
  uint16_t payload_size;
  uint32_t payload_crc;
  uint8_t stage_index;
} transaction_entry_t;

typedef struct
{
  uint8_t state;
  uint8_t count;
  transaction_entry_t
    entries[SMART_BAND_STORAGE_TRANSACTION_MAX_PARTICIPANTS];
} transaction_manifest_t;

static const smart_band_store_record_spec_t g_stage_specs[] =
{
  {{UINT32_C(0x00050000), UINT32_C(0x00050001)},
    SMART_BAND_STORAGE_RECORD_TRANSACTION_STAGE, 1, 0,
    NULL, 0, NULL, NULL},
  {{UINT32_C(0x00050002), UINT32_C(0x00050003)},
    SMART_BAND_STORAGE_RECORD_TRANSACTION_STAGE, 1, 0,
    NULL, 0, NULL, NULL},
  {{UINT32_C(0x00050004), UINT32_C(0x00050005)},
    SMART_BAND_STORAGE_RECORD_TRANSACTION_STAGE, 1, 0,
    NULL, 0, NULL, NULL},
  {{UINT32_C(0x00050006), UINT32_C(0x00050007)},
    SMART_BAND_STORAGE_RECORD_TRANSACTION_STAGE, 1, 0,
    NULL, 0, NULL, NULL}
};

static const smart_band_store_record_spec_t g_manifest_spec =
{
  {TRANSACTION_MANIFEST_SLOT_A, TRANSACTION_MANIFEST_SLOT_B},
  SMART_BAND_STORAGE_RECORD_TRANSACTION_MANIFEST, 1, 0,
  NULL, 0, NULL, NULL
};

static uint16_t read_le16(const uint8_t *input)
{
  return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8));
}

static uint32_t read_le32(const uint8_t *input)
{
  return (uint32_t)input[0] | ((uint32_t)input[1] << 8) |
         ((uint32_t)input[2] << 16) | ((uint32_t)input[3] << 24);
}

static void write_le16(uint8_t *output, uint16_t value)
{
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *output, uint32_t value)
{
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8);
  output[2] = (uint8_t)(value >> 16);
  output[3] = (uint8_t)(value >> 24);
}

static bool transaction_object_id(uint32_t object_id)
{
  return (object_id >= TRANSACTION_STAGE_OBJECT_BASE &&
          object_id <= TRANSACTION_MANIFEST_SLOT_B);
}

static bool target_is_valid(const smart_band_store_record_spec_t *target)
{
  return target != NULL && target->record_type != 0u &&
         target->schema_major != 0u &&
         target->slot_object_ids[0] != target->slot_object_ids[1] &&
         !transaction_object_id(target->slot_object_ids[0]) &&
         !transaction_object_id(target->slot_object_ids[1]) &&
         target->migrate == NULL;
}

static bool participants_are_valid(
  const smart_band_storage_transaction_participant_t *participants,
  size_t participant_count)
{
  size_t index;
  size_t other;

  if (participants == NULL || participant_count == 0u ||
      participant_count > SMART_BAND_STORAGE_TRANSACTION_MAX_PARTICIPANTS)
    {
      return false;
    }

  for (index = 0; index < participant_count; index++)
    {
      if (!target_is_valid(&participants[index].target) ||
          (participants[index].payload == NULL &&
           participants[index].payload_size != 0u) ||
          participants[index].payload_size >
            SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE)
        {
          return false;
        }

      for (other = 0; other < index; other++)
        {
          if (participants[index].target.slot_object_ids[0] ==
                participants[other].target.slot_object_ids[0] ||
              participants[index].target.slot_object_ids[0] ==
                participants[other].target.slot_object_ids[1] ||
              participants[index].target.slot_object_ids[1] ==
                participants[other].target.slot_object_ids[0] ||
              participants[index].target.slot_object_ids[1] ==
                participants[other].target.slot_object_ids[1])
            {
              return false;
            }
        }
    }

  return true;
}

static size_t encode_manifest(const transaction_manifest_t *manifest,
                              uint8_t *payload)
{
  size_t payload_size = TRANSACTION_MANIFEST_HEADER_SIZE +
                        manifest->count * TRANSACTION_MANIFEST_ENTRY_SIZE;
  size_t index;

  memset(payload, 0, payload_size);
  write_le32(payload, TRANSACTION_MAGIC);
  payload[4] = manifest->state;
  payload[5] = manifest->count;
  for (index = 0; index < manifest->count; index++)
    {
      const transaction_entry_t *entry = &manifest->entries[index];
      uint8_t *wire = payload + TRANSACTION_MANIFEST_HEADER_SIZE +
                      index * TRANSACTION_MANIFEST_ENTRY_SIZE;

      wire[0] = entry->stage_index;
      write_le16(wire + 2, entry->record_type);
      write_le16(wire + 4, entry->schema_major);
      write_le16(wire + 6, entry->schema_minor);
      write_le32(wire + 8, entry->slot_object_ids[0]);
      write_le32(wire + 12, entry->slot_object_ids[1]);
      write_le16(wire + 16, entry->payload_size);
      write_le32(wire + 20, entry->payload_crc);
    }

  return payload_size;
}

static bool decode_manifest(const uint8_t *payload, size_t payload_size,
                            transaction_manifest_t *manifest)
{
  size_t index;
  size_t other;

  if (payload == NULL || manifest == NULL ||
      payload_size < TRANSACTION_MANIFEST_HEADER_SIZE ||
      read_le32(payload) != TRANSACTION_MAGIC || payload[6] != 0u ||
      payload[7] != 0u || payload[4] > TRANSACTION_STATE_PREPARED ||
      payload[5] > SMART_BAND_STORAGE_TRANSACTION_MAX_PARTICIPANTS ||
      payload_size != TRANSACTION_MANIFEST_HEADER_SIZE +
                      payload[5] * TRANSACTION_MANIFEST_ENTRY_SIZE)
    {
      return false;
    }

  memset(manifest, 0, sizeof(*manifest));
  manifest->state = payload[4];
  manifest->count = payload[5];
  if ((manifest->state == TRANSACTION_STATE_EMPTY && manifest->count != 0u) ||
      (manifest->state == TRANSACTION_STATE_PREPARED && manifest->count == 0u))
    {
      return false;
    }

  for (index = 0; index < manifest->count; index++)
    {
      transaction_entry_t *entry = &manifest->entries[index];
      const uint8_t *wire = payload + TRANSACTION_MANIFEST_HEADER_SIZE +
                            index * TRANSACTION_MANIFEST_ENTRY_SIZE;

      if (wire[1] != 0u || read_le16(wire + 18) != 0u)
        {
          return false;
        }
      entry->stage_index = wire[0];
      entry->record_type = read_le16(wire + 2);
      entry->schema_major = read_le16(wire + 4);
      entry->schema_minor = read_le16(wire + 6);
      entry->slot_object_ids[0] = read_le32(wire + 8);
      entry->slot_object_ids[1] = read_le32(wire + 12);
      entry->payload_size = read_le16(wire + 16);
      entry->payload_crc = read_le32(wire + 20);
      if (entry->stage_index >=
            SMART_BAND_STORAGE_TRANSACTION_MAX_PARTICIPANTS ||
          entry->record_type == 0u || entry->schema_major == 0u ||
          entry->payload_size > SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE ||
          entry->slot_object_ids[0] == entry->slot_object_ids[1] ||
          transaction_object_id(entry->slot_object_ids[0]) ||
          transaction_object_id(entry->slot_object_ids[1]))
        {
          return false;
        }

      for (other = 0; other < index; other++)
        {
          const transaction_entry_t *previous = &manifest->entries[other];

          if (entry->stage_index == previous->stage_index ||
              entry->slot_object_ids[0] == previous->slot_object_ids[0] ||
              entry->slot_object_ids[0] == previous->slot_object_ids[1] ||
              entry->slot_object_ids[1] == previous->slot_object_ids[0] ||
              entry->slot_object_ids[1] == previous->slot_object_ids[1])
            {
              return false;
            }
        }
    }

  return true;
}

static smart_band_store_result_t clear_manifest(smart_band_store_t *store)
{
  transaction_manifest_t empty;
  uint8_t payload[TRANSACTION_MANIFEST_HEADER_SIZE];
  size_t payload_size;

  memset(&empty, 0, sizeof(empty));
  empty.state = TRANSACTION_STATE_EMPTY;
  payload_size = encode_manifest(&empty, payload);
  return smart_band_store_commit(store, &g_manifest_spec, payload,
                                 payload_size, NULL);
}

static smart_band_store_result_t apply_manifest(
  smart_band_store_t *store, const transaction_manifest_t *manifest)
{
  uint8_t payload[SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE];
  size_t index;

  for (index = 0; index < manifest->count; index++)
    {
      const transaction_entry_t *entry = &manifest->entries[index];
      smart_band_store_record_spec_t target;
      smart_band_store_result_t result;
      size_t payload_size = 0u;

      result = smart_band_store_load(store, &g_stage_specs[entry->stage_index],
                                     payload, sizeof(payload), &payload_size,
                                     NULL);
      if (result < SMART_BAND_STORE_OK ||
          result == SMART_BAND_STORE_DEFAULTED ||
          result == SMART_BAND_STORE_DEGRADED ||
          payload_size != entry->payload_size ||
          smart_band_storage_codec_crc32(payload, payload_size) !=
            entry->payload_crc)
        {
          return result < SMART_BAND_STORE_OK ? result :
                 SMART_BAND_STORE_DEGRADED;
        }

      memset(&target, 0, sizeof(target));
      target.slot_object_ids[0] = entry->slot_object_ids[0];
      target.slot_object_ids[1] = entry->slot_object_ids[1];
      target.record_type = entry->record_type;
      target.schema_major = entry->schema_major;
      target.schema_minor = entry->schema_minor;
      result = smart_band_store_commit(store, &target, payload, payload_size,
                                       NULL);
      if (result != SMART_BAND_STORE_OK)
        {
          return result;
        }
    }

  return SMART_BAND_STORE_OK;
}

static bool manifest_was_never_published(const smart_band_store_t *store,
                                         size_t payload_size)
{
  size_t slot;

  if (payload_size != 0u)
    {
      return false;
    }

  for (slot = 0; slot < SMART_BAND_STORE_SLOT_COUNT; slot++)
    {
      if (store->slot_states[slot] != SMART_BAND_STORE_SLOT_MISSING &&
          store->slot_states[slot] != SMART_BAND_STORE_SLOT_CORRUPT)
        {
          return false;
        }
    }

  return true;
}

smart_band_store_result_t smart_band_storage_transaction_recover(
  smart_band_store_t *store)
{
  uint8_t payload[TRANSACTION_MANIFEST_MAX_SIZE];
  transaction_manifest_t manifest;
  smart_band_store_result_t result;
  size_t payload_size = 0u;

  if (store == NULL || !store->initialized)
    {
      return SMART_BAND_STORE_INVALID;
    }

  result = smart_band_store_load(store, &g_manifest_spec, payload,
                                 sizeof(payload), &payload_size, NULL);
  if (result < SMART_BAND_STORE_OK || result == SMART_BAND_STORE_UNAVAILABLE)
    {
      return result;
    }
  if (result == SMART_BAND_STORE_DEFAULTED && payload_size == 0u)
    {
      return SMART_BAND_STORE_OK;
    }
  if (result == SMART_BAND_STORE_DEGRADED &&
      manifest_was_never_published(store, payload_size))
    {
      return SMART_BAND_STORE_OK;
    }
  if (result == SMART_BAND_STORE_DEGRADED ||
      !decode_manifest(payload, payload_size, &manifest))
    {
      return SMART_BAND_STORE_DEGRADED;
    }
  if (manifest.state == TRANSACTION_STATE_EMPTY)
    {
      return SMART_BAND_STORE_OK;
    }

  result = apply_manifest(store, &manifest);
  return result == SMART_BAND_STORE_OK ? clear_manifest(store) : result;
}

smart_band_store_result_t smart_band_storage_transaction_commit(
  smart_band_store_t *store,
  const smart_band_storage_transaction_participant_t *participants,
  size_t participant_count)
{
  uint8_t manifest_payload[TRANSACTION_MANIFEST_MAX_SIZE];
  transaction_manifest_t manifest;
  smart_band_store_result_t result;
  size_t manifest_size;
  size_t index;
  unsigned int copy;

  if (store == NULL || !store->initialized ||
      !participants_are_valid(participants, participant_count))
    {
      return SMART_BAND_STORE_INVALID;
    }

  result = smart_band_storage_transaction_recover(store);
  if (result != SMART_BAND_STORE_OK)
    {
      return result;
    }

  memset(&manifest, 0, sizeof(manifest));
  manifest.state = TRANSACTION_STATE_PREPARED;
  manifest.count = (uint8_t)participant_count;
  for (index = 0; index < participant_count; index++)
    {
      const smart_band_storage_transaction_participant_t *participant =
        &participants[index];
      transaction_entry_t *entry = &manifest.entries[index];

      for (copy = 0; copy < SMART_BAND_STORE_SLOT_COUNT; copy++)
        {
          result = smart_band_store_commit(
            store, &g_stage_specs[index], participant->payload,
            participant->payload_size, NULL);
          if (result != SMART_BAND_STORE_OK)
            {
              return result;
            }
        }

      entry->stage_index = (uint8_t)index;
      entry->slot_object_ids[0] = participant->target.slot_object_ids[0];
      entry->slot_object_ids[1] = participant->target.slot_object_ids[1];
      entry->record_type = participant->target.record_type;
      entry->schema_major = participant->target.schema_major;
      entry->schema_minor = participant->target.schema_minor;
      entry->payload_size = (uint16_t)participant->payload_size;
      entry->payload_crc = smart_band_storage_codec_crc32(
        participant->payload, participant->payload_size);
    }

  manifest_size = encode_manifest(&manifest, manifest_payload);
  for (copy = 0; copy < SMART_BAND_STORE_SLOT_COUNT; copy++)
    {
      result = smart_band_store_commit(store, &g_manifest_spec,
                                       manifest_payload, manifest_size, NULL);
      if (result != SMART_BAND_STORE_OK)
        {
          return result;
        }
    }

  result = apply_manifest(store, &manifest);
  return result == SMART_BAND_STORE_OK ? clear_manifest(store) : result;
}
