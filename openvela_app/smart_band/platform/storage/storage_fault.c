#include "smart_band_storage_backend.h"

#include "storage_fault_internal.h"

#include <string.h>

void smart_band_storage_fault_reset(smart_band_storage_fault_plan_t *plan)
{
  if (plan != NULL)
    {
      memset(plan, 0, sizeof(*plan));
    }
}

smart_band_platform_result_t smart_band_storage_fault_arm(
  smart_band_storage_fault_plan_t *plan,
  smart_band_storage_operation_t operation,
  smart_band_storage_fault_kind_t kind, uint32_t trigger_on_call,
  size_t byte_index, uint8_t xor_mask)
{
  bool write_only;
  bool data_fault;

  if (plan == NULL || operation > SMART_BAND_STORAGE_OPERATION_FLUSH ||
      kind == SMART_BAND_STORAGE_FAULT_NONE ||
      kind > SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION ||
      trigger_on_call == 0)
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  write_only = kind == SMART_BAND_STORAGE_FAULT_SHORT_WRITE ||
               kind == SMART_BAND_STORAGE_FAULT_WRITE_INTERRUPTION;
  data_fault = kind == SMART_BAND_STORAGE_FAULT_TRUNCATE ||
               kind == SMART_BAND_STORAGE_FAULT_CORRUPT;
  if ((write_only && operation != SMART_BAND_STORAGE_OPERATION_WRITE) ||
      (data_fault && operation == SMART_BAND_STORAGE_OPERATION_FLUSH))
    {
      return SMART_BAND_PLATFORM_INVALID;
    }

  memset(plan, 0, sizeof(*plan));
  plan->armed = true;
  plan->operation = operation;
  plan->kind = kind;
  plan->trigger_on_call = trigger_on_call;
  plan->byte_index = byte_index;
  plan->xor_mask = xor_mask == 0 ? 1 : xor_mask;
  return SMART_BAND_PLATFORM_OK;
}

void smart_band_storage_fault_snapshot(
  const smart_band_storage_fault_plan_t *plan,
  smart_band_storage_fault_state_t *state)
{
  if (state == NULL)
    {
      return;
    }

  if (plan == NULL)
    {
      memset(state, 0, sizeof(*state));
      return;
    }

  *state = *plan;
}

bool smart_band_storage_fault_take(smart_band_storage_fault_plan_t *plan,
                                   smart_band_storage_operation_t operation,
                                   smart_band_storage_fault_kind_t *kind,
                                   size_t *byte_index, uint8_t *xor_mask)
{
  if (plan == NULL || !plan->armed || plan->operation != operation)
    {
      return false;
    }

  plan->matching_calls++;
  if (plan->matching_calls != plan->trigger_on_call)
    {
      return false;
    }

  plan->armed = false;
  plan->trigger_count++;
  plan->last_triggered = plan->kind;
  if (kind != NULL)
    {
      *kind = plan->kind;
    }

  if (byte_index != NULL)
    {
      *byte_index = plan->byte_index;
    }

  if (xor_mask != NULL)
    {
      *xor_mask = plan->xor_mask;
    }

  return true;
}
