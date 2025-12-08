#pragma once

#include "error.h"

#include <talloc.h>

// Shared infrastructure context - resources shared across all agents
// Created as sibling to repl_ctx under root_ctx (DI pattern)
typedef struct ik_shared_ctx {
    // Fields will be migrated here incrementally
    // Currently empty - infrastructure only
} ik_shared_ctx_t;

// Create shared context (facade that will create infrastructure)
// ctx: talloc parent (root_ctx)
// out: receives allocated shared context
res_t ik_shared_ctx_init(TALLOC_CTX *ctx, ik_shared_ctx_t **out);
