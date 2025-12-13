#pragma once

#include "error.h"
#include "layer.h"
#include "layer_wrappers.h"
#include "scrollback.h"
#include "tool.h"

#include <talloc.h>
#include <stdbool.h>
#include <pthread.h>

// Forward declarations
typedef struct ik_shared_ctx ik_shared_ctx_t;
typedef struct ik_input_buffer_t ik_input_buffer_t;
typedef struct ik_openai_conversation ik_openai_conversation_t;
struct ik_openai_multi;

// Agent state machine
typedef enum {
    IK_AGENT_STATE_IDLE,              // Normal input mode
    IK_AGENT_STATE_WAITING_FOR_LLM,   // Waiting for LLM response (spinner visible)
    IK_AGENT_STATE_EXECUTING_TOOL     // Tool running in background thread
} ik_agent_state_t;

// Mark structure for conversation checkpoints
typedef struct {
    size_t message_index;     // Position in conversation at time of mark
    char *label;              // Optional user label (or NULL for unlabeled mark)
    char *timestamp;          // ISO 8601 timestamp
} ik_mark_t;

// Per-agent context - state specific to one agent
// Created as child of repl_ctx (owned by coordinator)
typedef struct ik_agent_ctx {
    // Identity (from agent-process-model.md)
    char *uuid;          // Internal unique identifier
    char *name;          // Optional human-friendly name (NULL if unnamed)
    char *parent_uuid;   // Parent agent's UUID (NULL for root agent)

    // Reference to shared infrastructure
    ik_shared_ctx_t *shared;

    // Display state (per-agent)
    ik_scrollback_t *scrollback;
    ik_layer_cake_t *layer_cake;
    ik_layer_t *scrollback_layer;
    ik_layer_t *spinner_layer;
    ik_layer_t *separator_layer;
    ik_layer_t *input_layer;
    ik_layer_t *completion_layer;

    // Viewport state
    size_t viewport_offset;

    // Spinner state (per-agent)
    ik_spinner_state_t spinner_state;

    // Input state (per-agent - preserves partial composition)
    ik_input_buffer_t *input_buffer;

    // Tab completion state (per-agent)
    ik_completion_t *completion;

    // Conversation state (per-agent)
    ik_openai_conversation_t *conversation;
    ik_mark_t **marks;
    size_t mark_count;

    // LLM interaction state (per-agent)
    struct ik_openai_multi *multi;
    int curl_still_running;
    ik_agent_state_t state;
    char *assistant_response;
    char *streaming_line_buffer;
    char *http_error_message;
    char *response_model;
    char *response_finish_reason;
    int32_t response_completion_tokens;

    // Layer reference fields (updated before each render)
    bool separator_visible;
    bool input_buffer_visible;
    const char *input_text;
    size_t input_text_len;

    // Tool execution state (per-agent)
    ik_tool_call_t *pending_tool_call;
    pthread_t tool_thread;
    pthread_mutex_t tool_thread_mutex;
    bool tool_thread_running;
    bool tool_thread_complete;
    TALLOC_CTX *tool_thread_ctx;
    char *tool_thread_result;
    int32_t tool_iteration_count;
} ik_agent_ctx_t;

// Create agent context
// ctx: talloc parent (repl_ctx)
// shared: shared infrastructure
// parent_uuid: parent agent's UUID (NULL for root agent)
// out: receives allocated agent context
res_t ik_agent_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                      const char *parent_uuid, ik_agent_ctx_t **out);

// Generate a new UUID as base64url string (helper function)
// ctx: talloc parent for the returned string
// Returns: newly allocated 22-character base64url UUID string
char *ik_agent_generate_uuid(TALLOC_CTX *ctx);
