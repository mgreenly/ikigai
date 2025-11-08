#ifndef IK_ARRAY_H
#define IK_ARRAY_H

#include <inttypes.h>
#include <talloc.h>
#include "error.h"

typedef struct ik_array {
    void *data;
    size_t element_size;
    size_t size;
    size_t capacity;
    size_t initial_capacity;
} ik_array_t;

ik_res_t ik_array_create(TALLOC_CTX *ctx, size_t element_size, size_t initial_capacity);
size_t ik_array_size(const ik_array_t *array);
size_t ik_array_capacity(const ik_array_t *array);

#endif // IK_ARRAY_H
