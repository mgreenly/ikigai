#include "test_contexts.h"

#include "panic.h"
#include "wrapper.h"

ik_cfg_t *test_cfg_create(TALLOC_CTX *ctx)
{
    ik_cfg_t *cfg = talloc_zero_(ctx, sizeof(ik_cfg_t));
    if (cfg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Minimal defaults for testing
    cfg->history_size = 100;
    cfg->db_connection_string = NULL;  // No database in tests by default
    cfg->openai_api_key = NULL;        // No API key in tests
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
    ik_cfg_t *cfg = test_cfg_create(ctx);
    return ik_shared_ctx_init(ctx, cfg, out);
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
                                       ik_cfg_t *cfg,
                                       ik_shared_ctx_t **out)
{
    return ik_shared_ctx_init(ctx, cfg, out);
}
