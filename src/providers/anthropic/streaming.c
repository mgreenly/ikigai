/**
 * @file streaming.c
 * @brief Anthropic streaming implementation
 */

#include "streaming.h"
#include "streaming_events.h"
#include "response.h"
#include "request.h"
#include "json_allocator.h"
#include "panic.h"
#include "providers/common/http_multi.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>

/* ================================================================
 * Context Creation
 * ================================================================ */

res_t ik_anthropic_stream_ctx_create(TALLOC_CTX *ctx, ik_stream_cb_t stream_cb,
                                      void *stream_ctx, ik_anthropic_stream_ctx_t **out)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(stream_cb != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);        // LCOV_EXCL_BR_LINE

    // Allocate streaming context
    ik_anthropic_stream_ctx_t *sctx = talloc_zero(ctx, ik_anthropic_stream_ctx_t);
    if (sctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Store user callbacks
    sctx->stream_cb = stream_cb;
    sctx->stream_ctx = stream_ctx;

    // Create SSE parser with event callback
    sctx->sse_parser = ik_sse_parser_create(sctx);
    if (sctx->sse_parser == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Initialize state
    sctx->model = NULL;
    sctx->finish_reason = IK_FINISH_UNKNOWN;
    memset(&sctx->usage, 0, sizeof(ik_usage_t));
    sctx->current_block_index = -1;
    sctx->current_block_type = IK_CONTENT_TEXT;
    sctx->current_tool_id = NULL;
    sctx->current_tool_name = NULL;

    *out = sctx;
    return OK(sctx);
}

/* ================================================================
 * Main Event Processing
 * ================================================================ */

void ik_anthropic_stream_process_event(ik_anthropic_stream_ctx_t *stream_ctx,
                                        const char *event, const char *data)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(event != NULL);      // LCOV_EXCL_BR_LINE
    assert(data != NULL);       // LCOV_EXCL_BR_LINE

    // Ignore ping events
    if (strcmp(event, "ping") == 0) {
        return;
    }

    // Parse JSON data
    yyjson_alc allocator = ik_make_talloc_allocator(stream_ctx);
    yyjson_doc *doc = yyjson_read_opts((char *)(void *)(size_t)(const void *)data,
                                        strlen(data), 0, &allocator, NULL);
    if (doc == NULL) {
        // Invalid JSON - emit error
        ik_stream_event_t error_event = {
            .type = IK_STREAM_ERROR,
            .index = 0,
            .data.error.category = IK_ERR_CAT_UNKNOWN,
            .data.error.message = "Invalid JSON in SSE event"
        };
        stream_ctx->stream_cb(&error_event, stream_ctx->stream_ctx);
        return;
    }

    yyjson_val *root = yyjson_doc_get_root(doc); // LCOV_EXCL_BR_LINE
    if (!yyjson_is_obj(root)) {
        // Not an object - emit error
        ik_stream_event_t error_event = {
            .type = IK_STREAM_ERROR,
            .index = 0,
            .data.error.category = IK_ERR_CAT_UNKNOWN,
            .data.error.message = "SSE event data is not a JSON object"
        };
        stream_ctx->stream_cb(&error_event, stream_ctx->stream_ctx);
        return;
    }

    // Dispatch based on event type
    if (strcmp(event, "message_start") == 0) {
        ik_anthropic_process_message_start(stream_ctx, root);
    } else if (strcmp(event, "content_block_start") == 0) {
        ik_anthropic_process_content_block_start(stream_ctx, root);
    } else if (strcmp(event, "content_block_delta") == 0) {
        ik_anthropic_process_content_block_delta(stream_ctx, root);
    } else if (strcmp(event, "content_block_stop") == 0) {
        ik_anthropic_process_content_block_stop(stream_ctx, root);
    } else if (strcmp(event, "message_delta") == 0) {
        ik_anthropic_process_message_delta(stream_ctx, root);
    } else if (strcmp(event, "message_stop") == 0) {
        ik_anthropic_process_message_stop(stream_ctx, root);
    } else if (strcmp(event, "error") == 0) {
        ik_anthropic_process_error(stream_ctx, root);
    }
    // Unknown events are ignored
}
