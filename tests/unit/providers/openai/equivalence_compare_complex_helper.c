/**
 * @file equivalence_compare_complex.c
 * @brief Complex comparison functions (responses, stream events)
 */

#include "equivalence_compare.h"
#include "panic.h"
#include <string.h>
#include <assert.h>

/* ================================================================
 * Response Comparison
 * ================================================================ */

/**
 * Compare a single content block
 *
 * @return NULL if blocks match, error message otherwise
 */
static char *compare_content_block(TALLOC_CTX *ctx,
                                   const ik_content_block_t *block_a,
                                   const ik_content_block_t *block_b,
                                   size_t index)
{
    /* Type must match */
    if (block_a->type != block_b->type) {
        return talloc_asprintf(ctx, "Content block %zu type mismatch: %d vs %d",
                               index, block_a->type, block_b->type);
    }

    /* Compare based on type */
    switch (block_a->type) {
        case IK_CONTENT_TEXT: {
            const char *text_a = block_a->data.text.text;
            const char *text_b = block_b->data.text.text;

            if (strcmp(text_a, text_b) != 0) {
                return talloc_asprintf(ctx, "Text content mismatch at block %zu:\nA: %s\nB: %s",
                                       index, text_a, text_b);
            }
            break;
        }

        case IK_CONTENT_TOOL_CALL: {
            /* Tool name must match exactly */
            const char *name_a = block_a->data.tool_call.name;
            const char *name_b = block_b->data.tool_call.name;

            if (strcmp(name_a, name_b) != 0) {
                return talloc_asprintf(ctx, "Tool call name mismatch at block %zu: %s vs %s",
                                       index, name_a, name_b);
            }

            /* Tool arguments must be JSON-equivalent */
            const char *args_a = block_a->data.tool_call.arguments;
            const char *args_b = block_b->data.tool_call.arguments;

            ik_compare_result_t *json_cmp = ik_compare_json_equivalent(ctx, args_a, args_b);
            if (!json_cmp->matches) {
                return talloc_asprintf(ctx, "Tool call arguments mismatch at block %zu: %s",
                                       index, json_cmp->diff_message);
            }

            /* Tool call ID pattern may differ - don't compare IDs */
            break;
        }

        case IK_CONTENT_THINKING: {
            const char *text_a = block_a->data.thinking.text;
            const char *text_b = block_b->data.thinking.text;

            if (strcmp(text_a, text_b) != 0) {
                return talloc_asprintf(ctx, "Thinking content mismatch at block %zu", index);
            }
            break;
        }

        case IK_CONTENT_REDACTED_THINKING:
            /* Both blocks are redacted thinking - types match, so equal */
            break;

        case IK_CONTENT_TOOL_RESULT:
            /* Tool results shouldn't appear in responses (only in requests) */
            return talloc_asprintf(ctx, "Unexpected tool result in response at block %zu", index);
    }

    return NULL;  /* Blocks match */
}

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

        char *error = compare_content_block(result, block_a, block_b, i);
        if (error != NULL) {
            result->matches = false;
            result->diff_message = error;
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

/**
 * Compare a single stream event
 *
 * @return NULL if events match, error message otherwise
 */
static char *compare_stream_event(TALLOC_CTX *ctx,
                                  const ik_stream_event_t *event_a,
                                  const ik_stream_event_t *event_b,
                                  size_t index)
{
    /* Event type must match */
    if (event_a->type != event_b->type) {
        return talloc_asprintf(ctx, "Event %zu type mismatch: %d vs %d",
                               index, event_a->type, event_b->type);
    }

    /* Compare event-specific data */
    switch (event_a->type) {
        case IK_STREAM_START:
            /* Model should match if both set */
            if (event_a->data.start.model != NULL && event_b->data.start.model != NULL) {
                if (strcmp(event_a->data.start.model, event_b->data.start.model) != 0) {
                    return talloc_asprintf(ctx,
                                           "START event model mismatch at %zu: %s vs %s",
                                           index, event_a->data.start.model, event_b->data.start.model);
                }
            }
            break;

        case IK_STREAM_TEXT_DELTA:
        case IK_STREAM_THINKING_DELTA:
            /* Text deltas should match exactly */
            if (strcmp(event_a->data.delta.text, event_b->data.delta.text) != 0) {
                return talloc_asprintf(ctx, "Delta text mismatch at event %zu", index);
            }
            break;

        case IK_STREAM_TOOL_CALL_START:
            /* Tool name should match */
            if (strcmp(event_a->data.tool_start.name, event_b->data.tool_start.name) != 0) {
                return talloc_asprintf(ctx,
                                       "Tool call name mismatch at event %zu: %s vs %s",
                                       index,
                                       event_a->data.tool_start.name,
                                       event_b->data.tool_start.name);
            }
            /* ID may differ - don't compare */
            break;

        case IK_STREAM_TOOL_CALL_DELTA:
            /* Argument deltas should match exactly */
            if (strcmp(event_a->data.tool_delta.arguments, event_b->data.tool_delta.arguments) != 0) {
                return talloc_asprintf(ctx, "Tool call delta mismatch at event %zu", index);
            }
            break;

        case IK_STREAM_TOOL_CALL_DONE:
            /* No data to compare */
            break;

        case IK_STREAM_DONE:
            /* Finish reason should match */
            if (event_a->data.done.finish_reason != event_b->data.done.finish_reason) {
                return talloc_asprintf(ctx,
                                       "DONE event finish_reason mismatch at %zu: %d vs %d",
                                       index,
                                       event_a->data.done.finish_reason,
                                       event_b->data.done.finish_reason);
            }

            /* Token usage with tolerance */
            if (!ik_compare_token_usage_tolerant(event_a->data.done.usage.input_tokens,
                                                 event_b->data.done.usage.input_tokens)) {
                return talloc_asprintf(ctx, "DONE event input_tokens mismatch at %zu", index);
            }
            break;

        case IK_STREAM_ERROR:
            /* Error category should match */
            if (event_a->data.error.category != event_b->data.error.category) {
                return talloc_asprintf(ctx,
                                       "ERROR event category mismatch at %zu: %d vs %d",
                                       index, event_a->data.error.category,
                                       event_b->data.error.category);
            }
            break;
    }

    return NULL;  /* Events match */
}

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

        char *error = compare_stream_event(result, event_a, event_b, i);
        if (error != NULL) {
            result->matches = false;
            result->diff_message = error;
            return result;
        }
    }

    /* All events match */
    result->matches = true;
    result->diff_message = NULL;

    return result;
}
