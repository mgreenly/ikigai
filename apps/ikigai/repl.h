#pragma once

#include "shared/error.h"
#include "shared/terminal.h"
#include "apps/ikigai/render.h"
#include "apps/ikigai/input_buffer/core.h"
#include "apps/ikigai/input.h"
#include "apps/ikigai/scroll_detector.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/layer.h"
#include "apps/ikigai/layer_wrappers.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/tool.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/history.h"
#include "apps/ikigai/agent.h"
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

    // Current agent (per-agent state)
    // Currently single agent, will become current selection from array
    // All per-agent fields accessed via this pointer
    ik_agent_ctx_t *current;

    // Agent array (for navigation and /agents command)
    ik_agent_ctx_t **agents;     // Array of all loaded agents
    size_t agent_count;           // Number of agents in array
    size_t agent_capacity;        // Allocated capacity

    ik_input_parser_t *input_parser;  // Input parser
    atomic_bool quit;           // Exit flag (atomic for thread safety)
    ik_scroll_detector_t *scroll_det;  // Scroll detector (rel-05)

    // Layer-based rendering (Phase 1.3)

    // Reference fields for layers (updated before each render)

    // Debug info for separator (updated before each render)
    size_t debug_viewport_offset;     // viewport_offset value
    size_t debug_viewport_row;        // first_visible_row
    size_t debug_viewport_height;     // terminal_rows
    size_t debug_document_height;     // total document height
    uint64_t render_start_us;         // Timestamp when input received (0 = not set)
    uint64_t render_elapsed_us;       // Elapsed time from previous render (computed at end of render)

#ifdef IKIGAI_DEV
    // Dev-mode framebuffer dump (compile with IKIGAI_DEV=1)
    char *dev_framebuffer;            // Last rendered framebuffer
    size_t dev_framebuffer_len;       // Length of framebuffer
    int32_t dev_cursor_row;           // Final cursor row (0-indexed)
    int32_t dev_cursor_col;           // Final cursor col (0-indexed)
#endif

    // Note: completion removed - now in agent context (repl->current->completion)
    // Note: history removed - now in shared context (repl->shared->history)
    // Note: tool state removed - now in agent context (repl->current->pending_tool_call, etc.)
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

// Calculate total document height (all layers)
size_t ik_repl_calculate_document_height(const ik_repl_ctx_t *repl);

// Submit current input buffer line to scrollback (Phase 4 Task 4.6)
res_t ik_repl_submit_line(ik_repl_ctx_t *repl);

// Handle terminal resize (Bug #5)
res_t ik_repl_handle_resize(ik_repl_ctx_t *repl);

// State transition functions (Phase 1.6)
// Internal helper functions moved to repl_event_handlers.h

// Async tool execution
void ik_repl_start_tool_execution(ik_repl_ctx_t *repl);
void ik_repl_complete_tool_execution(ik_repl_ctx_t *repl);

// Tool loop decision function (Phase 2: Story 02)
bool ik_agent_should_continue_tool_loop(const ik_agent_ctx_t *agent);

// Agent array management
res_t ik_repl_add_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent);
res_t ik_repl_remove_agent(ik_repl_ctx_t *repl, const char *uuid);

// Find agent by UUID (exact or prefix match)
// Returns first matching agent, or NULL if no match or ambiguous
ik_agent_ctx_t *ik_repl_find_agent(ik_repl_ctx_t *repl, const char *uuid_prefix);

// Check if UUID prefix is ambiguous (matches multiple agents)
bool ik_repl_uuid_ambiguous(ik_repl_ctx_t *repl, const char *uuid_prefix);

// Switch from current agent to new_agent
// Saves state on outgoing, restores on incoming
// Returns ERR if new_agent is NULL or same as current
res_t ik_repl_switch_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *new_agent);

// Navigate to previous sibling agent (wraps around)
// Siblings are agents with the same parent_uuid
res_t ik_repl_nav_prev_sibling(ik_repl_ctx_t *repl);

// Navigate to next sibling agent (wraps around)
// Siblings are agents with the same parent_uuid
res_t ik_repl_nav_next_sibling(ik_repl_ctx_t *repl);

// Navigate to parent agent
// No action if at root or parent not in agents array
res_t ik_repl_nav_parent(ik_repl_ctx_t *repl);

// Navigate to most recent running child
// No action if no children
res_t ik_repl_nav_child(ik_repl_ctx_t *repl);

// Calculate and update navigation context for current agent's separator
// Called automatically after agent switch, fork, and kill
// Can be called manually to refresh navigation indicators
void ik_repl_update_nav_context(ik_repl_ctx_t *repl);

#ifdef IKIGAI_DEV
// Dump framebuffer to .ikigai/debug/repl_viewport.framebuffer
// Only available when compiled with IKIGAI_DEV=1
// Writes only if .ikigai/debug/ directory exists (runtime opt-in)
void ik_repl_dev_dump_framebuffer(ik_repl_ctx_t *repl);
#endif
