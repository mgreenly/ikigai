// Internal HTTP callback handlers for OpenAI multi client
#pragma once

#include <curl/curl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include "error.h"
#include "openai/sse_parser.h"
#include "openai/client.h"
#include "tool.h"

/*
 * Context for HTTP write callback
 *
 * Accumulates response data and handles streaming via SSE parser.
 */
typedef struct {
    ik_openai_sse_parser_t *parser;     /* SSE parser for streaming responses */
    ik_openai_stream_cb_t user_callback; /* User's streaming callback */
    void *user_ctx;                      /* User's callback context */
    char *complete_response;             /* Accumulated complete response */
    size_t response_len;                 /* Length of complete response */
    bool has_error;                      /* Whether an error occurred */
    char *model;                         /* Model name from SSE stream */
    char *finish_reason;                 /* Finish reason from SSE stream */
    int32_t completion_tokens;           /* Completion token count from SSE stream */
    ik_tool_call_t *tool_call;           /* Tool call if present (NULL otherwise) */
} http_write_ctx_t;

/**
 * @brief libcurl write callback
 *
 * Called by libcurl as data arrives from the server.
 * Feeds data to SSE parser and invokes user callback for each content chunk.
 *
 * @param data     Data received from server
 * @param size     Size multiplier
 * @param nmemb    Number of members
 * @param userdata http_write_ctx_t pointer
 * @return Number of bytes processed
 */
size_t http_write_callback(char *data, size_t size, size_t nmemb, void *userdata);

