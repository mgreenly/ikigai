#include "openai/client.h"

#include "error.h"
#include "wrapper.h"

#include <assert.h>
#include <talloc.h>
#include "vendor/yyjson/yyjson.h"

/**
 * OpenAI message creation
 *
 * This module handles creation of OpenAI message structures.
 */

/*
 * Message functions
 */

res_t ik_openai_msg_create(void *parent, const char *role, const char *content) {
    assert(role != NULL); // LCOV_EXCL_BR_LINE
    assert(content != NULL); // LCOV_EXCL_BR_LINE

    ik_msg_t *msg = talloc_zero(parent, ik_msg_t);
    if (!msg) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate message"); // LCOV_EXCL_LINE
    }
    msg->id = 0;  /* In-memory message, not from DB */

    msg->kind = talloc_strdup(msg, role);
    if (!msg->kind) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate role string"); // LCOV_EXCL_LINE
    }

    msg->content = talloc_strdup(msg, content);
    if (!msg->content) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate content string"); // LCOV_EXCL_LINE
    }

    msg->data_json = NULL;  /* Text messages have no structured data */

    return OK(msg);
}

ik_msg_t *ik_openai_msg_create_tool_call(void *parent,
                                                const char *id,
                                                const char *type,
                                                const char *name,
                                                const char *arguments,
                                                const char *content) {
    assert(id != NULL);        // LCOV_EXCL_BR_LINE
    assert(type != NULL);      // LCOV_EXCL_BR_LINE
    assert(name != NULL);      // LCOV_EXCL_BR_LINE
    assert(arguments != NULL); // LCOV_EXCL_BR_LINE
    assert(content != NULL);   // LCOV_EXCL_BR_LINE

    /* Allocate message struct */
    ik_msg_t *msg = talloc_zero(parent, ik_msg_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg->id = 0;  /* In-memory message, not from DB */

    /* Set role to "tool_call" */
    msg->kind = talloc_strdup(msg, "tool_call");
    if (!msg->kind) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Set content to human-readable summary */
    msg->content = talloc_strdup(msg, content);
    if (!msg->content) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Build data_json with structured tool call data */
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (!root) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);

    /* Add id */
    if (!yyjson_mut_obj_add_str(doc, root, "id", id)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Add type */
    if (!yyjson_mut_obj_add_str(doc, root, "type", type)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Create function object */
    yyjson_mut_val *function_obj = yyjson_mut_obj(doc);
    if (!function_obj) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Add name to function */
    if (!yyjson_mut_obj_add_str(doc, function_obj, "name", name)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Add arguments to function */
    if (!yyjson_mut_obj_add_str(doc, function_obj, "arguments", arguments)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Add function object to root */
    if (!yyjson_mut_obj_add_val(doc, root, "function", function_obj)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Convert to JSON string */
    size_t len = 0;
    const char *json_str = yyjson_mut_write(doc, 0, &len);
    if (!json_str) {  // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc);  // LCOV_EXCL_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Copy JSON string to message (child of msg) */
    msg->data_json = talloc_strdup(msg, json_str);
    if (!msg->data_json) {  // LCOV_EXCL_BR_LINE
        free((void *)(uintptr_t)json_str);  // LCOV_EXCL_LINE
        yyjson_mut_doc_free(doc);  // LCOV_EXCL_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Free yyjson-allocated string (not managed by talloc) */
    free((void *)(uintptr_t)json_str);

    /* Clean up yyjson document */
    yyjson_mut_doc_free(doc);

    return msg;
}

ik_msg_t *ik_openai_msg_create_tool_result(void *parent,
                                                  const char *tool_call_id,
                                                  const char *content) {
    assert(tool_call_id != NULL); // LCOV_EXCL_BR_LINE
    assert(content != NULL);      // LCOV_EXCL_BR_LINE

    /* Allocate message struct */
    ik_msg_t *msg = talloc_zero(parent, ik_msg_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg->id = 0;  /* In-memory message, not from DB */

    /* Set role to "tool_result" (canonical format, transformed during serialization) */
    msg->kind = talloc_strdup(msg, "tool_result");
    if (!msg->kind) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Set content to tool result JSON */
    msg->content = talloc_strdup(msg, content);
    if (!msg->content) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Build data_json with tool_call_id */
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (!root) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);

    /* Add tool_call_id */
    if (!yyjson_mut_obj_add_str(doc, root, "tool_call_id", tool_call_id)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Convert to JSON string */
    size_t len = 0;
    const char *json_str = yyjson_mut_write(doc, 0, &len);
    if (!json_str) {  // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc);  // LCOV_EXCL_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Copy JSON string to message (child of msg) */
    msg->data_json = talloc_strdup(msg, json_str);
    if (!msg->data_json) {  // LCOV_EXCL_BR_LINE
        free((void *)(uintptr_t)json_str);  // LCOV_EXCL_LINE
        yyjson_mut_doc_free(doc);  // LCOV_EXCL_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    /* Free yyjson-allocated string (not managed by talloc) */
    free((void *)(uintptr_t)json_str);

    /* Clean up yyjson document */
    yyjson_mut_doc_free(doc);

    return msg;
}
