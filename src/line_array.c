// Typed wrapper for line (char*) arrays

#include "line_array.h"


#include "poison.h"
// Create new line array
res_t ik_line_array_create(TALLOC_CTX *ctx, size_t increment)
{
    return ik_array_create(ctx, sizeof(char *), increment);
}

// Append line to end of array
res_t ik_line_array_append(ik_line_array_t *array, char *line)
{
    return ik_array_append(array, &line);
}

// Insert line at specified index
res_t ik_line_array_insert(ik_line_array_t *array, size_t index, char *line)
{
    return ik_array_insert(array, index, &line);
}

// Delete line at index
void ik_line_array_delete(ik_line_array_t *array, size_t index)
{
    ik_array_delete(array, index);
}

// Set line at index
void ik_line_array_set(ik_line_array_t *array, size_t index, char *line)
{
    ik_array_set(array, index, &line);
}

// Clear all lines
void ik_line_array_clear(ik_line_array_t *array)
{
    ik_array_clear(array);
}

// Get line at index
char *ik_line_array_get(const ik_line_array_t *array, size_t index)
{
    return *(char **)ik_array_get(array, index);
}

// Get current number of lines
size_t ik_line_array_size(const ik_line_array_t *array)
{
    return ik_array_size(array);
}

// Get allocated capacity
size_t ik_line_array_capacity(const ik_line_array_t *array)
{
    return ik_array_capacity(array);
}
