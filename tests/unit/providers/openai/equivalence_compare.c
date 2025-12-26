/**
 * @file equivalence_compare.c
 * @brief Comparison functions for OpenAI equivalence validation
 */

#include "equivalence_compare.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <string.h>
#include <assert.h>
#include <math.h>

/* ================================================================
 * Token Usage Comparison
 * ================================================================ */

bool ik_compare_token_usage_tolerant(int32_t a, int32_t b)
{
    /* Handle zero cases */
    if (a == 0 && b == 0) {
        return true;
    }

    /* If one is zero and the other isn't, they don't match */
    if (a == 0 || b == 0) {
        return false;
    }

    /* Calculate percentage difference */
    int32_t diff = abs(a - b);
    int32_t max_val = (a > b) ? a : b;

    /* Allow 5% tolerance */
    double tolerance = 0.05;
    double diff_ratio = (double)diff / (double)max_val;

    return diff_ratio <= tolerance;
}

/* ================================================================
 * JSON Comparison
 * ================================================================ */

/**
 * Compare two yyjson values for semantic equivalence
 *
 * Recursively compares JSON values ignoring key order.
 */
static bool compare_json_values(yyjson_val *a, yyjson_val *b)
{
    /* Type must match */
    yyjson_type type_a = yyjson_get_type(a);
    yyjson_type type_b = yyjson_get_type(b);

    if (type_a != type_b) {
        return false;
    }

    switch (type_a) {
        case YYJSON_TYPE_NULL:
            return true;

        case YYJSON_TYPE_BOOL:
            return yyjson_get_bool(a) == yyjson_get_bool(b);

        case YYJSON_TYPE_NUM:
            /* Compare numbers with small tolerance for floating point */
            if (yyjson_is_int(a) && yyjson_is_int(b)) {
                return yyjson_get_int(a) == yyjson_get_int(b);
            }
            return fabs(yyjson_get_real(a) - yyjson_get_real(b)) < 0.0001;

        case YYJSON_TYPE_STR:
            return strcmp(yyjson_get_str(a), yyjson_get_str(b)) == 0;

        case YYJSON_TYPE_ARR: {
            size_t len_a = yyjson_arr_size(a);
            size_t len_b = yyjson_arr_size(b);

            if (len_a != len_b) {
                return false;
            }

            /* Compare elements in order */
            yyjson_arr_iter iter_a;
            yyjson_arr_iter iter_b;
            yyjson_arr_iter_init(a, &iter_a);
            yyjson_arr_iter_init(b, &iter_b);

            for (size_t i = 0; i < len_a; i++) {
                yyjson_val *elem_a = yyjson_arr_iter_next(&iter_a);
                yyjson_val *elem_b = yyjson_arr_iter_next(&iter_b);

                if (!compare_json_values(elem_a, elem_b)) {
                    return false;
                }
            }

            return true;
        }

        case YYJSON_TYPE_OBJ: {
            size_t len_a = yyjson_obj_size(a);
            size_t len_b = yyjson_obj_size(b);

            if (len_a != len_b) {
                return false;
            }

            /* For each key in a, find matching key in b and compare values */
            yyjson_obj_iter iter;
            yyjson_obj_iter_init(a, &iter);

            yyjson_val *key_a, *val_a;
            while ((key_a = yyjson_obj_iter_next(&iter))) {
                val_a = yyjson_obj_iter_get_val(key_a);
                const char *key_str = yyjson_get_str(key_a);

                /* Find same key in b */
                yyjson_val *val_b = yyjson_obj_get(b, key_str);
                if (val_b == NULL) {
                    return false;  /* Key exists in a but not in b */
                }

                /* Compare values */
                if (!compare_json_values(val_a, val_b)) {
                    return false;
                }
            }

            return true;
        }

        default:
            return false;
    }
}

ik_compare_result_t *ik_compare_json_equivalent(TALLOC_CTX *ctx,
                                                const char *json_a,
                                                const char *json_b)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(json_a != NULL);  // LCOV_EXCL_BR_LINE
    assert(json_b != NULL);  // LCOV_EXCL_BR_LINE

    ik_compare_result_t *result = talloc_zero(ctx, ik_compare_result_t);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Parse both JSON strings */
    yyjson_doc *doc_a = yyjson_read(json_a, strlen(json_a), 0);
    if (doc_a == NULL) {
        result->matches = false;
        result->diff_message = talloc_strdup(result, "Failed to parse json_a");
        return result;
    }

    yyjson_doc *doc_b = yyjson_read(json_b, strlen(json_b), 0);
    if (doc_b == NULL) {
        yyjson_doc_free(doc_a);
        result->matches = false;
        result->diff_message = talloc_strdup(result, "Failed to parse json_b");
        return result;
    }

    /* Compare root values */
    yyjson_val *root_a = yyjson_doc_get_root(doc_a);
    yyjson_val *root_b = yyjson_doc_get_root(doc_b);

    bool matches = compare_json_values(root_a, root_b);

    yyjson_doc_free(doc_a);
    yyjson_doc_free(doc_b);

    result->matches = matches;
    result->diff_message = matches ? NULL : talloc_strdup(result, "JSON values differ");

    return result;
}

/* ================================================================
 * Response Comparison
 * ================================================================ */

ik_compare_result_t *ik_compare_responses(TALLOC_CTX *ctx,
                                          const ik_response_t *resp_a,
                                          const ik_response_t *resp_b)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(resp_a != NULL);  // LCOV_EXCL_BR_LINE
    assert(resp_b != NULL);  // LCOV_EXCL_BR_LINE

    ik_compare_result_t *result = talloc_zero(ctx, ik_compare_result_t);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Compare content block counts */
    if (resp_a->content_count != resp_b->content_count) {
        result->matches = false;
        result->diff_message = talloc_asprintf(result,
                                               "Content block count mismatch: %zu vs %zu",
                                               resp_a->content_count, resp_b->content_count);
        return result;
    }

    /* Compare each content block */
    for (size_t i = 0; i < resp_a->content_count; i++) {
        const ik_content_block_t *block_a = &resp_a->content_blocks[i];
        const ik_content_block_t *block_b = &resp_b->content_blocks[i];

        /* Type must match */
        if (block_a->type != block_b->type) {
            result->matches = false;
            result->diff_message = talloc_asprintf(result,
                                                   "Content block %zu type mismatch: %d vs %d",
                                                   i, block_a->type, block_b->type);
            return result;
        }

        /* Compare based on type */
        switch (block_a->type) {
            case IK_CONTENT_TEXT: {
                const char *text_a = block_a->data.text.text;
                const char *text_b = block_b->data.text.text;

                if (strcmp(text_a, text_b) != 0) {
                    result->matches = false;
                    result->diff_message = talloc_asprintf(result,
                                                           "Text content mismatch at block %zu:\nA: %s\nB: %s",
                                                           i, text_a, text_b);
                    return result;
                }
                break;
            }

            case IK_CONTENT_TOOL_CALL: {
                /* Tool name must match exactly */
                const char *name_a = block_a->data.tool_call.name;
                const char *name_b = block_b->data.tool_call.name;

                if (strcmp(name_a, name_b) != 0) {
                    result->matches = false;
                    result->diff_message = talloc_asprintf(result,
                                                           "Tool call name mismatch at block %zu: %s vs %s",
                                                           i, name_a, name_b);
                    return result;
                }

                /* Tool arguments must be JSON-equivalent */
                const char *args_a = block_a->data.tool_call.arguments;
                const char *args_b = block_b->data.tool_call.arguments;

                ik_compare_result_t *json_cmp = ik_compare_json_equivalent(result, args_a, args_b);
                if (!json_cmp->matches) {
                    result->matches = false;
                    result->diff_message = talloc_asprintf(result,
                                                           "Tool call arguments mismatch at block %zu: %s",
                                                           i, json_cmp->diff_message);
                    return result;
                }

                /* Tool call ID pattern may differ - don't compare IDs */
                break;
            }

            case IK_CONTENT_THINKING: {
                const char *text_a = block_a->data.thinking.text;
                const char *text_b = block_b->data.thinking.text;

                if (strcmp(text_a, text_b) != 0) {
                    result->matches = false;
                    result->diff_message = talloc_asprintf(result,
                                                           "Thinking content mismatch at block %zu",
                                                           i);
                    return result;
                }
                break;
            }

            case IK_CONTENT_TOOL_RESULT:
                /* Tool results shouldn't appear in responses (only in requests) */
                result->matches = false;
                result->diff_message = talloc_asprintf(result,
                                                       "Unexpected tool result in response at block %zu", i);
                return result;
        }
    }

    /* Compare finish reason */
    if (resp_a->finish_reason != resp_b->finish_reason) {
        result->matches = false;
        result->diff_message = talloc_asprintf(result,
                                               "Finish reason mismatch: %d vs %d",
                                               resp_a->finish_reason, resp_b->finish_reason);
        return result;
    }

    /* Compare token usage with tolerance */
    if (!ik_compare_token_usage_tolerant(resp_a->usage.input_tokens,
                                         resp_b->usage.input_tokens)) {
        result->matches = false;
        result->diff_message = talloc_asprintf(result,
                                               "Input token count mismatch: %d vs %d (>5%% difference)",
                                               resp_a->usage.input_tokens, resp_b->usage.input_tokens);
        return result;
    }

    if (!ik_compare_token_usage_tolerant(resp_a->usage.output_tokens,
                                         resp_b->usage.output_tokens)) {
        result->matches = false;
        result->diff_message = talloc_asprintf(result,
                                               "Output token count mismatch: %d vs %d (>5%% difference)",
                                               resp_a->usage.output_tokens, resp_b->usage.output_tokens);
        return result;
    }

    /* Compare model (if both are set) */
    if (resp_a->model != NULL && resp_b->model != NULL) {
        if (strcmp(resp_a->model, resp_b->model) != 0) {
            result->matches = false;
            result->diff_message = talloc_asprintf(result,
                                                   "Model mismatch: %s vs %s",
                                                   resp_a->model, resp_b->model);
            return result;
        }
    }

    /* All checks passed */
    result->matches = true;
    result->diff_message = NULL;

    return result;
}

/* ================================================================
 * Stream Event Comparison
 * ================================================================ */

ik_compare_result_t *ik_compare_stream_events(TALLOC_CTX *ctx,
                                              const ik_stream_event_array_t *events_a,
                                              const ik_stream_event_array_t *events_b)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(events_a != NULL);  // LCOV_EXCL_BR_LINE
    assert(events_b != NULL);  // LCOV_EXCL_BR_LINE

    ik_compare_result_t *result = talloc_zero(ctx, ik_compare_result_t);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Compare event counts */
    if (events_a->count != events_b->count) {
        result->matches = false;
        result->diff_message = talloc_asprintf(result,
                                               "Event count mismatch: %zu vs %zu",
                                               events_a->count, events_b->count);
        return result;
    }

    /* Compare each event */
    for (size_t i = 0; i < events_a->count; i++) {
        const ik_stream_event_t *event_a = &events_a->events[i];
        const ik_stream_event_t *event_b = &events_b->events[i];

        /* Event type must match */
        if (event_a->type != event_b->type) {
            result->matches = false;
            result->diff_message = talloc_asprintf(result,
                                                   "Event %zu type mismatch: %d vs %d",
                                                   i,
                                                   event_a->type,
                                                   event_b->type);
            return result;
        }

        /* Compare event-specific data */
        switch (event_a->type) {
            case IK_STREAM_START:
                /* Model should match if both set */
                if (event_a->data.start.model != NULL && event_b->data.start.model != NULL) {
                    if (strcmp(event_a->data.start.model, event_b->data.start.model) != 0) {
                        result->matches = false;
                        result->diff_message = talloc_asprintf(result,
                                                               "START event model mismatch at %zu: %s vs %s",
                                                               i, event_a->data.start.model, event_b->data.start.model);
                        return result;
                    }
                }
                break;

            case IK_STREAM_TEXT_DELTA:
            case IK_STREAM_THINKING_DELTA:
                /* Text deltas should match exactly */
                if (strcmp(event_a->data.delta.text, event_b->data.delta.text) != 0) {
                    result->matches = false;
                    result->diff_message = talloc_asprintf(result,
                                                           "Delta text mismatch at event %zu", i);
                    return result;
                }
                break;

            case IK_STREAM_TOOL_CALL_START:
                /* Tool name should match */
                if (strcmp(event_a->data.tool_start.name, event_b->data.tool_start.name) != 0) {
                    result->matches = false;
                    result->diff_message = talloc_asprintf(result,
                                                           "Tool call name mismatch at event %zu: %s vs %s",
                                                           i,
                                                           event_a->data.tool_start.name,
                                                           event_b->data.tool_start.name);
                    return result;
                }
                /* ID may differ - don't compare */
                break;

            case IK_STREAM_TOOL_CALL_DELTA:
                /* Argument deltas should match exactly */
                if (strcmp(event_a->data.tool_delta.arguments, event_b->data.tool_delta.arguments) != 0) {
                    result->matches = false;
                    result->diff_message = talloc_asprintf(result,
                                                           "Tool call delta mismatch at event %zu", i);
                    return result;
                }
                break;

            case IK_STREAM_TOOL_CALL_DONE:
                /* No data to compare */
                break;

            case IK_STREAM_DONE:
                /* Finish reason should match */
                if (event_a->data.done.finish_reason != event_b->data.done.finish_reason) {
                    result->matches = false;
                    result->diff_message = talloc_asprintf(result,
                                                           "DONE event finish_reason mismatch at %zu: %d vs %d",
                                                           i,
                                                           event_a->data.done.finish_reason,
                                                           event_b->data.done.finish_reason);
                    return result;
                }

                /* Token usage with tolerance */
                if (!ik_compare_token_usage_tolerant(event_a->data.done.usage.input_tokens,
                                                     event_b->data.done.usage.input_tokens)) {
                    result->matches = false;
                    result->diff_message = talloc_asprintf(result,
                                                           "DONE event input_tokens mismatch at %zu", i);
                    return result;
                }
                break;

            case IK_STREAM_ERROR:
                /* Error category should match */
                if (event_a->data.error.category != event_b->data.error.category) {
                    result->matches = false;
                    result->diff_message = talloc_asprintf(result,
                                                           "ERROR event category mismatch at %zu: %d vs %d",
                                                           i, event_a->data.error.category,
                                                           event_b->data.error.category);
                    return result;
                }
                break;
        }
    }

    /* All events match */
    result->matches = true;
    result->diff_message = NULL;

    return result;
}
