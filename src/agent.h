#pragma once

#include "error.h"

#include <talloc.h>
#include <stdbool.h>

// Forward declaration
typedef struct ik_shared_ctx ik_shared_ctx_t;

// Per-agent context - state specific to one agent
// Created as child of repl_ctx (owned by coordinator)
typedef struct ik_agent_ctx {
    // Identity (from agent-process-model.md)
    char *uuid;          // Internal unique identifier
    char *name;          // Optional human-friendly name (NULL if unnamed)
    char *parent_uuid;   // Parent agent's UUID (NULL for root agent)

    // Reference to shared infrastructure
    ik_shared_ctx_t *shared;

    // Fields will be migrated here incrementally
} ik_agent_ctx_t;

// Create agent context
// ctx: talloc parent (repl_ctx)
// shared: shared infrastructure
// parent_uuid: parent agent's UUID (NULL for root agent)
// out: receives allocated agent context
res_t ik_agent_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                      const char *parent_uuid, ik_agent_ctx_t **out);

// Generate a new UUID as base64url string (helper function)
// ctx: talloc parent for the returned string
// Returns: newly allocated 22-character base64url UUID string
char *ik_agent_generate_uuid(TALLOC_CTX *ctx);
