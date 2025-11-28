#ifndef IK_FORMAT_H
#define IK_FORMAT_H

#include <inttypes.h>

#include "byte_array.h"
#include "error.h"

/**
 * Format buffer for building output strings.
 *
 * Thread-safety: Each thread should create its own buffer.
 * Buffers are NOT thread-safe for concurrent access.
 */
typedef struct ik_format_buffer_t {
    ik_byte_array_t *array;    // Underlying byte array
    void *parent;              // Talloc parent
} ik_format_buffer_t;

// Create format buffer
ik_format_buffer_t *ik_format_buffer_create(void *parent);

// Append formatted string (like sprintf)
res_t ik_format_appendf(ik_format_buffer_t *buf, const char *fmt, ...);

// Append raw string
res_t ik_format_append(ik_format_buffer_t *buf, const char *str);

// Append indent spaces
res_t ik_format_indent(ik_format_buffer_t *buf, int32_t indent);

// Get final string (null-terminated)
const char *ik_format_get_string(ik_format_buffer_t *buf);

// Get length in bytes (excluding null terminator)
size_t ik_format_get_length(ik_format_buffer_t *buf);

#endif // IK_FORMAT_H
