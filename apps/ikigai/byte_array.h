// Typed wrapper for byte (uint8_t) arrays
// Provides type-safe interface over generic ik_array_t

#ifndef IK_BYTE_ARRAY_H
#define IK_BYTE_ARRAY_H

#include <inttypes.h>
#include <talloc.h>
#include "apps/ikigai/array.h"
#include "shared/error.h"

// Byte array type - wrapper around generic array
typedef ik_array_t ik_byte_array_t;

// Lifecycle operations (IO - Returns res_t)
res_t ik_byte_array_create(TALLOC_CTX *ctx, size_t increment);

// Modification operations (IO - Returns res_t)
res_t ik_byte_array_append(ik_byte_array_t *array, uint8_t byte);
res_t ik_byte_array_insert(ik_byte_array_t *array, size_t index, uint8_t byte);

// Modification operations (No IO - Direct return with asserts)
void ik_byte_array_delete(ik_byte_array_t *array, size_t index);
void ik_byte_array_clear(ik_byte_array_t *array);

// Access operations (No IO - Direct return with asserts)
uint8_t ik_byte_array_get(const ik_byte_array_t *array, size_t index);

// Query operations (Pure - Direct return with asserts)
size_t ik_byte_array_size(const ik_byte_array_t *array);

#endif // IK_BYTE_ARRAY_H
