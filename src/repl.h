#pragma once

#include "error.h"
#include "terminal.h"
#include "render.h"
#include "input_buffer/core.h"
#include "input.h"
#include "scroll_accumulator.h"
#include "scrollback.h"
#include "layer.h"
#include "layer_wrappers.h"
#include "config.h"
#include "openai/client.h"
#include "debug_pipe.h"
#include "tool.h"
#include "db/connection.h"
#include "history.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <inttypes.h>

// Forward declarations
typedef struct ik_shared_ctx ik_shared_ctx_t;

// REPL state machine (Phase 1.6)
typedef enum {
    IK_REPL_STATE_IDLE,              // Normal input mode
    IK_REPL_STATE_WAITING_FOR_LLM,   // Waiting for LLM response (spinner visible)
    IK_REPL_STATE_EXECUTING_TOOL     // Tool running in background thread
} ik_repl_state_t;

// Mark structure for conversation checkpoints (Phase 1.7)
typedef struct {
    size_t message_index;     // Position in conversation at time of mark
    char *label;              // Optional user label (or NULL for unlabeled mark)
    char *timestamp;          // ISO 8601 timestamp
} ik_mark_t;

// Viewport boundaries for rendering (Phase 4)
typedef struct {
    size_t scrollback_start_line;   // First scrollback line to render
    size_t scrollback_lines_count;  // How many scrollback lines visible
    size_t input_buffer_start_row;     // Terminal row where input buffer begins
    bool separator_visible;         // Whether separator is in visible range
} ik_viewport_t;

// REPL context structure
typedef struct ik_repl_ctx_t {
    // Shared infrastructure (DI - not owned, just referenced)
    // See shared.h for what's available via this pointer
    ik_shared_ctx_t *shared;
    ik_input_buffer_t *input_buffer;  // Input buffer
    ik_input_parser_t *input_parser;  // Input parser
    ik_scrollback_t *scrollback;      // Scrollback buffer (Phase 4)
    size_t viewport_offset;           // Physical row offset for scrolling (0 = bottom)
    atomic_bool quit;           // Exit flag (atomic for thread safety)
    ik_scroll_accumulator_t *scroll_acc;  // Scroll accumulator (rel-05)

    // Layer-based rendering (Phase 1.3)
    ik_layer_cake_t *layer_cake;      // Layer cake manager
    ik_layer_t *scrollback_layer;     // Scrollback layer
    ik_layer_t *spinner_layer;        // Spinner layer (Phase 1.4)
    ik_layer_t *separator_layer;      // Separator layer (upper)
    ik_layer_t *input_layer;          // Input buffer layer
    ik_layer_t *lower_separator_layer; // Separator layer (lower) - below input
    ik_layer_t *completion_layer;     // Completion layer (rel-04)

    // Reference fields for layers (updated before each render)
    ik_spinner_state_t spinner_state; // Spinner state (Phase 1.4)
    bool separator_visible;           // Separator visibility flag (upper)
    bool lower_separator_visible;     // Separator visibility flag (lower)
    bool input_buffer_visible;        // Input buffer visibility flag
    const char *input_text;           // Input text pointer
    size_t input_text_len;            // Input text length

    // Event loop integration (Phase 1.6)
    struct ik_openai_multi *multi;    // curl_multi handle for non-blocking HTTP
    int curl_still_running;           // Number of active curl transfers
    ik_repl_state_t state;            // Current REPL state (IDLE or WAITING_FOR_LLM)

    // Configuration and conversation (Phase 1.6)
    ik_openai_conversation_t *conversation;       // Current conversation (session messages)
    char *assistant_response;                     // Accumulated assistant response (during streaming)
    char *streaming_line_buffer;                  // Buffer for incomplete line during streaming
    char *http_error_message;                     // HTTP error message (if request failed)
    char *response_model;                         // Model name from SSE stream (for database persistence)
    char *response_finish_reason;                 // Finish reason from SSE stream (for database persistence)
    int32_t response_completion_tokens;           // Completion token count from SSE stream (for database persistence)

    // Checkpoint management (Phase 1.7)
    ik_mark_t **marks;                            // Array of conversation marks
    size_t mark_count;                            // Number of marks

    // Tab completion (rel-04)
    ik_completion_t *completion;                  // Tab completion context (NULL when inactive)

    // Tool loop iteration tracking (Story 11)
    int32_t tool_iteration_count;     // Number of tool call iterations in current request

    // Pending tool call (Story 02)
    ik_tool_call_t *pending_tool_call; // Tool call awaiting execution (NULL if none)

    // Tool thread execution (async tool dispatch)
    pthread_t tool_thread;              // Worker thread handle
    pthread_mutex_t tool_thread_mutex;  // Protects tool_thread_* fields
    bool tool_thread_running;           // Thread is active
    bool tool_thread_complete;          // Thread finished, result ready
    TALLOC_CTX *tool_thread_ctx;        // Memory context for thread (owned by main)
    char *tool_thread_result;           // Result JSON from tool dispatch

    // Note: history removed - now in shared context (repl->shared->history)
} ik_repl_ctx_t;

// Initialize REPL context
res_t ik_repl_init(void *parent, ik_shared_ctx_t *shared, ik_repl_ctx_t **repl_out);

// Cleanup REPL context
void ik_repl_cleanup(ik_repl_ctx_t *repl);

// Run REPL event loop
res_t ik_repl_run(ik_repl_ctx_t *repl);

// Render current frame (input buffer only for now)
res_t ik_repl_render_frame(ik_repl_ctx_t *repl);

// Calculate viewport boundaries (Phase 4)
res_t ik_repl_calculate_viewport(ik_repl_ctx_t *repl, ik_viewport_t *viewport_out);

// Submit current input buffer line to scrollback (Phase 4 Task 4.6)
res_t ik_repl_submit_line(ik_repl_ctx_t *repl);

// Handle terminal resize (Bug #5)
res_t ik_repl_handle_resize(ik_repl_ctx_t *repl);

// State transition functions (Phase 1.6)
void ik_repl_transition_to_waiting_for_llm(ik_repl_ctx_t *repl);
void ik_repl_transition_to_idle(ik_repl_ctx_t *repl);
void ik_repl_transition_to_executing_tool(ik_repl_ctx_t *repl);
void ik_repl_transition_from_executing_tool(ik_repl_ctx_t *repl);

// Internal helper functions moved to repl_event_handlers.h

// Tool execution helper (exposed to reduce complexity in handle_request_success)
void ik_repl_execute_pending_tool(ik_repl_ctx_t *repl);

// Async tool execution (replaces synchronous ik_repl_execute_pending_tool)
void ik_repl_start_tool_execution(ik_repl_ctx_t *repl);
void ik_repl_complete_tool_execution(ik_repl_ctx_t *repl);

// Tool loop decision function (Phase 2: Story 02)
bool ik_repl_should_continue_tool_loop(const ik_repl_ctx_t *repl);
