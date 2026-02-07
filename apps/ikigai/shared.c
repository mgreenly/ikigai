#include "apps/ikigai/shared.h"

#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/history.h"
#include "apps/ikigai/history_io.h"
#include "apps/ikigai/internal_tools.h"
#include "shared/logger.h"
#include "shared/panic.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/render.h"
#include "shared/terminal.h"
#include "apps/ikigai/tool_discovery.h"
#include "apps/ikigai/tool_registry.h"
#include "shared/wrapper.h"

#include <fcntl.h>
#include <unistd.h>

#include <assert.h>


#include "shared/poison.h"
// Destructor for shared context - handles cleanup
static int shared_destructor(ik_shared_ctx_t *shared)
{
    // Cleanup terminal (restore state)
    if (shared->term != NULL) {  // LCOV_EXCL_BR_LINE - Defensive: NULL only if init failed
        ik_term_cleanup(shared->term);
    }
    return 0;
}

res_t ik_shared_ctx_init(TALLOC_CTX *ctx,
                         ik_config_t *cfg,
                         ik_credentials_t *creds,
                         ik_paths_t *paths,
                         ik_logger_t *logger,
                         ik_shared_ctx_t **out)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(cfg != NULL);   // LCOV_EXCL_BR_LINE
    assert(creds != NULL);   // LCOV_EXCL_BR_LINE
    assert(paths != NULL);   // LCOV_EXCL_BR_LINE
    assert(logger != NULL);   // LCOV_EXCL_BR_LINE
    assert(out != NULL);   // LCOV_EXCL_BR_LINE

    ik_shared_ctx_t *shared = talloc_zero_(ctx, sizeof(ik_shared_ctx_t));
    if (shared == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    shared->cfg = cfg;
    shared->paths = paths;

    // Use injected logger (DI pattern - explicit dependency)
    // Note: Do NOT talloc_steal - logger must outlive shared for final logging
    assert(logger != NULL);  // LCOV_EXCL_BR_LINE
    shared->logger = logger;

    // Initialize terminal (raw mode + alternate screen)
    res_t result = ik_term_init(shared, shared->logger, &shared->term);
    if (is_err(&result)) {
        talloc_free(shared);
        return result;
    }

    // Redirect stdout and stderr to /dev/null to prevent any library output
    // from bypassing the alternate screen buffer and causing screen flicker.
    // We use /dev/tty for all rendering, and logs go to files.
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
        dup2(null_fd, STDOUT_FILENO);  // Redirect stdout (fd 1)
        dup2(null_fd, STDERR_FILENO);  // Redirect stderr (fd 2)
        close(null_fd);
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
    char *db_connection_string = NULL;
    if (cfg->db_host && cfg->db_name && cfg->db_user) {
        // Build PostgreSQL connection string: postgresql://user:pass@host:port/dbname
        const char *db_pass = creds->db_pass ? creds->db_pass : "";
        db_connection_string = talloc_asprintf(shared,
                                               "postgresql://%s:%s@%s:%" PRId32 "/%s",
                                               cfg->db_user,
                                               db_pass,
                                               cfg->db_host,
                                               cfg->db_port,
                                               cfg->db_name);
        if (!db_connection_string) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    if (db_connection_string != NULL) {
        const char *data_dir = ik_paths_get_data_dir(paths);
        result = ik_db_init_(ctx, db_connection_string, data_dir, (void **)&shared->db_ctx);
        if (is_err(&result)) {
            // Cleanup already-initialized resources
            if (shared->term != NULL) {  // LCOV_EXCL_BR_LINE - Defensive: term always set before db init
                ik_term_cleanup(shared->term);
            }
            talloc_free(shared);
            return result;
        }
        // Steal db_ctx to shared for proper ownership
        talloc_steal(shared, shared->db_ctx);

        // Create worker thread DB connection (same connection string)
        result = ik_db_init_(ctx, db_connection_string, data_dir, (void **)&shared->worker_db_ctx);
        if (is_err(&result)) {
            // Cleanup already-initialized resources
            if (shared->term != NULL) {  // LCOV_EXCL_BR_LINE - Defensive: term always set before db init
                ik_term_cleanup(shared->term);
            }
            talloc_free(shared);
            return result;
        }
        // Steal worker_db_ctx to shared for proper ownership
        talloc_steal(shared, shared->worker_db_ctx);
    } else {
        shared->db_ctx = NULL;
        shared->worker_db_ctx = NULL;
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

    // Initialize tool registry (rel-08)
    shared->tool_registry = ik_tool_registry_create(shared);
    if (shared->tool_registry == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Run initial tool discovery
    const char *system_dir = ik_paths_get_tools_system_dir(paths);
    const char *user_dir = ik_paths_get_tools_user_dir(paths);
    const char *project_dir = ik_paths_get_tools_project_dir(paths);
    result = ik_tool_discovery_run(shared, system_dir, user_dir, project_dir, shared->tool_registry);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE - OOM or corruption in discovery
        // Log warning but continue with empty registry (graceful degradation)  // LCOV_EXCL_LINE
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        if (root == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE  // LCOV_EXCL_LINE
        if (!yyjson_mut_obj_add_str(log_doc, root, "message", "Failed to discover tools")) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE  // LCOV_EXCL_LINE
        if (!yyjson_mut_obj_add_str(log_doc, root, "error", result.err->msg)) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE  // LCOV_EXCL_LINE
        ik_logger_warn_json(logger, log_doc);  // LCOV_EXCL_LINE
        talloc_free(result.err);  // LCOV_EXCL_LINE
    }

    // Register internal tools (after external discovery, so internal tools overwrite on collision)
    ik_internal_tools_register(shared->tool_registry);

    // Sort registry after all tools are registered
    ik_tool_registry_sort(shared->tool_registry);

    // Set destructor for cleanup
    talloc_set_destructor(shared, shared_destructor);

    *out = shared;
    return OK(shared);
}
