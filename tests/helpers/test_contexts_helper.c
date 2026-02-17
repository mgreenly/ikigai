// NOTE: Calling test files must call ik_test_set_log_dir(__FILE__)
// before using these helpers to ensure proper log isolation.

#include "tests/helpers/test_contexts_helper.h"

#include "shared/credentials.h"
#include "shared/logger.h"
#include "shared/panic.h"
#include "apps/ikigai/paths.h"
#include "shared/wrapper.h"
#include "tests/helpers/test_utils_helper.h"

ik_config_t *test_cfg_create(TALLOC_CTX *ctx)
{
    ik_config_t *cfg = talloc_zero_(ctx, sizeof(ik_config_t));
    if (cfg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Minimal defaults for testing
    cfg->history_size = 100;
    cfg->db_host = NULL;  // No database in tests by default
    cfg->db_port = 0;
    cfg->db_name = NULL;
    cfg->db_user = NULL;
    cfg->openai_model = NULL;
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 4096;
    cfg->openai_system_message = NULL;
    cfg->listen_address = NULL;
    cfg->listen_port = 0;
    cfg->max_tool_turns = 10;
    cfg->max_output_size = 1048576;

    return cfg;
}

res_t test_shared_ctx_create(TALLOC_CTX *ctx, ik_shared_ctx_t **out)
{
    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(ctx, &paths);
    if (is_err(&result)) return result;

    // Create config, credentials, and logger
    ik_config_t *cfg = test_cfg_create(ctx);
    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    return ik_shared_ctx_init(ctx, cfg, creds, paths, logger, out);
}

res_t test_repl_create(TALLOC_CTX *ctx,
                       ik_shared_ctx_t **shared_out,
                       ik_repl_ctx_t **repl_out)
{
    ik_shared_ctx_t *shared = NULL;
    res_t result = test_shared_ctx_create(ctx, &shared);
    if (is_err(&result)) return result;

    ik_repl_ctx_t *repl = NULL;
    result = ik_repl_init(ctx, shared, &repl);
    if (is_err(&result)) {
        talloc_free(shared);
        return result;
    }

    *shared_out = shared;
    *repl_out = repl;
    return OK(repl);
}

res_t test_shared_ctx_create_with_cfg(TALLOC_CTX *ctx,
                                       ik_config_t *cfg,
                                       ik_shared_ctx_t **out)
{
    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(ctx, &paths);
    if (is_err(&result)) return result;

    // Create credentials and logger
    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    return ik_shared_ctx_init(ctx, cfg, creds, paths, logger, out);
}
