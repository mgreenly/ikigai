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
