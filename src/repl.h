#pragma once

#include "error.h"
#include "terminal.h"
#include "render.h"
#include "workspace.h"
#include "input.h"
#include "scrollback.h"
#include <stdbool.h>
#include <inttypes.h>

// Viewport boundaries for rendering (Phase 4)
typedef struct {
    size_t scrollback_start_line;   // First scrollback line to render
    size_t scrollback_lines_count;  // How many scrollback lines visible
    size_t workspace_start_row;     // Terminal row where workspace begins
} ik_viewport_t;

// REPL context structure
typedef struct ik_repl_ctx_t {
    ik_term_ctx_t *term;        // Terminal context
    ik_render_ctx_t *render;    // Rendering context
    ik_workspace_t *workspace;  // Workspace
    ik_input_parser_t *input_parser;  // Input parser
    ik_scrollback_t *scrollback;      // Scrollback buffer (Phase 4)
    size_t viewport_offset;           // Physical row offset for scrolling (0 = bottom)
    bool quit;                  // Exit flag
} ik_repl_ctx_t;

// Initialize REPL context
res_t ik_repl_init(void *parent, ik_repl_ctx_t **repl_out);

// Cleanup REPL context
void ik_repl_cleanup(ik_repl_ctx_t *repl);

// Run REPL event loop
res_t ik_repl_run(ik_repl_ctx_t *repl);

// Render current frame (workspace only for now)
res_t ik_repl_render_frame(ik_repl_ctx_t *repl);

// Calculate viewport boundaries (Phase 4)
res_t ik_repl_calculate_viewport(ik_repl_ctx_t *repl, ik_viewport_t *viewport_out);

// Process single input action
res_t ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action);
