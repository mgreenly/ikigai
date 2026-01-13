#include "tool_wrapper.h"

#include "json_allocator.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"

#include <string.h>
#include <talloc.h>

char *ik_tool_wrap_success(TALLOC_CTX *ctx, char *tool_result_json)
{
    // Parse the tool result JSON
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_read_err err;
    yyjson_doc *result_doc = yyjson_read_opts(tool_result_json, strlen(tool_result_json), 0, &allocator, &err);

    if (result_doc == NULL) {
        // Invalid JSON from tool - return failure wrapper instead
        talloc_free(result_doc);
        return ik_tool_wrap_failure(ctx, "Tool returned invalid JSON", "INVALID_OUTPUT");
    }

    // Create wrapper document
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_doc_set_root(doc, root);

    // Add tool_success: true
    yyjson_mut_val *success = yyjson_mut_bool(doc, true);
    if (success == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_val(doc, root, "tool_success", success)) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add tool_success field");  // LCOV_EXCL_LINE
    }

    // Add result: {...original...}
    yyjson_val *result_root = yyjson_doc_get_root(result_doc);
    yyjson_mut_val *result_copy = yyjson_val_mut_copy(doc, result_root);
    if (result_copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_val(doc, root, "result", result_copy)) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add result field");  // LCOV_EXCL_LINE
    }

    // Serialize to JSON string
    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *result = talloc_strdup(ctx, json_str);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    free(json_str);
    talloc_free(result_doc);

    return result;
}

char *ik_tool_wrap_failure(TALLOC_CTX *ctx, const char *error, const char *error_code)
{
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_doc_set_root(doc, root);

    // Add tool_success: false
    yyjson_mut_val *success = yyjson_mut_bool(doc, false);
    if (success == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_val(doc, root, "tool_success", success)) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add tool_success field");  // LCOV_EXCL_LINE
    }

    // Add error message
    yyjson_mut_val *error_val = yyjson_mut_strcpy(doc, error);
    if (error_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_val(doc, root, "error", error_val)) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add error field");  // LCOV_EXCL_LINE
    }

    // Add error code
    yyjson_mut_val *error_code_val = yyjson_mut_strcpy(doc, error_code);
    if (error_code_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_val(doc, root, "error_code", error_code_val)) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add error_code field");  // LCOV_EXCL_LINE
    }

    // Serialize to JSON string
    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *result = talloc_strdup(ctx, json_str);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    free(json_str);

    return result;
}
