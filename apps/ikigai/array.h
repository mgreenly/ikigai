// Generic expandable array implementation
// Provides type-safe foundation for dynamic collections

#ifndef IK_ARRAY_H
#define IK_ARRAY_H

#include <stddef.h>
#include <talloc.h>
#include "shared/error.h"

// Generic expandable array
// Uses configurable element_size to support any type
// Memory managed via talloc
typedef struct ik_array {
    void *data;              // Allocated storage (element_size * capacity bytes), NULL until first use
    size_t element_size;     // Size of each element in bytes (must be > 0)
    size_t size;             // Current number of elements (starts at 0)
    size_t capacity;         // Current allocated capacity in elements (starts at 0)
    size_t increment;        // Size for first allocation (then double)
} ik_array_t;

// Lifecycle operations (IO - Returns res_t)
res_t ik_array_create(TALLOC_CTX *ctx, size_t element_size, size_t increment);

// Modification operations (IO - Returns res_t)
res_t ik_array_append(ik_array_t *array, const void *element);
res_t ik_array_insert(ik_array_t *array, size_t index, const void *element);

// Modification operations (No IO - Direct return with asserts)
void ik_array_delete(ik_array_t *array, size_t index);
void ik_array_clear(ik_array_t *array);

// Access operations (No IO - Direct return with asserts)
void *ik_array_get(const ik_array_t *array, size_t index);

// Query operations (Pure - Direct return with asserts)
size_t ik_array_size(const ik_array_t *array);
size_t ik_array_capacity(const ik_array_t *array);

#endif // IK_ARRAY_H
