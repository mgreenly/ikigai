/**
 * @file streaming_responses_internal.h
 * @brief Internal definitions for OpenAI Responses API streaming
 */

#ifndef IK_PROVIDERS_OPENAI_STREAMING_RESPONSES_INTERNAL_H
#define IK_PROVIDERS_OPENAI_STREAMING_RESPONSES_INTERNAL_H

#include "streaming.h"
#include "providers/common/sse_parser.h"

/* Forward declarations */
typedef struct yyjson_val yyjson_val;

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

/**
 * Parse usage object from JSON into ik_usage_t structure
 */
void ik_openai_responses_parse_usage(yyjson_val *usage_val, ik_usage_t *out_usage);

/**
 * Map OpenAI Responses API status to finish reason
 */
ik_finish_reason_t ik_openai_map_responses_status(const char *status, const char *incomplete_reason);

/**
 * Event emission and control helpers
 */
void ik_openai_emit_event(ik_openai_responses_stream_ctx_t *sctx, const ik_stream_event_t *event);
void ik_openai_maybe_emit_start(ik_openai_responses_stream_ctx_t *sctx);
void ik_openai_maybe_end_tool_call(ik_openai_responses_stream_ctx_t *sctx);

/**
 * Event handler functions
 */
void ik_openai_responses_handle_response_created(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
void ik_openai_responses_handle_output_text_delta(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
void ik_openai_responses_handle_reasoning_summary_text_delta(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
void ik_openai_responses_handle_output_item_added(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
void ik_openai_responses_handle_function_call_arguments_delta(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
void ik_openai_responses_handle_output_item_done(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
void ik_openai_responses_handle_response_completed(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
void ik_openai_responses_handle_error_event(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);

#endif /* IK_PROVIDERS_OPENAI_STREAMING_RESPONSES_INTERNAL_H */
