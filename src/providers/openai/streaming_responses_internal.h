/**
 * @file streaming_responses_internal.h
 * @brief Internal definitions for OpenAI Responses API streaming
 */

#ifndef IK_PROVIDERS_OPENAI_STREAMING_RESPONSES_INTERNAL_H
#define IK_PROVIDERS_OPENAI_STREAMING_RESPONSES_INTERNAL_H

#include "streaming.h"
#include "providers/common/sse_parser.h"

/**
 * OpenAI Responses API streaming context structure
 */
struct ik_openai_responses_stream_ctx {
    ik_stream_cb_t stream_cb;          /* User's stream callback */
    void *stream_ctx;                  /* User's stream context */
    char *model;                       /* Model name from response.created */
    ik_finish_reason_t finish_reason;  /* Finish reason from status */
    ik_usage_t usage;                  /* Accumulated usage statistics */
    bool started;                      /* Whether IK_STREAM_START was emitted */
    bool in_tool_call;                 /* Whether currently in a tool call */
    int32_t tool_call_index;           /* Current tool call index (output_index) */
    char *current_tool_id;             /* Current tool call ID */
    char *current_tool_name;           /* Current tool call name */
    char *current_tool_args;           /* Accumulated tool call arguments */
    ik_sse_parser_t *sse_parser;       /* SSE parser for processing chunks */
};

#endif /* IK_PROVIDERS_OPENAI_STREAMING_RESPONSES_INTERNAL_H */
