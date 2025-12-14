// REPL action processing - internal shared declarations
#pragma once

#include "repl.h"
#include "error.h"
#include "input.h"

// Forward declarations for internal action handlers
// These are used by the main action dispatcher in repl_actions.c

// Completion-related actions
res_t ik_repl_handle_tab_action(ik_repl_ctx_t *repl);
void ik_repl_dismiss_completion(ik_repl_ctx_t *repl);
void ik_repl_update_completion_after_char(ik_repl_ctx_t *repl);
res_t ik_repl_handle_completion_space_commit(ik_repl_ctx_t *repl);

// History navigation actions
res_t ik_repl_handle_arrow_up_action(ik_repl_ctx_t *repl);
res_t ik_repl_handle_arrow_down_action(ik_repl_ctx_t *repl);
res_t ik_repl_handle_history_prev_action(ik_repl_ctx_t *repl);  // Ctrl+P (rel-05)
res_t ik_repl_handle_history_next_action(ik_repl_ctx_t *repl);  // Ctrl+N (rel-05)

// Viewport/scrolling actions
res_t ik_repl_handle_page_up_action(ik_repl_ctx_t *repl);
res_t ik_repl_handle_page_down_action(ik_repl_ctx_t *repl);
res_t ik_repl_handle_scroll_up_action(ik_repl_ctx_t *repl);
res_t ik_repl_handle_scroll_down_action(ik_repl_ctx_t *repl);
size_t ik_repl_calculate_max_viewport_offset(ik_repl_ctx_t *repl);

// LLM and slash command actions
res_t ik_repl_handle_newline_action(ik_repl_ctx_t *repl);

// Scroll detection - process arrow keys through scroll detector (rel-05)
res_t ik_repl_process_scroll_detection(ik_repl_ctx_t *repl, const ik_input_action_t *action);

// Flush pending arrow from scroll detector (rel-05)
res_t ik_repl_flush_pending_scroll_arrow(ik_repl_ctx_t *repl, const ik_input_action_t *action);
