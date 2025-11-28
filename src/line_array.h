// Typed wrapper for line (char*) arrays
// Provides type-safe interface over generic ik_array_t

#ifndef IK_LINE_ARRAY_H
#define IK_LINE_ARRAY_H

#include <talloc.h>
#include "array.h"
#include "error.h"

// Line array type - wrapper around generic array
typedef ik_array_t ik_line_array_t;

// Lifecycle operations (IO - Returns res_t)
res_t ik_line_array_create(TALLOC_CTX *ctx, size_t increment);

// Modification operations (IO - Returns res_t)
res_t ik_line_array_append(ik_line_array_t *array, char *line);
res_t ik_line_array_insert(ik_line_array_t *array, size_t index, char *line);

// Modification operations (No IO - Direct return with asserts)
void ik_line_array_delete(ik_line_array_t *array, size_t index);
void ik_line_array_set(ik_line_array_t *array, size_t index, char *line);
void ik_line_array_clear(ik_line_array_t *array);

// Access operations (No IO - Direct return with asserts)
char *ik_line_array_get(const ik_line_array_t *array, size_t index);

// Query operations (Pure - Direct return with asserts)
size_t ik_line_array_size(const ik_line_array_t *array);
size_t ik_line_array_capacity(const ik_line_array_t *array);

#endif // IK_LINE_ARRAY_H
