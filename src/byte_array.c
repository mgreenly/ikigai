// Typed wrapper for byte (uint8_t) arrays

#include "byte_array.h"

// Create new byte array
res_t ik_byte_array_create(TALLOC_CTX *ctx, size_t increment)
{
    return ik_array_create(ctx, sizeof(uint8_t), increment);
}

// Append byte to end of array
res_t ik_byte_array_append(ik_byte_array_t *array, uint8_t byte)
{
    return ik_array_append(array, &byte);
}

// Insert byte at specified index
res_t ik_byte_array_insert(ik_byte_array_t *array, size_t index, uint8_t byte)
{
    return ik_array_insert(array, index, &byte);
}

// Delete byte at index
void ik_byte_array_delete(ik_byte_array_t *array, size_t index)
{
    ik_array_delete(array, index);
}

// Set byte at index
void ik_byte_array_set(ik_byte_array_t *array, size_t index, uint8_t byte)
{
    ik_array_set(array, index, &byte);
}

// Clear all bytes
void ik_byte_array_clear(ik_byte_array_t *array)
{
    ik_array_clear(array);
}

// Get byte at index
uint8_t ik_byte_array_get(const ik_byte_array_t *array, size_t index)
{
    return *(uint8_t *)ik_array_get(array, index);
}

// Get current number of bytes
size_t ik_byte_array_size(const ik_byte_array_t *array)
{
    return ik_array_size(array);
}
