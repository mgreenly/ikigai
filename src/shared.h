#pragma once

#include "error.h"
#include "config.h"
#include "terminal.h"
#include "render.h"

#include <talloc.h>

// Shared infrastructure context - resources shared across all agents
// Created as sibling to repl_ctx under root_ctx (DI pattern)
typedef struct ik_shared_ctx {
    ik_cfg_t *cfg;  // Configuration (borrowed, not owned)
    ik_term_ctx_t *term;    // Terminal context
    ik_render_ctx_t *render; // Render context
} ik_shared_ctx_t;

// Create shared context (facade that will create infrastructure)
// ctx: talloc parent (root_ctx)
// cfg: configuration pointer (borrowed)
// out: receives allocated shared context
res_t ik_shared_ctx_init(TALLOC_CTX *ctx, ik_cfg_t *cfg, ik_shared_ctx_t **out);
