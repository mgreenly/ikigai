#pragma once

#include "config.h"
#include "db/connection.h"
#include "error.h"
#include "history.h"
#include "render.h"
#include "terminal.h"

#include <inttypes.h>
#include <talloc.h>

// Shared infrastructure context - resources shared across all agents
// Created as sibling to repl_ctx under root_ctx (DI pattern)
typedef struct ik_shared_ctx {
    ik_cfg_t *cfg;  // Configuration (borrowed, not owned)
    ik_term_ctx_t *term;    // Terminal context
    ik_render_ctx_t *render; // Render context
    ik_db_ctx_t *db_ctx;     // Database connection (NULL if not configured)
    int64_t session_id;       // Current session ID (0 if no database)
    ik_history_t *history;   // Command history (shared across all agents)
} ik_shared_ctx_t;

// Create shared context (facade that will create infrastructure)
// ctx: talloc parent (root_ctx)
// cfg: configuration pointer (borrowed)
// out: receives allocated shared context
res_t ik_shared_ctx_init(TALLOC_CTX *ctx, ik_cfg_t *cfg, ik_shared_ctx_t **out);
