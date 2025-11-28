#include "format.h"

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

#include "array.h"
#include "byte_array.h"
#include "error.h"
#include "panic.h"
#include "wrapper.h"

ik_format_buffer_t *ik_format_buffer_create(void *parent)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE - assertion branches untestable

    ik_format_buffer_t *buf = talloc_zero_(parent, sizeof(ik_format_buffer_t));
    if (buf == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    buf->parent = parent;
    res_t res = ik_byte_array_create(buf, 32);  // Start with 32 byte increment
    if (is_err(&res)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    buf->array = res.ok;

    return buf;
}

res_t ik_format_appendf(ik_format_buffer_t *buf, const char *fmt, ...)
{
    assert(buf != NULL);  // LCOV_EXCL_BR_LINE - assertion branches untestable
    assert(fmt != NULL);  // LCOV_EXCL_BR_LINE - assertion branches untestable

    va_list args;
    va_start(args, fmt);

    // First pass: determine required size
    va_list args_copy;
    va_copy(args_copy, args);
    int32_t needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0) PANIC("vsnprintf size calculation failed");   // LCOV_EXCL_LINE

    // Allocate temporary buffer
    size_t buf_size = (size_t)needed + 1;  // +1 for null terminator
    char *temp = talloc_array_(buf->parent, sizeof(char), buf_size);
    if (temp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Second pass: format into buffer
    int32_t written = vsnprintf(temp, buf_size, fmt, args);
    va_end(args);
    if (written < 0) PANIC("vsnprintf formatting failed");   // LCOV_EXCL_LINE
    if ((size_t)written >= buf_size) PANIC("vsnprintf truncated");   // LCOV_EXCL_LINE

    // Append formatted string (excluding null terminator)
    for (int32_t i = 0; i < written; i++) {
        res_t res = ik_byte_array_append(buf->array, (uint8_t)temp[i]);
        if (is_err(&res)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    }

    talloc_free(temp);
    return OK(buf);
}

res_t ik_format_append(ik_format_buffer_t *buf, const char *str)
{
    assert(buf != NULL);  // LCOV_EXCL_BR_LINE - assertion branches untestable
    assert(str != NULL);  // LCOV_EXCL_BR_LINE - assertion branches untestable

    size_t len = strlen(str);
    if (len == 0) {
        return OK(buf);
    }

    for (size_t i = 0; i < len; i++) {
        res_t res = ik_byte_array_append(buf->array, (uint8_t)str[i]);
        if (is_err(&res)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    }

    return OK(buf);
}

res_t ik_format_indent(ik_format_buffer_t *buf, int32_t indent)
{
    assert(buf != NULL);  // LCOV_EXCL_BR_LINE - assertion branches untestable
    assert(indent >= 0);  // LCOV_EXCL_BR_LINE - assertion branches untestable

    if (indent == 0) {
        return OK(buf);
    }

    for (int32_t i = 0; i < indent; i++) {
        res_t res = ik_byte_array_append(buf->array, (uint8_t)' ');
        if (is_err(&res)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    }

    return OK(buf);
}

const char *ik_format_get_string(ik_format_buffer_t *buf)
{
    assert(buf != NULL);  // LCOV_EXCL_BR_LINE - assertion branches untestable

    ik_array_t *array = (ik_array_t *)buf->array;
    size_t len = ik_byte_array_size(buf->array);

    // Check if already null-terminated
    if (len > 0 && ik_byte_array_get(buf->array, len - 1) == '\0') {
        // Already null-terminated
        return (const char *)array->data;
    }

    // Need to add null terminator
    res_t res = ik_byte_array_append(buf->array, '\0');
    if (is_err(&res)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Return pointer to data buffer
    return (const char *)array->data;
}

size_t ik_format_get_length(ik_format_buffer_t *buf)
{
    assert(buf != NULL);  // LCOV_EXCL_BR_LINE - assertion branches untestable

    size_t len = ik_byte_array_size(buf->array);

    // Length excludes null terminator if present
    if (len > 0 && ik_byte_array_get(buf->array, len - 1) == '\0') {  // LCOV_EXCL_BR_LINE - short-circuit branch
        return len - 1;
    }
    return len;
}
