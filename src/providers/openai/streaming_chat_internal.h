/**
 * @file streaming_chat_internal.h
 * @brief OpenAI Chat Completions streaming internal definitions
 */

#ifndef IK_PROVIDERS_OPENAI_STREAMING_CHAT_INTERNAL_H
#define IK_PROVIDERS_OPENAI_STREAMING_CHAT_INTERNAL_H

#include "streaming.h"
#include <inttypes.h>
#include <stdbool.h>

/**
 * OpenAI Chat Completions streaming context structure
 */
struct ik_openai_chat_stream_ctx {
    ik_stream_cb_t stream_cb;          /* User's stream callback */
    void *stream_ctx;                  /* User's stream context */
    char *model;                       /* Model name from first chunk */
    ik_finish_reason_t finish_reason;  /* Finish reason from choice */
    ik_usage_t usage;                  /* Accumulated usage statistics */
    bool started;                      /* Whether IK_STREAM_START was emitted */
    bool in_tool_call;                 /* Whether currently in a tool call */
    int32_t tool_call_index;           /* Current tool call index */
    char *current_tool_id;             /* Current tool call ID */
    char *current_tool_name;           /* Current tool call name */
    char *current_tool_args;           /* Accumulated tool arguments */
};

/**
 * Process choices[0].delta object from Chat Completions API
 *
 * @param sctx Streaming context
 * @param delta Delta object from JSON
 * @param finish_reason_str Finish reason string (or NULL)
 *
 * Handles:
 * - Role extraction (first chunk)
 * - Content deltas (text)
 * - Tool call deltas (id, name, arguments)
 * - Finish reason updates
 */
void ik_openai_chat_process_delta(ik_openai_chat_stream_ctx_t *sctx, void *delta, const char *finish_reason_str);

/**
 * Emit IK_STREAM_START if not yet started
 *
 * @param sctx Streaming context
 */
void ik_openai_chat_maybe_emit_start(ik_openai_chat_stream_ctx_t *sctx);

/**
 * Emit IK_STREAM_TOOL_CALL_DONE if in a tool call
 *
 * @param sctx Streaming context
 */
void ik_openai_chat_maybe_end_tool_call(ik_openai_chat_stream_ctx_t *sctx);

#endif /* IK_PROVIDERS_OPENAI_STREAMING_CHAT_INTERNAL_H */
