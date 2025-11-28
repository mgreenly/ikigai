// Generic expandable array implementation

#include "array.h"
#include "panic.h"
#include "wrapper.h"
#include <assert.h>
#include <string.h>

// Create new array with specified element size and increment
res_t ik_array_create(TALLOC_CTX *ctx, size_t element_size, size_t increment)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    // Validate arguments
    if (element_size == 0) {
        return ERR(ctx, INVALID_ARG, "element_size must be > 0");
    }
    if (increment == 0) {
        return ERR(ctx, INVALID_ARG, "increment must be > 0");
    }

    // Allocate array structure
    ik_array_t *array = talloc_zero_(ctx, sizeof(ik_array_t));
    if (array == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Initialize fields
    array->data = NULL;              // Lazy allocation - defer until first append/insert
    array->element_size = element_size;
    array->size = 0;
    array->capacity = 0;
    array->increment = increment;

    return OK(array);
}

// Grow array capacity (first allocation or doubling)
// Note: Two theoretical integer overflow scenarios exist but are not tested:
// 1. Capacity doubling: array->capacity * 2 could overflow if capacity > SIZE_MAX/2
//    (~9 exabytes on 64-bit systems)
// 2. Buffer allocation: element_size * new_capacity could overflow
// Both scenarios would require gigabytes of allocated memory. In practice, the
// system OOMs and talloc allocation fails long before overflow can occur. These
// edge cases are not tested as they cannot be reproduced without exabytes of RAM.
static res_t grow_array(ik_array_t *array)
{
    assert(array != NULL); // LCOV_EXCL_BR_LINE

    TALLOC_CTX *ctx = talloc_parent(array);
    size_t new_capacity;

    if (array->capacity == 0) {
        // First allocation - use increment
        new_capacity = array->increment;
    } else {
        // Subsequent growth - double capacity
        new_capacity = array->capacity * 2;
    }

    // Reallocate data buffer
    void *new_data = talloc_realloc_(ctx, array->data, array->element_size * new_capacity);
    if (new_data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    array->data = new_data;
    array->capacity = new_capacity;

    return OK(NULL);
}

// Append element to end of array
res_t ik_array_append(ik_array_t *array, const void *element)
{
    assert(array != NULL); // LCOV_EXCL_BR_LINE
    assert(element != NULL); // LCOV_EXCL_BR_LINE

    // Grow if needed
    if (array->size >= array->capacity) {
        res_t res = grow_array(array);
        if (is_err(&res)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    }

    // Copy element to end
    void *dest = (char *)array->data + (array->size * array->element_size);
    memcpy(dest, element, array->element_size);
    array->size++;

    return OK(NULL);
}

// Insert element at specified index
res_t ik_array_insert(ik_array_t *array, size_t index, const void *element)
{
    assert(array != NULL); // LCOV_EXCL_BR_LINE
    assert(element != NULL); // LCOV_EXCL_BR_LINE
    assert(index <= array->size); // LCOV_EXCL_BR_LINE - Can insert at end

    // Grow if needed
    if (array->size >= array->capacity) {
        res_t res = grow_array(array);
        if (is_err(&res)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    }

    // Shift elements right to make room
    if (index < array->size) {
        void *src = (char *)array->data + (index * array->element_size);
        void *dest = (char *)array->data + ((index + 1) * array->element_size);
        size_t bytes_to_move = (array->size - index) * array->element_size;
        memmove(dest, src, bytes_to_move);
    }

    // Copy element to index position
    void *dest = (char *)array->data + (index * array->element_size);
    memcpy(dest, element, array->element_size);
    array->size++;

    return OK(NULL);
}

// Delete element at index
void ik_array_delete(ik_array_t *array, size_t index)
{
    assert(array != NULL); // LCOV_EXCL_BR_LINE
    assert(index < array->size); // LCOV_EXCL_BR_LINE

    // Shift elements left to fill gap
    if (index < array->size - 1) {
        void *dest = (char *)array->data + (index * array->element_size);
        void *src = (char *)array->data + ((index + 1) * array->element_size);
        size_t bytes_to_move = (array->size - index - 1) * array->element_size;
        memmove(dest, src, bytes_to_move);
    }

    array->size--;
}

// Set element at index
void ik_array_set(ik_array_t *array, size_t index, const void *element)
{
    assert(array != NULL); // LCOV_EXCL_BR_LINE
    assert(element != NULL); // LCOV_EXCL_BR_LINE
    assert(index < array->size); // LCOV_EXCL_BR_LINE

    void *dest = (char *)array->data + (index * array->element_size);
    memcpy(dest, element, array->element_size);
}

// Clear all elements
void ik_array_clear(ik_array_t *array)
{
    assert(array != NULL); // LCOV_EXCL_BR_LINE
    array->size = 0;
}

// Get pointer to element at index
void *ik_array_get(const ik_array_t *array, size_t index)
{
    assert(array != NULL); // LCOV_EXCL_BR_LINE
    assert(index < array->size); // LCOV_EXCL_BR_LINE

    return (char *)array->data + (index * array->element_size);
}

// Get current number of elements
size_t ik_array_size(const ik_array_t *array)
{
    assert(array != NULL); // LCOV_EXCL_BR_LINE
    return array->size;
}

// Get allocated capacity
size_t ik_array_capacity(const ik_array_t *array)
{
    assert(array != NULL); // LCOV_EXCL_BR_LINE
    return array->capacity;
}
