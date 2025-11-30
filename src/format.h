#ifndef IK_FORMAT_H
#define IK_FORMAT_H

#include <inttypes.h>

#include "byte_array.h"
#include "error.h"
#include "tool.h"

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

// Format a tool call for display in scrollback
//
// Takes a tool call structure and returns a formatted string suitable for display.
// Format: [tool] tool_name(param1="value1", param2="value2", ...)
//
// @param parent Talloc parent context for result allocation
// @param call Tool call structure (cannot be NULL)
// @return Formatted string (owned by parent), never NULL
const char *ik_format_tool_call(void *parent, const ik_tool_call_t *call);

// Format a tool result for display in scrollback
//
// Takes a tool name and JSON result string and returns a formatted string.
// Parses the JSON and displays the result in a readable format.
// Format: [result] count items/lines of formatted output
//
// @param parent Talloc parent context for result allocation
// @param tool_name Tool name (e.g., "glob")
// @param result_json JSON result string (can be NULL)
// @return Formatted string (owned by parent), never NULL
const char *ik_format_tool_result(void *parent, const char *tool_name, const char *result_json);

#endif // IK_FORMAT_H
