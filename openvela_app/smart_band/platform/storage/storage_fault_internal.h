#ifndef SMART_BAND_STORAGE_FAULT_INTERNAL_H
#define SMART_BAND_STORAGE_FAULT_INTERNAL_H

#include "smart_band_storage_backend.h"

bool smart_band_storage_fault_take(smart_band_storage_fault_plan_t *plan,
                                   smart_band_storage_operation_t operation,
                                   smart_band_storage_fault_kind_t *kind,
                                   size_t *byte_index, uint8_t *xor_mask);

#endif
