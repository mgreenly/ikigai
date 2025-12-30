/**
 * @file response_helpers.c
 * @brief Anthropic response parsing helper functions
 */

#include "response_helpers.h"

#include "json_allocator.h"
#include "panic.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

/**
 * Parse content block array from JSON
 */
res_t ik_anthropic_parse_content_blocks(TALLOC_CTX *ctx, yyjson_val *content_arr,
                                         ik_content_block_t **out_blocks, size_t *out_count)
{
    assert(ctx != NULL);         // LCOV_EXCL_BR_LINE
    assert(content_arr != NULL); // LCOV_EXCL_BR_LINE
    assert(out_blocks != NULL);  // LCOV_EXCL_BR_LINE
    assert(out_count != NULL);   // LCOV_EXCL_BR_LINE

    size_t count = yyjson_arr_size(content_arr);
    if (count == 0) {
        *out_blocks = NULL;
        *out_count = 0;
        return OK(NULL);
    }

    // Safe cast: content block count should never exceed UINT_MAX in practice
    assert(count <= (size_t)UINT_MAX); // LCOV_EXCL_BR_LINE
    ik_content_block_t *blocks = talloc_array(ctx, ik_content_block_t, (unsigned int)count);
    if (blocks == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    size_t idx, max;
    yyjson_val *item;
    yyjson_arr_foreach(content_arr, idx, max, item) { // LCOV_EXCL_BR_LINE
        yyjson_val *type_val = yyjson_obj_get(item, "type");
        if (type_val == NULL) {
            return ERR(ctx, PARSE, "Content block missing 'type' field");
        }

        const char *type_str = yyjson_get_str(type_val);
        if (type_str == NULL) {
            return ERR(ctx, PARSE, "Content block 'type' is not a string");
        }

        if (strcmp(type_str, "text") == 0) {
            blocks[idx].type = IK_CONTENT_TEXT; // LCOV_EXCL_BR_LINE
            yyjson_val *text_val = yyjson_obj_get(item, "text");
            if (text_val == NULL) {
                return ERR(ctx, PARSE, "Text block missing 'text' field");
            }
            const char *text = yyjson_get_str(text_val);
            if (text == NULL) {
                return ERR(ctx, PARSE, "Text block 'text' is not a string");
            }
            blocks[idx].data.text.text = talloc_strdup(blocks, text);
            if (blocks[idx].data.text.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        } else if (strcmp(type_str, "thinking") == 0) {
            blocks[idx].type = IK_CONTENT_THINKING; // LCOV_EXCL_BR_LINE
            yyjson_val *thinking_val = yyjson_obj_get(item, "thinking");
            if (thinking_val == NULL) {
                return ERR(ctx, PARSE, "Thinking block missing 'thinking' field");
            }
            const char *thinking = yyjson_get_str(thinking_val);
            if (thinking == NULL) {
                return ERR(ctx, PARSE, "Thinking block 'thinking' is not a string");
            }
            blocks[idx].data.thinking.text = talloc_strdup(blocks, thinking);
            if (blocks[idx].data.thinking.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        } else if (strcmp(type_str, "redacted_thinking") == 0) {
            blocks[idx].type = IK_CONTENT_THINKING;
            blocks[idx].data.thinking.text = talloc_strdup(blocks, "[thinking redacted]");
            if (blocks[idx].data.thinking.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        } else if (strcmp(type_str, "tool_use") == 0) {
            blocks[idx].type = IK_CONTENT_TOOL_CALL; // LCOV_EXCL_BR_LINE

            // Extract id
            yyjson_val *id_val = yyjson_obj_get(item, "id");
            if (id_val == NULL) {
                return ERR(ctx, PARSE, "Tool use block missing 'id' field");
            }
            const char *id = yyjson_get_str(id_val);
            if (id == NULL) {
                return ERR(ctx, PARSE, "Tool use 'id' is not a string");
            }
            blocks[idx].data.tool_call.id = talloc_strdup(blocks, id);
            if (blocks[idx].data.tool_call.id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

            // Extract name
            yyjson_val *name_val = yyjson_obj_get(item, "name");
            if (name_val == NULL) {
                return ERR(ctx, PARSE, "Tool use block missing 'name' field");
            }
            const char *name = yyjson_get_str(name_val);
            if (name == NULL) {
                return ERR(ctx, PARSE, "Tool use 'name' is not a string");
            }
            blocks[idx].data.tool_call.name = talloc_strdup(blocks, name);
            if (blocks[idx].data.tool_call.name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

            // Extract input (serialize to JSON string)
            yyjson_val *input_val = yyjson_obj_get(item, "input");
            if (input_val == NULL) {
                return ERR(ctx, PARSE, "Tool use block missing 'input' field");
            }

            // Serialize input to JSON string
            char *input_json = yyjson_val_write(input_val, 0, NULL);
            if (input_json == NULL) { // LCOV_EXCL_LINE - only fails on OOM
                return ERR(ctx, PARSE, "Failed to serialize tool input"); // LCOV_EXCL_LINE
            } // LCOV_EXCL_LINE
            blocks[idx].data.tool_call.arguments = talloc_strdup(blocks, input_json);
            free(input_json); // yyjson allocates with malloc
            if (blocks[idx].data.tool_call.arguments == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        } else {
            // Unknown type - log warning and skip (task says to continue parsing)
            // For now, treat as text block with warning marker
            blocks[idx].type = IK_CONTENT_TEXT;
            char *warning = talloc_asprintf(blocks, "[unknown content type: %s]", type_str);
            if (warning == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            blocks[idx].data.text.text = warning;
        }
    }

    *out_blocks = blocks;
    *out_count = count;
    return OK(NULL);
}

/**
 * Parse usage statistics from JSON
 */
void ik_anthropic_parse_usage(yyjson_val *usage_obj, ik_usage_t *out_usage)
{
    assert(out_usage != NULL); // LCOV_EXCL_BR_LINE

    // Initialize to zero
    memset(out_usage, 0, sizeof(ik_usage_t));

    if (usage_obj == NULL) {
        return; // All zeros
    }

    // Extract input_tokens
    yyjson_val *input_val = yyjson_obj_get(usage_obj, "input_tokens");
    if (input_val != NULL && yyjson_is_int(input_val)) {
        out_usage->input_tokens = (int32_t)yyjson_get_int(input_val);
    }

    // Extract output_tokens
    yyjson_val *output_val = yyjson_obj_get(usage_obj, "output_tokens");
    if (output_val != NULL && yyjson_is_int(output_val)) {
        out_usage->output_tokens = (int32_t)yyjson_get_int(output_val);
    }

    // Extract thinking_tokens (optional)
    yyjson_val *thinking_val = yyjson_obj_get(usage_obj, "thinking_tokens");
    if (thinking_val != NULL && yyjson_is_int(thinking_val)) {
        out_usage->thinking_tokens = (int32_t)yyjson_get_int(thinking_val);
    }

    // Extract cache_read_input_tokens (optional)
    yyjson_val *cached_val = yyjson_obj_get(usage_obj, "cache_read_input_tokens");
    if (cached_val != NULL && yyjson_is_int(cached_val)) {
        out_usage->cached_tokens = (int32_t)yyjson_get_int(cached_val);
    }

    // Calculate total
    out_usage->total_tokens = out_usage->input_tokens + out_usage->output_tokens +
                              out_usage->thinking_tokens;
}
