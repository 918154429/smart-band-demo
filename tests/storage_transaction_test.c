#include "smart_band_storage_backend.h"
#include "smart_band_storage_transaction.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
  do                                                                         \
    {                                                                        \
      if (!(condition))                                                      \
        {                                                                    \
          fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
                  #condition);                                               \
          return 1;                                                          \
        }                                                                    \
    }                                                                        \
  while (0)

static const smart_band_store_record_spec_t g_targets[] =
{
  {{UINT32_C(0x00060000), UINT32_C(0x00060001)},
    SMART_BAND_STORAGE_RECORD_DAILY_HISTORY, 1, 0,
    NULL, 0, NULL, NULL},
  {{UINT32_C(0x00060002), UINT32_C(0x00060003)},
    SMART_BAND_STORAGE_RECORD_RUNTIME_CHECKPOINT, 1, 0,
    NULL, 0, NULL, NULL}
};

static const uint8_t g_old_daily[] = {0x10, 0x11, 0x12};
static const uint8_t g_old_checkpoint[] = {0x20, 0x21};
static const uint8_t g_new_daily[] = {0x30, 0x31, 0x32, 0x33};
static const uint8_t g_new_checkpoint[] = {0x40, 0x41, 0x42};

static int init_stack(smart_band_storage_memory_t *memory,
                      smart_band_storage_t *storage,
                      smart_band_store_t *store)
{
  return smart_band_storage_memory_init(memory, storage) ==
           SMART_BAND_PLATFORM_OK &&
         smart_band_store_init(store, storage) == 0 ? 0 : -1;
}

static int seed_old(smart_band_store_t *store)
{
  return smart_band_store_commit(store, &g_targets[0], g_old_daily,
                                 sizeof(g_old_daily), NULL) ==
           SMART_BAND_STORE_OK &&
         smart_band_store_commit(store, &g_targets[1], g_old_checkpoint,
                                 sizeof(g_old_checkpoint), NULL) ==
           SMART_BAND_STORE_OK ? 0 : -1;
}

static void make_participants(
  smart_band_storage_transaction_participant_t participants[2])
{
  memset(participants, 0, 2u * sizeof(participants[0]));
  participants[0].target = g_targets[0];
  participants[0].payload = g_new_daily;
  participants[0].payload_size = sizeof(g_new_daily);
  participants[1].target = g_targets[1];
  participants[1].payload = g_new_checkpoint;
  participants[1].payload_size = sizeof(g_new_checkpoint);
}

static int payload_is(smart_band_store_t *store,
                      const smart_band_store_record_spec_t *spec,
                      const uint8_t *expected, size_t expected_size)
{
  uint8_t payload[32];
  size_t payload_size = 0u;
  smart_band_store_result_t result = smart_band_store_load(
    store, spec, payload, sizeof(payload), &payload_size, NULL);

  return result >= SMART_BAND_STORE_OK &&
         result != SMART_BAND_STORE_DEGRADED &&
         payload_size == expected_size &&
         memcmp(payload, expected, expected_size) == 0;
}

static int verify_coherent_after_recovery(
  smart_band_storage_memory_t *memory, smart_band_storage_t *storage)
{
  smart_band_store_t reopened;
  bool old_state;
  bool new_state;

  smart_band_storage_fault_reset(&memory->fault);
  CHECK(smart_band_store_init(&reopened, storage) == 0);
  CHECK(smart_band_storage_transaction_recover(&reopened) ==
        SMART_BAND_STORE_OK);
  old_state = payload_is(&reopened, &g_targets[0], g_old_daily,
                         sizeof(g_old_daily)) &&
              payload_is(&reopened, &g_targets[1], g_old_checkpoint,
                         sizeof(g_old_checkpoint));
  new_state = payload_is(&reopened, &g_targets[0], g_new_daily,
                         sizeof(g_new_daily)) &&
              payload_is(&reopened, &g_targets[1], g_new_checkpoint,
                         sizeof(g_new_checkpoint));
  CHECK(old_state || new_state);
  smart_band_store_deinit(&reopened);
  return 0;
}

static uint32_t operation_call_count(smart_band_storage_operation_t operation)
{
  smart_band_storage_memory_t memory;
  smart_band_storage_t storage;
  smart_band_store_t store;
  smart_band_storage_transaction_participant_t participants[2];
  smart_band_storage_fault_state_t state;

  if (init_stack(&memory, &storage, &store) != 0 || seed_old(&store) != 0 ||
      smart_band_storage_fault_arm(
        &memory.fault, operation, SMART_BAND_STORAGE_FAULT_IO, UINT32_MAX,
        0u, 0u) != SMART_BAND_PLATFORM_OK)
    {
      return 0u;
    }
  make_participants(participants);
  if (smart_band_storage_transaction_commit(&store, participants, 2u) !=
      SMART_BAND_STORE_OK)
    {
      return 0u;
    }
  smart_band_storage_fault_snapshot(&memory.fault, &state);
  return state.matching_calls;
}

static int test_every_operation_cut_is_coherent(void)
{
  smart_band_storage_operation_t operations[] =
  {
    SMART_BAND_STORAGE_OPERATION_READ,
    SMART_BAND_STORAGE_OPERATION_WRITE,
    SMART_BAND_STORAGE_OPERATION_FLUSH
  };
  size_t operation_index;

  for (operation_index = 0;
       operation_index < sizeof(operations) / sizeof(operations[0]);
       operation_index++)
    {
      smart_band_storage_operation_t operation = operations[operation_index];
      uint32_t calls = operation_call_count(operation);
      uint32_t trigger;

      CHECK(calls != 0u);
      for (trigger = 1u; trigger <= calls; trigger++)
        {
          smart_band_storage_memory_t memory;
          smart_band_storage_t storage;
          smart_band_store_t store;
          smart_band_storage_transaction_participant_t participants[2];
          smart_band_storage_fault_kind_t kind =
            operation == SMART_BAND_STORAGE_OPERATION_WRITE ?
            SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION :
            SMART_BAND_STORAGE_FAULT_IO;

          CHECK(init_stack(&memory, &storage, &store) == 0);
          CHECK(seed_old(&store) == 0);
          CHECK(smart_band_storage_fault_arm(
                  &memory.fault, operation, kind, trigger, 17u,
                  UINT8_C(0x01)) == SMART_BAND_PLATFORM_OK);
          make_participants(participants);
          (void)smart_band_storage_transaction_commit(&store, participants,
                                                       2u);
          smart_band_store_deinit(&store);
          CHECK(verify_coherent_after_recovery(&memory, &storage) == 0);
        }
    }

  return 0;
}

static int test_redundant_manifest_and_stage_survive_one_bad_slot(void)
{
  uint32_t corrupt_read;

  for (corrupt_read = 1u; corrupt_read <= 2u; corrupt_read++)
    {
      smart_band_storage_memory_t memory;
      smart_band_storage_t storage;
      smart_band_store_t store;
      smart_band_storage_transaction_participant_t participants[2];

      CHECK(init_stack(&memory, &storage, &store) == 0);
      CHECK(seed_old(&store) == 0);
      make_participants(participants);
      CHECK(smart_band_storage_transaction_commit(&store, participants, 2u) ==
            SMART_BAND_STORE_OK);
      smart_band_store_deinit(&store);
      CHECK(smart_band_store_init(&store, &storage) == 0);
      CHECK(smart_band_storage_fault_arm(
              &memory.fault, SMART_BAND_STORAGE_OPERATION_READ,
              SMART_BAND_STORAGE_FAULT_CORRUPT, corrupt_read, 8u,
              UINT8_C(0x80)) == SMART_BAND_PLATFORM_OK);
      CHECK(smart_band_storage_transaction_recover(&store) ==
            SMART_BAND_STORE_OK);
      CHECK(payload_is(&store, &g_targets[0], g_new_daily,
                       sizeof(g_new_daily)));
      CHECK(payload_is(&store, &g_targets[1], g_new_checkpoint,
                       sizeof(g_new_checkpoint)));
      smart_band_store_deinit(&store);
    }

  for (corrupt_read = 3u; corrupt_read <= 4u; corrupt_read++)
    {
      smart_band_storage_memory_t memory;
      smart_band_storage_t storage;
      smart_band_store_t store;
      smart_band_storage_transaction_participant_t participants[2];

      CHECK(init_stack(&memory, &storage, &store) == 0);
      CHECK(seed_old(&store) == 0);
      CHECK(smart_band_storage_fault_arm(
              &memory.fault, SMART_BAND_STORAGE_OPERATION_WRITE,
              SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION, 7u, 17u,
              UINT8_C(0x01)) == SMART_BAND_PLATFORM_OK);
      make_participants(participants);
      CHECK(smart_band_storage_transaction_commit(&store, participants, 2u) !=
            SMART_BAND_STORE_OK);
      smart_band_storage_fault_reset(&memory.fault);
      smart_band_store_deinit(&store);
      CHECK(smart_band_store_init(&store, &storage) == 0);
      CHECK(smart_band_storage_fault_arm(
              &memory.fault, SMART_BAND_STORAGE_OPERATION_READ,
              SMART_BAND_STORAGE_FAULT_CORRUPT, corrupt_read, 8u,
              UINT8_C(0x80)) == SMART_BAND_PLATFORM_OK);
      CHECK(smart_band_storage_transaction_recover(&store) ==
            SMART_BAND_STORE_OK);
      CHECK(payload_is(&store, &g_targets[0], g_new_daily,
                       sizeof(g_new_daily)));
      CHECK(payload_is(&store, &g_targets[1], g_new_checkpoint,
                       sizeof(g_new_checkpoint)));
      smart_band_store_deinit(&store);
    }

  return 0;
}

int main(void)
{
  CHECK(test_every_operation_cut_is_coherent() == 0);
  CHECK(test_redundant_manifest_and_stage_survive_one_bad_slot() == 0);
  puts("smart band storage transaction tests passed");
  return 0;
}
