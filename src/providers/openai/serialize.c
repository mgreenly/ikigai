/**
 * @file serialize.c
 * @brief OpenAI JSON serialization implementation
 */

#include "serialize.h"
#include "panic.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static const char *map_role_to_string(ik_role_t role)
{
    switch (role) {  // LCOV_EXCL_BR_LINE: default case unreachable
        case IK_ROLE_USER:
            return "user";
        case IK_ROLE_ASSISTANT:
            return "assistant";
        case IK_ROLE_TOOL:
            return "tool";
        default: // LCOV_EXCL_LINE
            PANIC("Unknown role"); // LCOV_EXCL_LINE
    }
}

static void add_tool_result_content(yyjson_mut_doc *doc, yyjson_mut_val *msg_obj,
                                   const ik_message_t *msg)
{
    if (msg->content_count > 0 && msg->content_blocks[0].type == IK_CONTENT_TOOL_RESULT) {
        const ik_content_block_t *block = &msg->content_blocks[0];
        if (!yyjson_mut_obj_add_str(doc, msg_obj, "tool_call_id", block->data.tool_result.tool_call_id)) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
        if (!yyjson_mut_obj_add_str(doc, msg_obj, "content", block->data.tool_result.content)) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
    }
}

static bool has_tool_calls(const ik_message_t *msg)
{
    for (size_t i = 0; i < msg->content_count; i++) {
        if (msg->content_blocks[i].type == IK_CONTENT_TOOL_CALL) {
            return true;
        }
    }
    return false;
}

static void add_tool_call_to_array(yyjson_mut_doc *doc, yyjson_mut_val *tool_calls_arr,
                                   const ik_content_block_t *block)
{
    yyjson_mut_val *tc_obj = yyjson_mut_obj(doc);
    if (tc_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_LINE

    if (!yyjson_mut_obj_add_str(doc, tc_obj, "id", block->data.tool_call.id)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }
    if (!yyjson_mut_obj_add_str(doc, tc_obj, "type", "function")) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    yyjson_mut_val *func_obj = yyjson_mut_obj(doc);
    if (func_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_LINE

    if (!yyjson_mut_obj_add_str(doc, func_obj, "name", block->data.tool_call.name)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }
    if (!yyjson_mut_obj_add_str(doc, func_obj, "arguments", block->data.tool_call.arguments)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, tc_obj, "function", func_obj)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_arr_append(tool_calls_arr, tc_obj)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }
}

static void add_tool_calls_content(yyjson_mut_doc *doc, yyjson_mut_val *msg_obj,
                                  const ik_message_t *msg)
{
    if (!yyjson_mut_obj_add_null(doc, msg_obj, "content")) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    yyjson_mut_val *tool_calls_arr = yyjson_mut_arr(doc);
    if (tool_calls_arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_LINE

    for (size_t i = 0; i < msg->content_count; i++) {
        if (msg->content_blocks[i].type == IK_CONTENT_TOOL_CALL) {
            add_tool_call_to_array(doc, tool_calls_arr, &msg->content_blocks[i]);
        }
    }

    if (!yyjson_mut_obj_add_val(doc, msg_obj, "tool_calls", tool_calls_arr)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }
}

static size_t calculate_text_content_length(const ik_message_t *msg)
{
    size_t total_len = 0;
    for (size_t i = 0; i < msg->content_count; i++) {
        if (msg->content_blocks[i].type == IK_CONTENT_TEXT) {
            if (total_len > 0) total_len += 2; /* "\n\n" */
            total_len += strlen(msg->content_blocks[i].data.text.text);
        }
    }
    return total_len;
}

static void concatenate_text_blocks(const ik_message_t *msg, char *content)
{
    content[0] = '\0';
    bool first = true;
    for (size_t i = 0; i < msg->content_count; i++) {
        if (msg->content_blocks[i].type == IK_CONTENT_TEXT) {
            if (!first) {
                strcat(content, "\n\n");  /* NOLINT - buffer sized correctly */
            }
            strcat(content, msg->content_blocks[i].data.text.text);  /* NOLINT - buffer sized correctly */
            first = false;
        }
    }
}

static void add_text_content(yyjson_mut_doc *doc, yyjson_mut_val *msg_obj,
                            const ik_message_t *msg)
{
    size_t total_len = calculate_text_content_length(msg);

    if (total_len == 0) {
        if (!yyjson_mut_obj_add_str(doc, msg_obj, "content", "")) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
    } else {
        char *content = malloc(total_len + 1);
        if (content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        concatenate_text_blocks(msg, content);

        if (!yyjson_mut_obj_add_strcpy(doc, msg_obj, "content", content)) {  // LCOV_EXCL_BR_LINE
            free(content);  // LCOV_EXCL_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
        free(content);
    }
}

yyjson_mut_val *ik_openai_serialize_message(yyjson_mut_doc *doc, const ik_message_t *msg)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE
    assert(msg != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
    if (msg_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_LINE

    const char *role_str = map_role_to_string(msg->role);
    if (!yyjson_mut_obj_add_str(doc, msg_obj, "role", role_str)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    if (msg->role == IK_ROLE_TOOL) {
        add_tool_result_content(doc, msg_obj, msg);
    } else if (has_tool_calls(msg)) {
        add_tool_calls_content(doc, msg_obj, msg);
    } else {
        add_text_content(doc, msg_obj, msg);
    }

    return msg_obj;
}
