#include "smart_band_store.h"

#include <limits.h>
#include <string.h>

typedef struct
{
  smart_band_store_slot_state_t state;
  uint16_t schema_major;
  uint16_t schema_minor;
  uint64_t generation;
  size_t payload_size;
} slot_record_t;

static bool spec_is_valid(const smart_band_store_record_spec_t *spec)
{
  return spec != NULL &&
         spec->slot_object_ids[0] != spec->slot_object_ids[1] &&
         spec->record_type != SMART_BAND_STORAGE_CODEC_RECORD_TYPE_ANY &&
         spec->schema_major != 0 &&
         spec->default_payload_size <=
           SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE &&
         (spec->default_payload != NULL || spec->default_payload_size == 0);
}

static bool backend_is_valid(const smart_band_storage_t *backend)
{
  return backend != NULL && backend->ops != NULL &&
         backend->ops->read != NULL && backend->ops->write != NULL &&
         backend->ops->flush != NULL;
}

static void reset_observation(smart_band_store_t *store)
{
  size_t slot;

  store->last_backend_result = SMART_BAND_PLATFORM_OK;
  store->generation = 0;
  store->selected_slot = SMART_BAND_STORE_NO_SLOT;
  for (slot = 0; slot < SMART_BAND_STORE_SLOT_COUNT; slot++)
    {
      store->slot_states[slot] = SMART_BAND_STORE_SLOT_MISSING;
    }
}

static smart_band_store_slot_state_t classify_backend_result(
  smart_band_platform_result_t result)
{
  if (result == SMART_BAND_PLATFORM_NOT_FOUND)
    {
      return SMART_BAND_STORE_SLOT_MISSING;
    }

  if (result == SMART_BAND_PLATFORM_UNAVAILABLE)
    {
      return SMART_BAND_STORE_SLOT_UNAVAILABLE;
    }

  return SMART_BAND_STORE_SLOT_IO_ERROR;
}

static void read_slot(smart_band_store_t *store,
                      const smart_band_store_record_spec_t *spec,
                      size_t slot, slot_record_t *record,
                      uint8_t *payload)
{
  smart_band_storage_decode_options_t options;
  smart_band_storage_record_view_t view;
  smart_band_platform_result_t backend_result;
  smart_band_storage_codec_result_t codec_result;
  size_t actual_size = 0;

  memset(record, 0, sizeof(*record));
  backend_result = store->backend.ops->read(
    store->backend.context, spec->slot_object_ids[slot], store->frame,
    sizeof(store->frame), &actual_size);
  if (backend_result != SMART_BAND_PLATFORM_OK)
    {
      record->state = classify_backend_result(backend_result);
      store->slot_states[slot] = record->state;
      if (store->last_backend_result == SMART_BAND_PLATFORM_OK)
        {
          store->last_backend_result = backend_result;
        }

      return;
    }

  memset(&options, 0, sizeof(options));
  options.expected_record_type = spec->record_type;
  codec_result = smart_band_storage_codec_decode(
    store->frame, actual_size, &options, &view);
  if (codec_result != SMART_BAND_STORAGE_CODEC_OK)
    {
      record->state = SMART_BAND_STORE_SLOT_CORRUPT;
      store->slot_states[slot] = record->state;
      return;
    }

  record->state = SMART_BAND_STORE_SLOT_VALID;
  record->schema_major = view.schema_major;
  record->schema_minor = view.schema_minor;
  record->generation = view.generation;
  record->payload_size = view.payload_length;
  if (payload != NULL && view.payload_length != 0)
    {
      memcpy(payload, view.payload, view.payload_length);
    }
  store->slot_states[slot] = record->state;
}

static bool slot_has_problem(smart_band_store_slot_state_t state)
{
  return state == SMART_BAND_STORE_SLOT_CORRUPT ||
         state == SMART_BAND_STORE_SLOT_UNSUPPORTED ||
         state == SMART_BAND_STORE_SLOT_IO_ERROR ||
         state == SMART_BAND_STORE_SLOT_UNAVAILABLE;
}

static smart_band_store_result_t copy_defaults(
  smart_band_store_t *store, const smart_band_store_record_spec_t *spec,
  void *output, size_t output_capacity, size_t *output_size,
  smart_band_store_result_t result)
{
  if (output_capacity < spec->default_payload_size)
    {
      store->last_result = SMART_BAND_STORE_BUFFER_TOO_SMALL;
      return store->last_result;
    }

  if (spec->default_payload_size != 0)
    {
      memmove(output, spec->default_payload, spec->default_payload_size);
    }

  *output_size = spec->default_payload_size;
  store->last_result = result;
  return result;
}

static bool records_equal(const slot_record_t *left,
                          const uint8_t *left_payload,
                          const slot_record_t *right,
                          const uint8_t *right_payload)
{
  return left->schema_major == right->schema_major &&
         left->schema_minor == right->schema_minor &&
         left->generation == right->generation &&
         left->payload_size == right->payload_size &&
         memcmp(left_payload, right_payload, left->payload_size) == 0;
}

int smart_band_store_init(smart_band_store_t *store,
                          const smart_band_storage_t *backend)
{
  if (store == NULL)
    {
      return -1;
    }

  memset(store, 0, sizeof(*store));
  store->selected_slot = SMART_BAND_STORE_NO_SLOT;
  if (!backend_is_valid(backend))
    {
      store->last_result = SMART_BAND_STORE_UNAVAILABLE;
      return -1;
    }

  store->backend = *backend;
  store->initialized = true;
  return 0;
}

void smart_band_store_deinit(smart_band_store_t *store)
{
  if (store != NULL)
    {
      memset(store, 0, sizeof(*store));
      store->selected_slot = SMART_BAND_STORE_NO_SLOT;
    }
}

smart_band_store_result_t smart_band_store_load(
  smart_band_store_t *store, const smart_band_store_record_spec_t *spec,
  void *output, size_t output_capacity, size_t *output_size,
  uint64_t *generation)
{
  slot_record_t slots[SMART_BAND_STORE_SLOT_COUNT];
  uint8_t payloads[SMART_BAND_STORE_SLOT_COUNT]
                  [SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE];
  size_t order[SMART_BAND_STORE_SLOT_COUNT];
  size_t valid_slots = 0;
  size_t selected;
  size_t slot;
  smart_band_store_result_t success_result;

  if (output_size != NULL)
    {
      *output_size = 0;
    }
  if (generation != NULL)
    {
      *generation = 0;
    }

  if (store == NULL || !store->initialized || !spec_is_valid(spec) ||
      output == NULL || output_size == NULL)
    {
      return SMART_BAND_STORE_INVALID;
    }

  reset_observation(store);
  for (slot = 0; slot < SMART_BAND_STORE_SLOT_COUNT; slot++)
    {
      read_slot(store, spec, slot, &slots[slot], payloads[slot]);
      if (slots[slot].state != SMART_BAND_STORE_SLOT_VALID)
        {
          continue;
        }

      order[valid_slots++] = slot;
    }

  if (valid_slots == 0)
    {
      bool unavailable =
        store->slot_states[0] == SMART_BAND_STORE_SLOT_UNAVAILABLE &&
        store->slot_states[1] == SMART_BAND_STORE_SLOT_UNAVAILABLE;
      bool problem = slot_has_problem(store->slot_states[0]) ||
                     slot_has_problem(store->slot_states[1]);

      return copy_defaults(store, spec, output, output_capacity, output_size,
                           unavailable ? SMART_BAND_STORE_UNAVAILABLE :
                           problem ? SMART_BAND_STORE_DEGRADED :
                           SMART_BAND_STORE_DEFAULTED);
    }

  if (valid_slots == 2 &&
      slots[order[0]].generation == slots[order[1]].generation &&
      !records_equal(&slots[order[0]], payloads[order[0]],
                     &slots[order[1]], payloads[order[1]]))
    {
      store->slot_states[0] = SMART_BAND_STORE_SLOT_CORRUPT;
      store->slot_states[1] = SMART_BAND_STORE_SLOT_CORRUPT;
      return copy_defaults(store, spec, output, output_capacity, output_size,
                           SMART_BAND_STORE_DEGRADED);
    }

  if (valid_slots == 2 &&
      slots[order[1]].generation > slots[order[0]].generation)
    {
      size_t temporary = order[0];

      order[0] = order[1];
      order[1] = temporary;
    }

  for (slot = 0; slot < valid_slots; slot++)
    {
      smart_band_store_migration_result_t migration_result =
        SMART_BAND_STORE_MIGRATION_UNSUPPORTED;

      selected = order[slot];
      if (slots[selected].schema_major == spec->schema_major &&
          slots[selected].schema_minor == spec->schema_minor)
        {
          if (output_capacity < slots[selected].payload_size)
            {
              store->last_result = SMART_BAND_STORE_BUFFER_TOO_SMALL;
              return store->last_result;
            }

          if (slots[selected].payload_size != 0)
            {
              memcpy(output, payloads[selected], slots[selected].payload_size);
            }
          *output_size = slots[selected].payload_size;
          success_result = slot > 0 ||
                           slot_has_problem(store->slot_states[1u - selected]) ?
                           SMART_BAND_STORE_RECOVERED : SMART_BAND_STORE_OK;
        }
      else if (spec->migrate != NULL)
        {
          *output_size = 0;
          migration_result = spec->migrate(
            spec->migrate_context, slots[selected].schema_major,
            slots[selected].schema_minor, payloads[selected],
            slots[selected].payload_size, spec->schema_major,
            spec->schema_minor, output, output_capacity, output_size);
          if (migration_result == SMART_BAND_STORE_MIGRATION_OK)
            {
              if (*output_size > output_capacity ||
                  *output_size > SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE)
                {
                  store->last_result = SMART_BAND_STORE_INVALID;
                  return store->last_result;
                }

              success_result = SMART_BAND_STORE_MIGRATED;
            }
          else if (migration_result ==
                   SMART_BAND_STORE_MIGRATION_BUFFER_TOO_SMALL)
            {
              store->last_result = SMART_BAND_STORE_BUFFER_TOO_SMALL;
              return store->last_result;
            }
          else if (migration_result == SMART_BAND_STORE_MIGRATION_INVALID)
            {
              store->last_result = SMART_BAND_STORE_INVALID;
              return store->last_result;
            }
          else
            {
              store->slot_states[selected] =
                SMART_BAND_STORE_SLOT_UNSUPPORTED;
              continue;
            }
        }
      else
        {
          store->slot_states[selected] =
            SMART_BAND_STORE_SLOT_UNSUPPORTED;
          continue;
        }

      store->generation = slots[selected].generation;
      store->selected_slot = (uint8_t)selected;
      store->last_result = success_result;
      if (generation != NULL)
        {
          *generation = store->generation;
        }

      return success_result;
    }

  return copy_defaults(store, spec, output, output_capacity, output_size,
                       SMART_BAND_STORE_DEGRADED);
}

smart_band_store_result_t smart_band_store_commit(
  smart_band_store_t *store, const smart_band_store_record_spec_t *spec,
  const void *payload, size_t payload_size, uint64_t *generation)
{
  slot_record_t slots[SMART_BAND_STORE_SLOT_COUNT];
  smart_band_storage_record_t record;
  smart_band_storage_decode_options_t options;
  smart_band_storage_record_view_t view;
  smart_band_platform_result_t backend_result;
  bool have_valid_generation = false;
  uint64_t maximum_generation = 0;
  uint64_t next_generation = 1;
  size_t encoded_size = 0;
  size_t actual_size = 0;
  size_t target = 0;
  size_t slot;

  if (generation != NULL)
    {
      *generation = 0;
    }
  if (store == NULL || !store->initialized || !spec_is_valid(spec) ||
      (payload == NULL && payload_size != 0) ||
      payload_size > SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE)
    {
      return SMART_BAND_STORE_INVALID;
    }

  reset_observation(store);
  for (slot = 0; slot < SMART_BAND_STORE_SLOT_COUNT; slot++)
    {
      read_slot(store, spec, slot, &slots[slot], NULL);
      if (slots[slot].state == SMART_BAND_STORE_SLOT_UNAVAILABLE ||
          slots[slot].state == SMART_BAND_STORE_SLOT_IO_ERROR)
        {
          store->last_result =
            slots[slot].state == SMART_BAND_STORE_SLOT_UNAVAILABLE ?
            SMART_BAND_STORE_UNAVAILABLE : SMART_BAND_STORE_BACKEND_ERROR;
          return store->last_result;
        }

      if (slots[slot].state == SMART_BAND_STORE_SLOT_VALID &&
          (!have_valid_generation ||
           slots[slot].generation > maximum_generation))
        {
          have_valid_generation = true;
          maximum_generation = slots[slot].generation;
          target = 1u - slot;
        }
    }

  if (have_valid_generation)
    {
      if (maximum_generation == UINT64_MAX)
        {
          store->last_result = SMART_BAND_STORE_GENERATION_EXHAUSTED;
          return store->last_result;
        }

      next_generation = maximum_generation + 1u;
    }

  memset(&record, 0, sizeof(record));
  record.record_type = spec->record_type;
  record.schema_major = spec->schema_major;
  record.schema_minor = spec->schema_minor;
  record.generation = next_generation;
  record.payload = payload;
  record.payload_length = payload_size;
  if (smart_band_storage_codec_encode(&record, store->frame,
                                      sizeof(store->frame), &encoded_size) !=
      SMART_BAND_STORAGE_CODEC_OK)
    {
      store->last_result = SMART_BAND_STORE_INVALID;
      return store->last_result;
    }

  backend_result = store->backend.ops->write(
    store->backend.context, spec->slot_object_ids[target], store->frame,
    encoded_size);
  if (backend_result == SMART_BAND_PLATFORM_OK)
    {
      backend_result = store->backend.ops->flush(store->backend.context);
    }
  if (backend_result != SMART_BAND_PLATFORM_OK)
    {
      store->last_backend_result = backend_result;
      store->last_result = backend_result == SMART_BAND_PLATFORM_UNAVAILABLE ?
                           SMART_BAND_STORE_UNAVAILABLE :
                           SMART_BAND_STORE_BACKEND_ERROR;
      return store->last_result;
    }

  backend_result = store->backend.ops->read(
    store->backend.context, spec->slot_object_ids[target],
    store->frame, sizeof(store->frame), &actual_size);
  if (backend_result != SMART_BAND_PLATFORM_OK)
    {
      store->last_backend_result = backend_result;
      store->last_result = SMART_BAND_STORE_VERIFY_FAILED;
      return store->last_result;
    }

  memset(&options, 0, sizeof(options));
  options.expected_record_type = spec->record_type;
  options.accepted_schema_major = spec->schema_major;
  options.maximum_schema_minor = spec->schema_minor;
  if (smart_band_storage_codec_decode(store->frame, actual_size,
                                      &options, &view) !=
        SMART_BAND_STORAGE_CODEC_OK ||
      view.schema_minor != spec->schema_minor ||
      view.generation != next_generation ||
      view.payload_length != payload_size ||
      (payload_size != 0 && memcmp(view.payload, payload, payload_size) != 0))
    {
      store->last_result = SMART_BAND_STORE_VERIFY_FAILED;
      return store->last_result;
    }

  store->slot_states[target] = SMART_BAND_STORE_SLOT_VALID;
  store->generation = next_generation;
  store->selected_slot = (uint8_t)target;
  store->last_result = SMART_BAND_STORE_OK;
  if (generation != NULL)
    {
      *generation = next_generation;
    }

  return SMART_BAND_STORE_OK;
}
