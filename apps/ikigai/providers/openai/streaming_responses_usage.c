/**
 * @file streaming_responses_usage.c
 * @brief OpenAI Responses API usage parsing utilities
 */

#include "apps/ikigai/providers/openai/streaming_responses_internal.h"

#include "shared/wrapper_json.h"

#include <assert.h>


#include "shared/poison.h"
/**
 * Parse usage object from JSON
 */
void ik_openai_responses_parse_usage(yyjson_val *usage_val, ik_usage_t *out_usage)
{
    assert(usage_val != NULL); // LCOV_EXCL_BR_LINE
    assert(out_usage != NULL); // LCOV_EXCL_BR_LINE

    if (!yyjson_is_obj(usage_val)) {
        return;
    }

    yyjson_val *input_tokens_val = yyjson_obj_get(usage_val, "input_tokens");
    if (input_tokens_val != NULL && yyjson_is_int(input_tokens_val)) {
        out_usage->input_tokens = (int32_t)yyjson_get_int(input_tokens_val);
    }

    yyjson_val *output_tokens_val = yyjson_obj_get(usage_val, "output_tokens");
    if (output_tokens_val != NULL && yyjson_is_int(output_tokens_val)) {
        out_usage->output_tokens = (int32_t)yyjson_get_int(output_tokens_val);
    }

    yyjson_val *total_tokens_val = yyjson_obj_get(usage_val, "total_tokens");
    if (total_tokens_val != NULL && yyjson_is_int(total_tokens_val)) {
        out_usage->total_tokens = (int32_t)yyjson_get_int(total_tokens_val);
    } else if (out_usage->input_tokens > 0 || out_usage->output_tokens > 0) {
        out_usage->total_tokens = out_usage->input_tokens + out_usage->output_tokens;
    }

    yyjson_val *details_val = yyjson_obj_get(usage_val, "output_tokens_details");
    if (details_val != NULL && yyjson_is_obj(details_val)) {
        yyjson_val *reasoning_tokens_val = yyjson_obj_get(details_val, "reasoning_tokens");
        if (reasoning_tokens_val != NULL && yyjson_is_int(reasoning_tokens_val)) {
            out_usage->thinking_tokens = (int32_t)yyjson_get_int(reasoning_tokens_val);
        }
    }
}
