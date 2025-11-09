#pragma once

#include "result.h"
#include <stdbool.h>

// Forward declarations
typedef struct ik_term_ctx_t ik_term_ctx_t;
typedef struct ik_render_ctx_t ik_render_ctx_t;
typedef struct ik_workspace_t ik_workspace_t;
typedef struct ik_input_parser_t ik_input_parser_t;

// REPL context structure
typedef struct ik_repl_ctx_t {
    ik_term_ctx_t *term;              // Terminal context
    ik_render_ctx_t *render;          // Render context
    ik_workspace_t *workspace;        // Workspace
    ik_input_parser_t *input_parser;  // Input parser
    bool quit;                        // Exit flag
} ik_repl_ctx_t;

// Initialize REPL context
res_t ik_repl_init(void *parent, ik_repl_ctx_t **repl_out);

// Cleanup REPL context
void ik_repl_cleanup(ik_repl_ctx_t *repl);

// Run REPL event loop
res_t ik_repl_run(ik_repl_ctx_t *repl);
