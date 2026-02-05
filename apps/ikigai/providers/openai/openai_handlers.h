/**
 * @file openai_handlers.h
 * @brief OpenAI HTTP completion handlers
 */

#ifndef IK_OPENAI_HANDLERS_H
#define IK_OPENAI_HANDLERS_H

#include "apps/ikigai/providers/openai/openai.h"
#include "apps/ikigai/providers/common/http_multi.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/openai/streaming.h"
#include <inttypes.h>
#include <stdbool.h>

/**
 * Internal request context for tracking in-flight requests
 */
typedef struct {
    ik_openai_ctx_t *provider;
    bool use_responses_api;
    ik_provider_completion_cb_t cb;
    void *cb_ctx;
} ik_openai_request_ctx_t;

/**
 * Internal request context for tracking in-flight streaming requests
 */
typedef struct {
    ik_openai_ctx_t *provider;
    bool use_responses_api;
    ik_stream_cb_t stream_cb;
    void *stream_ctx;
    ik_provider_completion_cb_t completion_cb;
    void *completion_ctx;
    void *parser_ctx;  /* Chat: ik_openai_chat_stream_ctx_t*, Responses: ik_openai_responses_stream_ctx_t* */
    char *sse_buffer;
    size_t sse_buffer_len;
} ik_openai_stream_request_ctx_t;

/**
 * HTTP completion callback for non-streaming requests
 */
void ik_openai_http_completion_handler(const ik_http_completion_t *http_completion, void *user_ctx);

/**
 * curl write callback for SSE streaming
 */
size_t ik_openai_stream_write_callback(const char *data, size_t len, void *userdata);

/**
 * HTTP completion callback for streaming requests
 */
void ik_openai_stream_completion_handler(const ik_http_completion_t *http_completion, void *user_ctx);

#endif // IK_OPENAI_HANDLERS_H
