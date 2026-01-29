#pragma once

#include "config.h"
#include "credentials.h"
#include "db/connection.h"
#include "error.h"
#include "history.h"
#include "logger.h"
#include "paths.h"
#include "render.h"
#include "terminal.h"
#include "tool_registry.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <talloc.h>

/**
 * Shared infrastructure context for ikigai.
 *
 * Contains resources shared across all agents in a session:
 * - Configuration (borrowed, not owned)
 * - Terminal I/O
 * - Database connection
 * - Command history
 *
 * Ownership: Created as sibling to ik_repl_ctx_t under root_ctx.
 * This follows DI principles: dependencies created first, injected
 * into consumers (repl_ctx).
 *
 * Thread safety: Currently single-threaded. Phase 2 will add
 * synchronization for multi-agent access.
 */
typedef struct ik_shared_ctx {
    ik_config_t *cfg;  // Configuration (borrowed, not owned)
    ik_paths_t *paths; // Path resolution (borrowed, not owned)
    ik_logger_t *logger;     // Logger instance (DI pattern)
    ik_term_ctx_t *term;    // Terminal context
    ik_render_ctx_t *render; // Render context
    ik_db_ctx_t *db_ctx;     // Database connection (NULL if not configured)
    int64_t session_id;       // Current session ID (0 if no database)
    ik_history_t *history;   // Command history (shared across all agents)
    atomic_bool fork_pending;             // Fork operation in progress (concurrency control)
    ik_tool_registry_t *tool_registry;   // External tool registry (rel-08)
} ik_shared_ctx_t;

// Create shared context (facade that will create infrastructure)
// ctx: talloc parent (root_ctx)
// cfg: configuration pointer (borrowed)
// creds: credentials pointer (borrowed)
// paths: paths instance (borrowed)
// logger: pre-created logger instance (ownership transferred)
// out: receives allocated shared context
res_t ik_shared_ctx_init(TALLOC_CTX *ctx,
                         ik_config_t *cfg,
                         ik_credentials_t *creds,
                         ik_paths_t *paths,
                         ik_logger_t *logger,
                         ik_shared_ctx_t **out);
