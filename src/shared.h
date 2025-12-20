#pragma once

#include "config.h"
#include "db/connection.h"
#include "debug_pipe.h"
#include "error.h"
#include "history.h"
#include "logger.h"
#include "render.h"
#include "terminal.h"

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
    ik_cfg_t *cfg;  // Configuration (borrowed, not owned)
    ik_logger_t *logger;     // Logger instance (DI pattern)
    ik_term_ctx_t *term;    // Terminal context
    ik_render_ctx_t *render; // Render context
    ik_db_ctx_t *db_ctx;     // Database connection (NULL if not configured)
    int64_t session_id;       // Current session ID (0 if no database)
    ik_history_t *history;   // Command history (shared across all agents)
    ik_debug_pipe_manager_t *debug_mgr;  // Debug pipe manager
    ik_debug_pipe_t *openai_debug_pipe;  // OpenAI debug pipe
    ik_debug_pipe_t *db_debug_pipe;      // Database debug pipe
    bool debug_enabled;                   // Debug flag
    atomic_bool fork_pending;             // Fork operation in progress (concurrency control)
} ik_shared_ctx_t;

// Create shared context (facade that will create infrastructure)
// ctx: talloc parent (root_ctx)
// cfg: configuration pointer (borrowed)
// working_dir: directory for logger initialization (typically cwd)
// ikigai_path: path to ikigai directory (e.g., ".ikigai")
// logger: pre-created logger instance (ownership transferred)
// out: receives allocated shared context
res_t ik_shared_ctx_init(TALLOC_CTX *ctx,
                         ik_cfg_t *cfg,
                         const char *working_dir,
                         const char *ikigai_path,
                         ik_logger_t *logger,
                         ik_shared_ctx_t **out);
