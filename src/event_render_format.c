/**
 * @file event_render_format.c
 * @brief Formatting helpers for event rendering
 */

#include "event_render_format.h"

#include "format.h"
#include "output_style.h"
#include "panic.h"
#include "tool.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>


#include "poison.h"
const char *ik_event_render_format_tool_call(TALLOC_CTX *ctx, const char *content, const char *data_json)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    // If content is already formatted (starts with prefix), use as-is
    const char *tool_req_prefix = ik_output_prefix(IK_OUTPUT_TOOL_REQUEST);
    if (content != NULL && strncmp(content, tool_req_prefix, strlen(tool_req_prefix)) == 0) {
        return content;
    }

    // Try to extract tool info from data_json
    if (data_json == NULL) {
        return content; // No data to format with, use content as-is
    }

    yyjson_doc *doc = yyjson_read_(data_json, strlen(data_json), 0);
    if (doc == NULL) {
        return content; // Invalid JSON, use content as-is
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    yyjson_val *name_val = yyjson_obj_get_(root, "tool_name");
    yyjson_val *args_val = yyjson_obj_get_(root, "tool_args");
    yyjson_val *id_val = yyjson_obj_get_(root, "tool_call_id");

    if (name_val == NULL || !yyjson_is_str(name_val) ||
        args_val == NULL || !yyjson_is_str(args_val) ||
        id_val == NULL || !yyjson_is_str(id_val)) {
        yyjson_doc_free(doc);
        return content; // Missing required fields
    }

    // Extract strings (lifetime tied to doc, so make copies)
    const char *id_str = yyjson_get_str_(id_val);
    const char *name_str = yyjson_get_str_(name_val);
    const char *args_str = yyjson_get_str_(args_val);

    char *id_copy = talloc_strdup(ctx, id_str);
    char *name_copy = talloc_strdup(ctx, name_str);
    char *args_copy = talloc_strdup(ctx, args_str);

    if (id_copy == NULL || name_copy == NULL || args_copy == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Build tool_call_t structure for formatting
    ik_tool_call_t tc;
    tc.id = id_copy;
    tc.name = name_copy;
    tc.arguments = args_copy;

    // Apply formatting (includes truncation)
    const char *formatted = ik_format_tool_call(ctx, &tc);

    yyjson_doc_free(doc);
    return formatted;
}

// Helper to format raw tool result content with truncation (no tool name available)
const char *ik_event_render_format_tool_result_raw(TALLOC_CTX *ctx, const char *content)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    ik_format_buffer_t *buf = ik_format_buffer_create(ctx);
    if (buf == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add prefix for tool response
    const char *prefix = ik_output_prefix(IK_OUTPUT_TOOL_RESPONSE);
    res_t res = ik_format_appendf(buf, "%s ", prefix);
    if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE

    // Truncate and append raw content
    if (content == NULL) {
        res = ik_format_append(buf, "(no output)");
        if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
    } else {
        ik_format_truncate_and_append(buf, content, strlen(content));
    }

    return ik_format_get_string(buf);
}

const char *ik_event_render_format_tool_result(TALLOC_CTX *ctx, const char *content, const char *data_json)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    // If content is already formatted (starts with prefix), use as-is
    const char *tool_resp_prefix = ik_output_prefix(IK_OUTPUT_TOOL_RESPONSE);
    if (content != NULL && strncmp(content, tool_resp_prefix, strlen(tool_resp_prefix)) == 0) {
        return content;
    }

    // Try to extract tool info from data_json
    if (data_json == NULL) {
        // No data to format with - apply truncation to raw content
        return ik_event_render_format_tool_result_raw(ctx, content);
    }

    yyjson_doc *doc = yyjson_read_(data_json, strlen(data_json), 0);
    if (doc == NULL) {
        // Invalid JSON - apply truncation to raw content
        return ik_event_render_format_tool_result_raw(ctx, content);
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    yyjson_val *name_val = yyjson_obj_get_(root, "name");
    yyjson_val *output_val = yyjson_obj_get_(root, "output");

    if (name_val == NULL || !yyjson_is_str(name_val)) {
        yyjson_doc_free(doc);
        // Missing tool name - apply truncation to raw content
        return ik_event_render_format_tool_result_raw(ctx, content);
    }

    const char *tool_name = yyjson_get_str_(name_val);
    const char *output = (output_val != NULL && yyjson_is_str(output_val))
                         ? yyjson_get_str_(output_val)
                         : NULL;

    // Apply formatting (includes truncation)
    const char *formatted = ik_format_tool_result(ctx, tool_name, output);

    yyjson_doc_free(doc);
    return formatted;
}
