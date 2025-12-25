#include "message.h"

#include "panic.h"
#include "providers/request.h"
#include "wrapper.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

ik_message_t *ik_message_create_text(TALLOC_CTX *ctx, ik_role_t role, const char *text) {
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->role = role;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Create text content block using existing helper
    ik_content_block_t *block = ik_content_block_text(msg, text);
    msg->content_blocks[0] = *block;
    msg->content_count = 1;
    msg->provider_metadata = NULL;

    return msg;
}

ik_message_t *ik_message_create_tool_call(TALLOC_CTX *ctx, const char *id,
                                           const char *name, const char *arguments) {
    assert(id != NULL);        // LCOV_EXCL_BR_LINE
    assert(name != NULL);      // LCOV_EXCL_BR_LINE
    assert(arguments != NULL); // LCOV_EXCL_BR_LINE

    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->role = IK_ROLE_ASSISTANT;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Create tool call content block using existing helper
    ik_content_block_t *block = ik_content_block_tool_call(msg, id, name, arguments);
    msg->content_blocks[0] = *block;
    msg->content_count = 1;
    msg->provider_metadata = NULL;

    return msg;
}

ik_message_t *ik_message_create_tool_result(TALLOC_CTX *ctx, const char *tool_call_id,
                                             const char *content, bool is_error) {
    assert(tool_call_id != NULL); // LCOV_EXCL_BR_LINE
    assert(content != NULL);      // LCOV_EXCL_BR_LINE

    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->role = IK_ROLE_TOOL;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Create tool result content block using existing helper
    ik_content_block_t *block = ik_content_block_tool_result(msg, tool_call_id, content, is_error);
    msg->content_blocks[0] = *block;
    msg->content_count = 1;
    msg->provider_metadata = NULL;

    return msg;
}

res_t ik_message_from_db_msg(TALLOC_CTX *ctx, const ik_msg_t *db_msg, ik_message_t **out) {
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(db_msg != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);    // LCOV_EXCL_BR_LINE

    // Handle system messages - they go in request->system_prompt, not messages array
    if (strcmp(db_msg->kind, "system") == 0) {
        *out = NULL;
        return OK(NULL);
    }

    // Handle user messages
    if (strcmp(db_msg->kind, "user") == 0) {
        if (db_msg->content == NULL) {
            return ERR(ctx, PARSE, "User message missing content");
        }
        *out = ik_message_create_text(ctx, IK_ROLE_USER, db_msg->content);
        return OK(*out);
    }

    // Handle assistant messages
    if (strcmp(db_msg->kind, "assistant") == 0) {
        if (db_msg->content == NULL) {
            return ERR(ctx, PARSE, "Assistant message missing content");
        }
        *out = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, db_msg->content);
        return OK(*out);
    }

    // Handle tool_call messages - parse data_json
    if (strcmp(db_msg->kind, "tool_call") == 0) {
        if (db_msg->data_json == NULL) {
            return ERR(ctx, PARSE, "Tool call message missing data_json");
        }

        // Parse JSON
        yyjson_doc *doc = yyjson_read(db_msg->data_json, strlen(db_msg->data_json), 0);
        if (!doc) {
            return ERR(ctx, PARSE, "Invalid JSON in tool_call data_json");
        }

        yyjson_val *root = yyjson_doc_get_root(doc);
        yyjson_val *id_val = yyjson_obj_get(root, "tool_call_id");
        yyjson_val *name_val = yyjson_obj_get(root, "name");
        yyjson_val *args_val = yyjson_obj_get(root, "arguments");

        if (!id_val || !name_val || !args_val) {
            yyjson_doc_free(doc);
            return ERR(ctx, PARSE, "Missing required fields in tool_call data_json");
        }

        const char *id = yyjson_get_str(id_val);
        const char *name = yyjson_get_str(name_val);
        const char *arguments = yyjson_get_str(args_val);

        if (!id || !name || !arguments) {
            yyjson_doc_free(doc);
            return ERR(ctx, PARSE, "Invalid field types in tool_call data_json");
        }

        *out = ik_message_create_tool_call(ctx, id, name, arguments);
        yyjson_doc_free(doc);
        return OK(*out);
    }

    // Handle tool_result and tool messages - parse data_json
    if (strcmp(db_msg->kind, "tool_result") == 0 || strcmp(db_msg->kind, "tool") == 0) {
        if (db_msg->data_json == NULL) {
            return ERR(ctx, PARSE, "Tool result message missing data_json");
        }

        // Parse JSON
        yyjson_doc *doc = yyjson_read(db_msg->data_json, strlen(db_msg->data_json), 0);
        if (!doc) {
            return ERR(ctx, PARSE, "Invalid JSON in tool_result data_json");
        }

        yyjson_val *root = yyjson_doc_get_root(doc);
        yyjson_val *id_val = yyjson_obj_get(root, "tool_call_id");
        yyjson_val *output_val = yyjson_obj_get(root, "output");
        yyjson_val *success_val = yyjson_obj_get(root, "success");

        if (!id_val || !output_val) {
            yyjson_doc_free(doc);
            return ERR(ctx, PARSE, "Missing required fields in tool_result data_json");
        }

        const char *tool_call_id = yyjson_get_str(id_val);
        const char *output = yyjson_get_str(output_val);

        if (!tool_call_id || !output) {
            yyjson_doc_free(doc);
            return ERR(ctx, PARSE, "Invalid field types in tool_result data_json");
        }

        // Map success to is_error (inverted boolean)
        bool is_error = success_val ? !yyjson_get_bool(success_val) : false;

        *out = ik_message_create_tool_result(ctx, tool_call_id, output, is_error);
        yyjson_doc_free(doc);
        return OK(*out);
    }

    // Unknown message kind
    return ERR(ctx, PARSE, "Unknown message kind: %s", db_msg->kind);
}
