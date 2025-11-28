#pragma once

#include "error.h"
#include "terminal.h"
#include "render.h"
#include "input_buffer.h"
#include "input.h"
#include "scrollback.h"
#include <stdbool.h>
#include <inttypes.h>

// Viewport boundaries for rendering (Phase 4)
typedef struct {
    size_t scrollback_start_line;   // First scrollback line to render
    size_t scrollback_lines_count;  // How many scrollback lines visible
    size_t input_buffer_start_row;     // Terminal row where input buffer begins
    bool separator_visible;         // Whether separator is in visible range
} ik_viewport_t;

// REPL context structure
typedef struct ik_repl_ctx_t {
    ik_term_ctx_t *term;        // Terminal context
    ik_render_ctx_t *render;    // Rendering context
    ik_input_buffer_t *input_buffer;  // Input buffer
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

// Render current frame (input buffer only for now)
res_t ik_repl_render_frame(ik_repl_ctx_t *repl);

// Calculate viewport boundaries (Phase 4)
res_t ik_repl_calculate_viewport(ik_repl_ctx_t *repl, ik_viewport_t *viewport_out);

// Submit current input buffer line to scrollback (Phase 4 Task 4.6)
res_t ik_repl_submit_line(ik_repl_ctx_t *repl);

// Handle terminal resize (Bug #5)
res_t ik_repl_handle_resize(ik_repl_ctx_t *repl);
