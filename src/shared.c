#include "shared.h"

#include "db/connection.h"
#include "debug_pipe.h"
#include "history.h"
#include "logger.h"
#include "panic.h"
#include "render.h"
#include "terminal.h"
#include "wrapper.h"

#include <assert.h>

// Destructor for shared context - handles cleanup
static int shared_destructor(ik_shared_ctx_t *shared)
{
    // Cleanup terminal (restore state)
    if (shared->term != NULL) {  // LCOV_EXCL_BR_LINE - Defensive: NULL only if init failed
        ik_term_cleanup(shared->term);
    }
    return 0;
}

res_t ik_shared_ctx_init(TALLOC_CTX *ctx, ik_cfg_t *cfg, const char *working_dir, const char *ikigai_path, ik_logger_t *logger, ik_shared_ctx_t **out)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(cfg != NULL);   // LCOV_EXCL_BR_LINE
    assert(working_dir != NULL);   // LCOV_EXCL_BR_LINE
    assert(ikigai_path != NULL);   // LCOV_EXCL_BR_LINE
    assert(logger != NULL);   // LCOV_EXCL_BR_LINE
    assert(out != NULL);   // LCOV_EXCL_BR_LINE

    (void)working_dir;  // Reserved for future use
    (void)ikigai_path;  // Reserved for future use

    ik_shared_ctx_t *shared = talloc_zero_(ctx, sizeof(ik_shared_ctx_t));
    if (shared == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    shared->cfg = cfg;

    // Use injected logger (DI pattern - explicit dependency)
    assert(logger != NULL);  // LCOV_EXCL_BR_LINE
    shared->logger = logger;
    talloc_steal(shared, logger);  // Transfer ownership

    // Initialize terminal (raw mode + alternate screen)
    res_t result = ik_term_init(shared, &shared->term);
    if (is_err(&result)) {
        talloc_free(shared);
        return result;
    }

    // Initialize render
    result = ik_render_create(shared,
                              shared->term->screen_rows,
                              shared->term->screen_cols,
                              shared->term->tty_fd,
                              &shared->render);
    if (is_err(&result)) {
        ik_term_cleanup(shared->term);
        talloc_free(shared);
        return result;
    }

    // Initialize database connection if configured
    if (cfg->db_connection_string != NULL) {
        result = ik_db_init_(shared, cfg->db_connection_string, (void **)&shared->db_ctx);
        if (is_err(&result)) {
            // Cleanup already-initialized resources
            if (shared->term != NULL) {  // LCOV_EXCL_BR_LINE - Defensive: term always set before db init
                ik_term_cleanup(shared->term);
            }
            talloc_free(shared);
            return result;
        }
    } else {
        shared->db_ctx = NULL;
    }

    // Initialize session_id to 0 (session creation stays in repl_init for now)
    shared->session_id = 0;

    // Initialize command history
    shared->history = ik_history_create(shared, (size_t)cfg->history_size);
    result = ik_history_load(shared, shared->history, logger);
    if (is_err(&result)) {
        // Log warning but continue with empty history (graceful degradation)
        yyjson_mut_doc *log_doc = ik_log_create();
        yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);
        if (root == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_str(log_doc, root, "message", "Failed to load history")) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_str(log_doc, root, "error", result.err->msg)) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        ik_logger_warn_json(logger, log_doc);
        talloc_free(result.err);
    }

    // Initialize debug infrastructure
    shared->debug_enabled = false;
    shared->debug_mgr = ik_debug_manager_create(shared).ok;
    if (shared->debug_mgr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    shared->openai_debug_pipe = ik_debug_manager_add_pipe(shared->debug_mgr, "[openai]").ok;
    if (shared->openai_debug_pipe == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    shared->db_debug_pipe = ik_debug_manager_add_pipe(shared->debug_mgr, "[db]").ok;
    if (shared->db_debug_pipe == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Set destructor for cleanup
    talloc_set_destructor(shared, shared_destructor);

    *out = shared;
    return OK(shared);
}
