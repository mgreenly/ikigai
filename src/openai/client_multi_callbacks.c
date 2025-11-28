// Internal HTTP callback handlers for OpenAI multi client
#include "openai/client_multi_callbacks.h"
#include "openai/sse_parser.h"
#include "panic.h"

#include <assert.h>
#include <talloc.h>

/*
 * libcurl write callback
 *
 * Called by libcurl as data arrives from the server.
 * Feeds data to SSE parser and invokes user callback for each content chunk.
 */
size_t http_write_callback(char *data, size_t size, size_t nmemb, void *userdata) {
    http_write_ctx_t *ctx = (http_write_ctx_t *)userdata;
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    size_t total_size = size * nmemb;

    /* Feed data to SSE parser */
    ik_openai_sse_parser_feed(ctx->parser, data, total_size);

    /* Extract and process all complete SSE events */
    while (true) {
        char *event = ik_openai_sse_parser_get_event(ctx->parser);
        if (event == NULL) {
            break; /* No more complete events */
        }

        /* Parse SSE event to extract content */
        res_t content_res = ik_openai_parse_sse_event(ctx->parser, event);
        if (content_res.is_err) {
            /* Parse error - log but continue */
            talloc_free(event);
            continue;
        }

        char *content = content_res.ok;
        if (content != NULL) {
            /* Invoke user's streaming callback if provided */
            if (ctx->user_callback != NULL) {
                res_t cb_res = ctx->user_callback(content, ctx->user_ctx);
                if (cb_res.is_err) {
                    ctx->has_error = true;
                    talloc_free(content);
                    talloc_free(event);
                    return 0;
                }
            }

            /* Accumulate to complete response */
            if (ctx->complete_response == NULL) {
                ctx->complete_response = talloc_strdup(ctx->parser, content);
            } else {
                ctx->complete_response = talloc_strdup_append(ctx->complete_response, content);
            }

            if (ctx->complete_response == NULL) {  // LCOV_EXCL_BR_LINE
                PANIC("Failed to accumulate response");  // LCOV_EXCL_LINE
            }

            talloc_free(content);
        }

        talloc_free(event);
    }

    return total_size;
}
