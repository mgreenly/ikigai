#include "message.h"

#include "panic.h"
#include "providers/request.h"
#include "wrapper.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

ik_message_t *ik_message_create_text(TALLOC_CTX *ctx, ik_role_t role, const char *text)
{
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->role = role;
    msg->content_blocks = talloc_zero_array(msg, ik_content_block_t, 1);
    if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Create text content block using existing helper
    ik_content_block_t *block = ik_content_block_text(msg, text);
    msg->content_blocks[0] = *block;
    msg->content_count = 1;
    msg->provider_metadata = NULL;

    return msg;
}

ik_message_t *ik_message_create_tool_call(TALLOC_CTX *ctx, const char *id,
                                          const char *name, const char *arguments)
{
    assert(id != NULL);        // LCOV_EXCL_BR_LINE
    assert(name != NULL);      // LCOV_EXCL_BR_LINE
    assert(arguments != NULL); // LCOV_EXCL_BR_LINE

    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->role = IK_ROLE_ASSISTANT;
    msg->content_blocks = talloc_zero_array(msg, ik_content_block_t, 1);
    if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Create tool call content block using existing helper
    ik_content_block_t *block = ik_content_block_tool_call(msg, id, name, arguments);
    msg->content_blocks[0] = *block;
    msg->content_count = 1;
    msg->provider_metadata = NULL;

    return msg;
}

ik_message_t *ik_message_create_tool_call_with_thinking(
    TALLOC_CTX *ctx,
    const char *thinking_text,
    const char *thinking_sig,
    const char *redacted_data,
    const char *tool_id,
    const char *tool_name,
    const char *tool_args,
    const char *tool_thought_sig)
{
    assert(tool_id != NULL);    // LCOV_EXCL_BR_LINE
    assert(tool_name != NULL);  // LCOV_EXCL_BR_LINE
    assert(tool_args != NULL);  // LCOV_EXCL_BR_LINE

    // Count blocks
    size_t block_count = 1;  // tool_call always present
    if (thinking_text != NULL) block_count++;
    if (redacted_data != NULL) block_count++;

    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->role = IK_ROLE_ASSISTANT;
    msg->content_blocks = talloc_zero_array(msg, ik_content_block_t, (unsigned int)block_count);
    if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg->content_count = block_count;
    msg->provider_metadata = NULL;

    size_t idx = 0;

    // Add thinking block first (if present)
    if (thinking_text != NULL) {
        msg->content_blocks[idx].type = IK_CONTENT_THINKING;
        msg->content_blocks[idx].data.thinking.text = talloc_strdup(msg, thinking_text);
        if (!msg->content_blocks[idx].data.thinking.text) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (thinking_sig != NULL) {
            msg->content_blocks[idx].data.thinking.signature = talloc_strdup(msg, thinking_sig);
            if (!msg->content_blocks[idx].data.thinking.signature) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        } else {
            msg->content_blocks[idx].data.thinking.signature = NULL;
        }
        idx++;
    }

    // Add redacted thinking (if present)
    if (redacted_data != NULL) {
        msg->content_blocks[idx].type = IK_CONTENT_REDACTED_THINKING;
        msg->content_blocks[idx].data.redacted_thinking.data = talloc_strdup(msg, redacted_data);
        if (!msg->content_blocks[idx].data.redacted_thinking.data) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        idx++;
    }

    // Add tool_call - populate directly (don't use helper which allocates separately)
    msg->content_blocks[idx].type = IK_CONTENT_TOOL_CALL;
    msg->content_blocks[idx].data.tool_call.id = talloc_strdup(msg, tool_id);
    if (!msg->content_blocks[idx].data.tool_call.id) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg->content_blocks[idx].data.tool_call.name = talloc_strdup(msg, tool_name);
    if (!msg->content_blocks[idx].data.tool_call.name) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg->content_blocks[idx].data.tool_call.arguments = talloc_strdup(msg, tool_args);
    if (!msg->content_blocks[idx].data.tool_call.arguments) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg->content_blocks[idx].data.tool_call.thought_signature = NULL;
    if (tool_thought_sig != NULL) {
        msg->content_blocks[idx].data.tool_call.thought_signature = talloc_strdup(msg, tool_thought_sig);
        if (!msg->content_blocks[idx].data.tool_call.thought_signature) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }

    return msg;
}

ik_message_t *ik_message_create_tool_result(TALLOC_CTX *ctx, const char *tool_call_id,
                                            const char *content, bool is_error)
{
    assert(tool_call_id != NULL); // LCOV_EXCL_BR_LINE
    assert(content != NULL);      // LCOV_EXCL_BR_LINE

    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->role = IK_ROLE_TOOL;
    msg->content_blocks = talloc_zero_array(msg, ik_content_block_t, 1);
    if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Create tool result content block using existing helper
    ik_content_block_t *block = ik_content_block_tool_result(msg, tool_call_id, content, is_error);
    msg->content_blocks[0] = *block;
    msg->content_count = 1;
    msg->provider_metadata = NULL;

    return msg;
}

res_t ik_message_from_db_msg(TALLOC_CTX *ctx, const ik_msg_t *db_msg, ik_message_t **out)
{
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

        yyjson_val *root = yyjson_doc_get_root_(doc);
        yyjson_val *id_val = yyjson_obj_get_(root, "tool_call_id");
        yyjson_val *name_val = yyjson_obj_get_(root, "tool_name");
        yyjson_val *args_val = yyjson_obj_get_(root, "tool_args");

        if (!id_val || !name_val || !args_val) {
            yyjson_doc_free(doc);
            return ERR(ctx, PARSE, "Missing required fields in tool_call data_json");
        }

        const char *id = yyjson_get_str_(id_val);
        const char *name = yyjson_get_str_(name_val);
        const char *arguments = yyjson_get_str_(args_val);

        if (!id || !name || !arguments) {
            yyjson_doc_free(doc);
            return ERR(ctx, PARSE, "Invalid field types in tool_call data_json");
        }

        // Parse optional thinking blocks
        yyjson_val *thinking_obj = yyjson_obj_get_(root, "thinking");
        yyjson_val *redacted_obj = yyjson_obj_get_(root, "redacted_thinking");

        const char *thinking_text = NULL;
        const char *thinking_sig = NULL;
        const char *redacted_data = NULL;

        if (thinking_obj != NULL && yyjson_is_obj(thinking_obj)) {
            yyjson_val *text_val = yyjson_obj_get_(thinking_obj, "text");
            yyjson_val *sig_val = yyjson_obj_get_(thinking_obj, "signature");
            thinking_text = yyjson_get_str(text_val);
            thinking_sig = yyjson_get_str(sig_val);
        }

        if (redacted_obj != NULL && yyjson_is_obj(redacted_obj)) {
            yyjson_val *data_val = yyjson_obj_get_(redacted_obj, "data");
            redacted_data = yyjson_get_str(data_val);
        }

        // Create message with appropriate blocks
        if (thinking_text != NULL || redacted_data != NULL) {
            *out = ik_message_create_tool_call_with_thinking(
                ctx, thinking_text, thinking_sig, redacted_data, id, name, arguments, NULL);
        } else {
            *out = ik_message_create_tool_call(ctx, id, name, arguments); // LCOV_EXCL_BR_LINE
        }

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

        yyjson_val *root = yyjson_doc_get_root_(doc);
        yyjson_val *id_val = yyjson_obj_get_(root, "tool_call_id");
        yyjson_val *output_val = yyjson_obj_get_(root, "output");
        yyjson_val *success_val = yyjson_obj_get_(root, "success");

        if (!id_val || !output_val) {
            yyjson_doc_free(doc);
            return ERR(ctx, PARSE, "Missing required fields in tool_result data_json");
        }

        const char *tool_call_id = yyjson_get_str_(id_val);
        const char *output = yyjson_get_str_(output_val);

        if (!tool_call_id || !output) {
            yyjson_doc_free(doc);
            return ERR(ctx, PARSE, "Invalid field types in tool_result data_json");
        }

        // Map success to is_error (inverted boolean)
        bool is_error = success_val ? !yyjson_get_bool(success_val) : false;

        *out = ik_message_create_tool_result(ctx, tool_call_id, output, is_error); // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return OK(*out);
    }

    // Unknown message kind
    return ERR(ctx, PARSE, "Unknown message kind: %s", db_msg->kind);
}
