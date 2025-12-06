#ifndef IK_FORMAT_H
#define IK_FORMAT_H

#include <inttypes.h>

#include "byte_array.h"
#include "error.h"
#include "tool.h"
#include "vendor/yyjson/yyjson.h"

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
// Format: → tool_name: key1="value1", key2=value2, ...
//
// Arguments are parsed from JSON and formatted as key=value pairs.
// String values are quoted, other types (int, real, bool, null) are unquoted.
// If arguments are NULL, empty, or invalid JSON, shows just the tool name or raw args.
//
// @param parent Talloc parent context for result allocation
// @param call Tool call structure (cannot be NULL)
// @return Formatted string (owned by parent), never NULL
const char *ik_format_tool_call(void *parent, const ik_tool_call_t *call);

// Format a tool result for display in scrollback
//
// Takes a tool name and JSON result string and returns a formatted string.
// Parses the JSON and displays meaningful content in a readable format.
// Format: ← tool_name: <truncated content>
// - Truncates at 3 lines or 400 characters (whichever first)
// - Adds "..." when truncated
// - Arrays joined with ", ", strings used directly, objects as JSON
//
// @param parent Talloc parent context for result allocation
// @param tool_name Tool name (e.g., "glob")
// @param result_json JSON result string (can be NULL)
// @return Formatted string (owned by parent), never NULL
const char *ik_format_tool_result(void *parent, const char *tool_name, const char *result_json);

// Helper functions (non-static to ensure LCOV markers work properly)
// See style.md "Avoid Static Functions" rule

// Truncate content to 3 lines or 400 chars and append to buffer
void ik_format_truncate_and_append(ik_format_buffer_t *buf, const char *content, size_t content_len);

// Extract and join array elements with ", "
const char *ik_format_extract_array_content(void *parent, yyjson_val *root, size_t *out_len);

// Wrappers for yyjson inline functions to avoid coverage gaps from inline expansion
// These wrap vendor inline functions so they expand only once, in a testable location
void ik_format_yyjson_obj_iter_init_wrapper(yyjson_val *obj, yyjson_obj_iter *iter);
yyjson_val *ik_format_yyjson_obj_iter_next_wrapper(yyjson_obj_iter *iter);
yyjson_val *ik_format_yyjson_obj_iter_get_val_wrapper(yyjson_val *key);
char *ik_format_yyjson_val_write_wrapper(yyjson_val *val) __attribute__((weak));

#endif // IK_FORMAT_H
