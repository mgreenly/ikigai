// REPL tool JSON building utilities
#include "apps/ikigai/repl_tool_json.h"

#include "shared/panic.h"
#include "shared/wrapper.h"

#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include "vendor/yyjson/yyjson.h"


#include "shared/poison.h"
// Build tool_call data_json for database with thinking/redacted blocks.
char *ik_build_tool_call_data_json(TALLOC_CTX *ctx,
                                   const ik_tool_call_t *tc,
                                   const char *thinking_text,
                                   const char *thinking_signature,
                                   const char *redacted_data)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);
    if (!yyjson_mut_obj_add_str(doc, root, "tool_call_id", tc->id)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, root, "tool_name", tc->name)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, root, "tool_args", tc->arguments)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (thinking_text != NULL) {
        yyjson_mut_val *thinking_obj = yyjson_mut_obj(doc);
        if (thinking_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_str(doc, thinking_obj, "text", thinking_text)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (thinking_signature != NULL) {
            if (!yyjson_mut_obj_add_str(doc, thinking_obj, "signature", thinking_signature)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
        if (!yyjson_mut_obj_add_val(doc, root, "thinking", thinking_obj)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }
    if (redacted_data != NULL) {
        yyjson_mut_val *redacted_obj = yyjson_mut_obj(doc);
        if (redacted_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_str(doc, redacted_obj, "data", redacted_data)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_val(doc, root, "redacted_thinking", redacted_obj)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }

    char *json = yyjson_mut_write(doc, 0, NULL);
    char *data_json = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(doc);
    if (data_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return data_json;
}

// Build tool_result data_json for database.
char *ik_build_tool_result_data_json(TALLOC_CTX *ctx,
                                     const char *tool_call_id,
                                     const char *tool_name,
                                     const char *result_json)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);
    if (!yyjson_mut_obj_add_str(doc, root, "tool_call_id", tool_call_id)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, root, "name", tool_name)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, root, "output", result_json)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    bool success = false;
    yyjson_doc *result_doc = yyjson_read(result_json, strlen(result_json), 0);
    if (result_doc != NULL) {
        yyjson_val *result_root = yyjson_doc_get_root_(result_doc);
        yyjson_val *success_val = yyjson_obj_get_(result_root, "tool_success");
        if (success_val != NULL) {
            success = yyjson_get_bool(success_val);
        }
        yyjson_doc_free(result_doc);
    }

    if (!yyjson_mut_obj_add_bool(doc, root, "success", success)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *json = yyjson_mut_write(doc, 0, NULL);
    char *data_json = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(doc);
    if (data_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return data_json;
}
