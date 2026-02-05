#pragma once

#include "apps/ikigai/shared.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/config.h"
#include "shared/error.h"

#include <talloc.h>

/**
 * @file test_contexts.h
 * @brief Test helper functions for creating test contexts
 *
 * These helpers simplify creating shared and repl contexts in tests by
 * providing reasonable defaults and reducing boilerplate code.
 *
 * Usage examples:
 *
 * 1. Create just a shared context:
 *    @code
 *    ik_shared_ctx_t *shared = NULL;
 *    res_t res = test_shared_ctx_create(ctx, &shared);
 *    @endcode
 *
 * 2. Create both shared and repl contexts (most common):
 *    @code
 *    ik_shared_ctx_t *shared = NULL;
 *    ik_repl_ctx_t *repl = NULL;
 *    res_t res = test_repl_create(ctx, &shared, &repl);
 *    @endcode
 *
 * 3. Create shared context with custom config:
 *    @code
 *    ik_config_t *cfg = test_cfg_create(ctx);
 *    cfg->history_size = 250;  // Customize as needed
 *    ik_shared_ctx_t *shared = NULL;
 *    res_t res = test_shared_ctx_create_with_cfg(ctx, cfg, &shared);
 *    @endcode
 */

// Create minimal config suitable for testing (no database, no API key)
ik_config_t *test_cfg_create(TALLOC_CTX *ctx);

// Create shared_ctx with test defaults
res_t test_shared_ctx_create(TALLOC_CTX *ctx, ik_shared_ctx_t **out);

// Create shared + repl together (most common test need)
// Both are children of ctx (siblings to each other)
res_t test_repl_create(TALLOC_CTX *ctx,
                       ik_shared_ctx_t **shared_out,
                       ik_repl_ctx_t **repl_out);

// Create shared with custom config
res_t test_shared_ctx_create_with_cfg(TALLOC_CTX *ctx,
                                       ik_config_t *cfg,
                                       ik_shared_ctx_t **out);
