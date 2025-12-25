/**
 * @file serialize.c
 * @brief OpenAI JSON serialization implementation
 */

#include "serialize.h"
#include "panic.h"

#include <assert.h>
#include <string.h>

yyjson_mut_val *ik_openai_serialize_message(yyjson_mut_doc *doc, const ik_message_t *msg)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE
    assert(msg != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
    if (msg_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Add role */
    const char *role_str = NULL;
    switch (msg->role) {
    case IK_ROLE_USER:
        role_str = "user";
        break;
    case IK_ROLE_ASSISTANT:
        role_str = "assistant";
        break;
    case IK_ROLE_TOOL:
        role_str = "tool";
        break;
    default:
        PANIC("Unknown role");  // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_str(doc, msg_obj, "role", role_str)) {
        PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }

    /* Handle different message types */
    if (msg->role == IK_ROLE_TOOL) {
        /* Tool result message */
        if (msg->content_count > 0 && msg->content_blocks[0].type == IK_CONTENT_TOOL_RESULT) {
            const ik_content_block_t *block = &msg->content_blocks[0];
            if (!yyjson_mut_obj_add_str(doc, msg_obj, "tool_call_id", block->data.tool_result.tool_call_id)) {
                PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            }
            if (!yyjson_mut_obj_add_str(doc, msg_obj, "content", block->data.tool_result.content)) {
                PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            }
        }
    } else {
        /* Check if this is an assistant message with tool calls */
        bool has_tool_calls = false;
        for (size_t i = 0; i < msg->content_count; i++) {
            if (msg->content_blocks[i].type == IK_CONTENT_TOOL_CALL) {
                has_tool_calls = true;
                break;
            }
        }

        if (has_tool_calls) {
            /* Assistant message with tool calls: content is null, add tool_calls array */
            if (!yyjson_mut_obj_add_null(doc, msg_obj, "content")) {
                PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            }

            yyjson_mut_val *tool_calls_arr = yyjson_mut_arr(doc);
            if (tool_calls_arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            for (size_t i = 0; i < msg->content_count; i++) {
                if (msg->content_blocks[i].type == IK_CONTENT_TOOL_CALL) {
                    const ik_content_block_t *block = &msg->content_blocks[i];

                    yyjson_mut_val *tc_obj = yyjson_mut_obj(doc);
                    if (tc_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                    if (!yyjson_mut_obj_add_str(doc, tc_obj, "id", block->data.tool_call.id)) {
                        PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                    }
                    if (!yyjson_mut_obj_add_str(doc, tc_obj, "type", "function")) {
                        PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                    }

                    yyjson_mut_val *func_obj = yyjson_mut_obj(doc);
                    if (func_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                    if (!yyjson_mut_obj_add_str(doc, func_obj, "name", block->data.tool_call.name)) {
                        PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                    }
                    if (!yyjson_mut_obj_add_str(doc, func_obj, "arguments", block->data.tool_call.arguments)) {
                        PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                    }

                    if (!yyjson_mut_obj_add_val(doc, tc_obj, "function", func_obj)) {
                        PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                    }

                    if (!yyjson_mut_arr_append(tool_calls_arr, tc_obj)) {
                        PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                    }
                }
            }

            if (!yyjson_mut_obj_add_val(doc, msg_obj, "tool_calls", tool_calls_arr)) {
                PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            }
        } else {
            /* Regular message: concatenate text content */
            /* Build content string using yyjson's string pooling */
            size_t total_len = 0;
            for (size_t i = 0; i < msg->content_count; i++) {
                if (msg->content_blocks[i].type == IK_CONTENT_TEXT) {
                    if (total_len > 0) total_len += 2; /* "\n\n" */
                    total_len += strlen(msg->content_blocks[i].data.text.text);
                }
            }

            if (total_len == 0) {
                /* Empty content */
                if (!yyjson_mut_obj_add_str(doc, msg_obj, "content", "")) {
                    PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                }
            } else {
                /* Allocate buffer for concatenated content */
                char *content = malloc(total_len + 1);
                if (content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

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

                if (!yyjson_mut_obj_add_strcpy(doc, msg_obj, "content", content)) {
                    free(content);
                    PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                }
                free(content);
            }
        }
    }

    return msg_obj;
}
