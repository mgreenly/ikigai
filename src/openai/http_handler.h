#ifndef IK_OPENAI_HTTP_HANDLER_H
#define IK_OPENAI_HTTP_HANDLER_H

#include "openai/client.h"
#include "error.h"
#include "tool.h"

/**
 * HTTP response structure
 *
 * Holds both content and metadata from streaming response.
 */
typedef struct {
    char *content;          /* Complete response content */
    char *finish_reason;    /* Finish reason (may be NULL) */
    ik_tool_call_t *tool_call; /* Tool call if present (NULL otherwise) */
} ik_openai_http_response_t;

/**
 * Perform HTTP POST request to OpenAI API
 *
 * Handles libcurl operations, SSE streaming, and finish_reason extraction.
 *
 * @param parent       Talloc context parent
 * @param url          API endpoint URL
 * @param api_key      OpenAI API key
 * @param request_body JSON request body
 * @param stream_cb    Streaming callback (or NULL)
 * @param cb_ctx       Callback context
 * @return             OK(ik_openai_http_response_t*) or ERR(...)
 */
res_t ik_openai_http_post(void *parent, const char *url, const char *api_key,
                          const char *request_body,
                          ik_openai_stream_cb_t stream_cb, void *cb_ctx);

#endif /* IK_OPENAI_HTTP_HANDLER_H */
