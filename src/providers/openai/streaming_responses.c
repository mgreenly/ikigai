/**
 * @file streaming_responses.c
 * @brief OpenAI Responses API streaming implementation
 */

#include "streaming.h"

#include "debug_log.h"
#include "panic.h"
#include "streaming_responses_internal.h"

#include <assert.h>
#include <string.h>

/* ================================================================
 * Forward Declarations
 * ================================================================ */

static void sse_event_handler(const char *event_name, const char *data, size_t len, void *user_ctx);

/* ================================================================
 * Context Creation
 * ================================================================ */

ik_openai_responses_stream_ctx_t *ik_openai_responses_stream_ctx_create(TALLOC_CTX *ctx,
                                                                        ik_stream_cb_t stream_cb,
                                                                        void *stream_ctx)
{
    DEBUG_LOG("responses_stream_ctx_create: ctx=%p stream_cb=%p stream_ctx=%p",
              (void *)ctx, (void *)stream_cb, stream_ctx);

    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(stream_cb != NULL);  // LCOV_EXCL_BR_LINE

    ik_openai_responses_stream_ctx_t *sctx = talloc_zero(ctx, ik_openai_responses_stream_ctx_t);
    if (sctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    sctx->stream_cb = stream_cb;
    sctx->stream_ctx = stream_ctx;

    sctx->model = NULL;
    sctx->finish_reason = IK_FINISH_UNKNOWN;
    memset(&sctx->usage, 0, sizeof(ik_usage_t));
    sctx->started = false;
    sctx->in_tool_call = false;
    sctx->tool_call_index = -1;
    sctx->current_tool_id = NULL;
    sctx->current_tool_name = NULL;
    sctx->current_tool_args = NULL;

    sctx->sse_parser = ik_sse_parser_create(sctx);
    if (sctx->sse_parser == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    DEBUG_LOG("responses_stream_ctx_create: created sctx=%p sse_parser=%p",
              (void *)sctx, (void *)sctx->sse_parser);

    return sctx;
}

/* ================================================================
 * Getters
 * ================================================================ */

ik_usage_t ik_openai_responses_stream_get_usage(ik_openai_responses_stream_ctx_t *stream_ctx)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    return stream_ctx->usage;
}

ik_finish_reason_t ik_openai_responses_stream_get_finish_reason(ik_openai_responses_stream_ctx_t *stream_ctx)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    return stream_ctx->finish_reason;
}

/* ================================================================
 * SSE Parser Callback
 * ================================================================ */

/**
 * SSE event handler - routes events to process_event
 */
static void sse_event_handler(const char *event_name, const char *data, size_t len, void *user_ctx)
{
    (void)len;

    assert(user_ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(event_name != NULL); // LCOV_EXCL_BR_LINE
    assert(data != NULL);       // LCOV_EXCL_BR_LINE

    ik_openai_responses_stream_ctx_t *stream_ctx = (ik_openai_responses_stream_ctx_t *)user_ctx;
    assert(stream_ctx->stream_cb != NULL); // LCOV_EXCL_BR_LINE

    DEBUG_LOG("sse_event_handler: event='%s' data_len=%zu calling process_event",
              event_name, strlen(data));
    ik_openai_responses_stream_process_event(stream_ctx, event_name, data);
}

/* ================================================================
 * Write Callback
 * ================================================================ */

/**
 * Curl write callback for Responses API streaming
 */
size_t ik_openai_responses_stream_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    assert(userdata != NULL); // LCOV_EXCL_BR_LINE

    ik_openai_responses_stream_ctx_t *ctx = (ik_openai_responses_stream_ctx_t *)userdata;
    assert(ctx->sse_parser != NULL); // LCOV_EXCL_BR_LINE
    assert(ctx->stream_cb != NULL);  // LCOV_EXCL_BR_LINE

    size_t total = size * nmemb;
    DEBUG_LOG("responses_write_callback: feeding %zu bytes to SSE parser", total);

    ik_sse_parser_feed(ctx->sse_parser, (const char *)ptr, total);

    TALLOC_CTX *tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_sse_event_t *event;
    int event_count = 0;
    while ((event = ik_sse_parser_next(ctx->sse_parser, tmp_ctx)) != NULL) {
        event_count++;
        DEBUG_LOG("responses_write_callback: event #%d type='%s' data_len=%zu",
                  event_count,
                  event->event ? event->event : "(null)",
                  event->data ? strlen(event->data) : 0);

        if (event->event != NULL && event->data != NULL) {
            sse_event_handler(event->event, event->data, strlen(event->data), ctx);
        }
        talloc_free(event);
    }

    DEBUG_LOG("responses_write_callback: processed %d events, returning %zu", event_count, total);
    talloc_free(tmp_ctx);

    return total;
}

/* ================================================================
 * Response Builder
 * ================================================================ */

ik_response_t *ik_openai_responses_stream_build_response(TALLOC_CTX *ctx,
                                                         ik_openai_responses_stream_ctx_t *sctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE

    // Allocate response structure
    ik_response_t *resp = talloc_zero(ctx,
                                      ik_response_t);
    if (resp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Copy model
    if (sctx->model != NULL) {
        resp->model = talloc_strdup(resp, sctx->model);
        if (resp->model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    // Copy finish reason and usage
    resp->finish_reason = sctx->finish_reason;
    resp->usage = sctx->usage;

    // Check if we have a tool call to include
    if (sctx->current_tool_id != NULL && sctx->current_tool_name != NULL) {
        // Override finish_reason: Responses API returns "completed" even for tool calls,
        // but we need IK_FINISH_TOOL_USE so the tool loop continues
        resp->finish_reason = IK_FINISH_TOOL_USE;

        // Allocate content blocks array with one tool call (zero-init to avoid garbage pointers)
        resp->content_blocks = talloc_zero_array(resp, ik_content_block_t, 1);
        if (resp->content_blocks == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        resp->content_count = 1;

        // Populate tool call content block
        ik_content_block_t *block = &resp->content_blocks[0];
        block->type = IK_CONTENT_TOOL_CALL;
        block->data.tool_call.id = talloc_strdup(resp->content_blocks, sctx->current_tool_id);
        if (block->data.tool_call.id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        block->data.tool_call.name = talloc_strdup(resp->content_blocks, sctx->current_tool_name);
        if (block->data.tool_call.name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        block->data.tool_call.arguments = talloc_strdup(resp->content_blocks,
                                                        sctx->current_tool_args !=
                                                        NULL ? sctx->current_tool_args : "{}");
        if (block->data.tool_call.arguments == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        // thought_signature is NULL (zero-initialized) - Responses API doesn't provide it
    } else {
        // No tool call - empty content
        resp->content_blocks = NULL;
        resp->content_count = 0;
    }

    return resp;
}
