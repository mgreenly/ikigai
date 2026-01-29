/**
 * @file streaming_internal.h
 * @brief Google streaming internal definitions (shared between streaming.c and streaming_helpers.c)
 */

#ifndef IK_PROVIDERS_GOOGLE_STREAMING_INTERNAL_H
#define IK_PROVIDERS_GOOGLE_STREAMING_INTERNAL_H

#include "streaming.h"

/**
 * Google streaming context structure
 */
struct ik_google_stream_ctx {
    ik_stream_cb_t user_cb;            /* User's stream callback */
    void *user_ctx;                    /* User's stream context */
    char *model;                       /* Model name from modelVersion */
    ik_finish_reason_t finish_reason;  /* Finish reason from finishReason */
    ik_usage_t usage;                  /* Accumulated usage statistics */
    bool started;                      /* true after IK_STREAM_START emitted */
    bool in_thinking;                  /* true when processing thinking content */
    bool in_tool_call;                 /* true when processing tool call */
    char *current_tool_id;             /* Current tool call ID (generated) */
    char *current_tool_name;           /* Current tool call name */
    char *current_tool_args;           /* Accumulated tool call arguments (JSON) */
    char *current_tool_thought_sig;    /* Thought signature for current tool call (Gemini 3 only) */
    int32_t part_index;                /* Current part index for events */
};

#endif /* IK_PROVIDERS_GOOGLE_STREAMING_INTERNAL_H */
