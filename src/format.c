#include "format.h"

#include "array.h"
#include "byte_array.h"
#include "error.h"
#include "json_allocator.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include "wrapper.h"

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

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
    int32_t needed = vsnprintf_(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0) {
        va_end(args);
        return ERR(buf->parent, IO, "vsnprintf size calculation failed");
    }

    // Allocate temporary buffer
    size_t buf_size = (size_t)needed + 1;  // +1 for null terminator
    char *temp = talloc_array_(buf->parent, sizeof(char), buf_size);
    if (temp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Second pass: format into buffer
    int32_t written = vsnprintf_(temp, buf_size, fmt, args);
    va_end(args);
    if (written < 0) {
        talloc_free(temp);
        return ERR(buf->parent, IO, "vsnprintf formatting failed");
    }
    if ((size_t)written >= buf_size) {
        talloc_free(temp);
        return ERR(buf->parent, IO, "vsnprintf truncated output");
    }

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

const char *ik_format_tool_call(void *parent, const ik_tool_call_t *call)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE
    assert(call != NULL);    // LCOV_EXCL_BR_LINE

    ik_format_buffer_t *buf = ik_format_buffer_create(parent);
    if (buf == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Start with arrow and tool name
    res_t res = ik_format_appendf(buf, "→ %s", call->name);
    if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE

    // Handle missing or empty arguments
    if (call->arguments == NULL || call->arguments[0] == '\0') {
        return ik_format_get_string(buf);
    }

    // Try to parse JSON arguments
    yyjson_alc allocator = ik_make_talloc_allocator(parent);
    yyjson_doc *doc = yyjson_read_opts(call->arguments, strlen(call->arguments), 0, &allocator, NULL);
    if (doc == NULL) {
        // Invalid JSON - show raw arguments as fallback
        res = ik_format_appendf(buf, ": %s", call->arguments);
        if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
        return ik_format_get_string(buf);
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (!yyjson_is_obj(root)) {
        // Not an object - show raw
        res = ik_format_appendf(buf, ": %s", call->arguments);
        if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
        return ik_format_get_string(buf);
    }

    // Check if object is empty
    size_t obj_size = yyjson_obj_size(root);
    if (obj_size == 0) {
        return ik_format_get_string(buf);
    }

    // Format key=value pairs
    res = ik_format_append(buf, ": ");
    if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE

    bool first = true;
    yyjson_obj_iter iter;
    ik_format_yyjson_obj_iter_init_wrapper(root, &iter);
    yyjson_val *key;
    while ((key = ik_format_yyjson_obj_iter_next_wrapper(&iter)) != NULL) {
        yyjson_val *val = ik_format_yyjson_obj_iter_get_val_wrapper(key);

        if (!first) {
            res = ik_format_append(buf, ", ");
            if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
        }
        first = false;

        const char *key_str = yyjson_get_str_(key);

        // Format value based on type
        if (yyjson_is_str(val)) {
            res = ik_format_appendf(buf, "%s=\"%s\"", key_str, yyjson_get_str_(val));
        } else if (yyjson_is_int(val)) {
            res = ik_format_appendf(buf, "%s=%" PRId64, key_str, yyjson_get_sint_(val));
        } else if (yyjson_is_real(val)) {
            res = ik_format_appendf(buf, "%s=%g", key_str, yyjson_get_real(val));
        } else if (yyjson_is_bool(val)) {
            res = ik_format_appendf(buf, "%s=%s", key_str, yyjson_get_bool(val) ? "true" : "false");
        } else if (yyjson_is_null(val)) {
            res = ik_format_appendf(buf, "%s=null", key_str);
        } else {
            // Arrays/objects - show as JSON
            char *val_str = ik_format_yyjson_val_write_wrapper(val);
            if (val_str != NULL) {
                res = ik_format_appendf(buf, "%s=%s", key_str, val_str);
                free(val_str);
            }
        }
        if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
    }

    return ik_format_get_string(buf);
}

// Non-static helper functions to ensure LCOV markers work properly
// See style.md "Avoid Static Functions" rule

// Helper to truncate content to 3 lines or 400 chars, whichever comes first
void ik_format_truncate_and_append(ik_format_buffer_t *buf, const char *content, size_t content_len)
{
    assert(buf != NULL);     // LCOV_EXCL_BR_LINE
    assert(content != NULL); // LCOV_EXCL_BR_LINE

    if (content_len == 0) {
        res_t res = ik_format_append(buf, "(no output)");
        if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
        return;
    }

    // Count lines and find truncation point
    size_t line_count = 1;
    size_t char_count = 0;
    size_t truncate_at = content_len;
    bool needs_truncation = false;

    for (size_t i = 0; i < content_len; i++) {
        char_count++;

        if (content[i] == '\n') {
            line_count++;
            if (line_count > 3) {
                truncate_at = i;
                needs_truncation = true;
                break;
            }
        }

        if (char_count >= 400) {
            truncate_at = i + 1;
            needs_truncation = true;
            break;
        }
    }

    // Append content with truncation if needed
    res_t res;
    if (needs_truncation) {
        res = ik_format_appendf(buf, "%.*s...", (int32_t)truncate_at, content);
    } else {
        res = ik_format_append(buf, content);
    }
    if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
}

// Helper to extract content from JSON array - joins elements with ", "
const char *ik_format_extract_array_content(void *parent, yyjson_val *root, size_t *out_len)
{
    assert(parent != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL);   // LCOV_EXCL_BR_LINE

    ik_format_buffer_t *arr_buf = ik_format_buffer_create(parent);
    if (arr_buf == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    size_t arr_size = yyjson_arr_size(root);

    for (size_t i = 0; i < arr_size; i++) {
        yyjson_val *elem = yyjson_arr_get(root, i);
        if (i > 0) {
            res_t r = ik_format_append(arr_buf, ", ");
            if (is_err(&r)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
        }
        if (yyjson_is_str(elem)) {
            res_t r = ik_format_append(arr_buf, yyjson_get_str_(elem));
            if (is_err(&r)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
        } else {
            char *elem_str = ik_format_yyjson_val_write_wrapper(elem);
            if (elem_str != NULL) {
                res_t r = ik_format_append(arr_buf, elem_str);
                if (is_err(&r)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
                free(elem_str);
            }
        }
    }

    *out_len = ik_format_get_length(arr_buf);
    return ik_format_get_string(arr_buf);
}

const char *ik_format_tool_result(void *parent, const char *tool_name, const char *result_json)
{
    assert(parent != NULL);    // LCOV_EXCL_BR_LINE
    assert(tool_name != NULL); // LCOV_EXCL_BR_LINE

    ik_format_buffer_t *buf = ik_format_buffer_create(parent);
    if (buf == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Start with arrow and tool name
    res_t res = ik_format_appendf(buf, "← %s: ", tool_name);
    if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE

    // Handle null result
    if (result_json == NULL) {
        res = ik_format_append(buf, "(no output)");
        if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
        return ik_format_get_string(buf);
    }

    // Try to parse JSON
    yyjson_alc allocator = ik_make_talloc_allocator(parent);
    // yyjson_read_opts wants non-const pointer but doesn't modify the data (same cast pattern as yyjson.h:993)
    yyjson_doc *doc = yyjson_read_opts((char *)(void *)(size_t)(const void *)result_json,
                                       strlen(result_json),
                                       0,
                                       &allocator,
                                       NULL);
    if (doc == NULL) {
        // Invalid JSON - show raw, truncated
        ik_format_truncate_and_append(buf, result_json, strlen(result_json));
        return ik_format_get_string(buf);
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    const char *content = NULL;
    size_t content_len = 0;

    if (yyjson_is_str(root)) {
        // String result - use directly
        const char *str = yyjson_get_str_(root);
        content_len = yyjson_get_len(root);

        // Check for empty string
        if (content_len == 0) {
            res = ik_format_append(buf, "(no output)");
            if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
            return ik_format_get_string(buf);
        }

        // Copy string before freeing doc
        char *content_owned = talloc_strndup(parent, str, content_len);
        if (content_owned == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        content = content_owned;
    } else if (yyjson_is_arr(root)) {
        // Array - join elements with ", "
        content = ik_format_extract_array_content(parent, root, &content_len);
    } else {
        // Object or other - serialize to JSON
        char *json_str = ik_format_yyjson_val_write_wrapper(root);
        if (json_str != NULL) {
            char *content_owned = talloc_strdup(parent, json_str);
            if (content_owned == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            content = content_owned;
            content_len = strlen(content);
            free(json_str);
        }
    }

    // Truncate and append content
    if (content != NULL) {
        ik_format_truncate_and_append(buf, content, content_len);
    } else {
        res = ik_format_append(buf, "(no output)");
        if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
    }

    return ik_format_get_string(buf);
}

// Wrappers for yyjson inline functions - expand once here for testability
// See mocking.md "Wrapping Vendor Inline Functions"

void ik_format_yyjson_obj_iter_init_wrapper(yyjson_val *obj, yyjson_obj_iter *iter)
{
    yyjson_obj_iter_init(obj, iter);
}

yyjson_val *ik_format_yyjson_obj_iter_next_wrapper(yyjson_obj_iter *iter)
{
    return yyjson_obj_iter_next(iter);
}

yyjson_val *ik_format_yyjson_obj_iter_get_val_wrapper(yyjson_val *key)
{
    return yyjson_obj_iter_get_val(key);
}

char *ik_format_yyjson_val_write_wrapper(yyjson_val *val)
{
    return yyjson_val_write(val, 0, NULL);
}
