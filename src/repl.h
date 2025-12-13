#pragma once

#include "error.h"
#include "terminal.h"
#include "render.h"
#include "input_buffer/core.h"
#include "input.h"
#include "scroll_detector.h"
#include "scrollback.h"
#include "layer.h"
#include "layer_wrappers.h"
#include "config.h"
#include "openai/client.h"
#include "debug_pipe.h"
#include "tool.h"
#include "db/connection.h"
#include "history.h"
#include "agent.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <inttypes.h>

// Forward declarations
typedef struct ik_shared_ctx ik_shared_ctx_t;

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

    // Current agent (owns display state)
    ik_agent_ctx_t *current;

    ik_input_parser_t *input_parser;  // Input parser
    atomic_bool quit;           // Exit flag (atomic for thread safety)
    ik_scroll_detector_t *scroll_det;  // Scroll detector (rel-05)

    // Layer-based rendering (Phase 1.3)
    ik_layer_t *lower_separator_layer; // Separator layer (lower) - below input

    // Reference fields for layers (updated before each render)
    ik_spinner_state_t spinner_state; // Spinner state (Phase 1.4)
    bool lower_separator_visible;     // Separator visibility flag (lower)

    // Debug info for separator (updated before each render)
    size_t debug_viewport_offset;     // viewport_offset value
    size_t debug_viewport_row;        // first_visible_row
    size_t debug_viewport_height;     // terminal_rows
    size_t debug_document_height;     // total document height
    uint64_t render_start_us;         // Timestamp when input received (0 = not set)
    uint64_t render_elapsed_us;       // Elapsed time from previous render (computed at end of render)

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
