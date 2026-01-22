#ifndef IK_DB_AGENT_H
#define IK_DB_AGENT_H

#include "connection.h"
#include "../error.h"
#include "../agent.h"

/**
 * Insert agent into registry
 *
 * Inserts a new agent record into the agents table with status='running'.
 * The created_at timestamp is set to the current time (NOW()).
 *
 * @param db_ctx Database context (must not be NULL)
 * @param agent Agent context with uuid, parent_uuid, created_at set (must not be NULL)
 * @return OK on success, ERR on failure
 */
res_t ik_db_agent_insert(ik_db_ctx_t *db_ctx, const ik_agent_ctx_t *agent);

/**
 * Mark agent as dead
 *
 * Updates an agent's status from 'running' to 'dead' and sets the ended_at timestamp.
 * Called when killing an agent. Operation is idempotent - marking an already-dead
 * agent is a no-op.
 *
 * @param db_ctx Database context (must not be NULL)
 * @param uuid Agent UUID to update (must not be NULL)
 * @return OK on success, ERR on failure
 */
res_t ik_db_agent_mark_dead(ik_db_ctx_t *db_ctx, const char *uuid);

/**
 * Agent row structure for query results
 *
 * Represents a single agent record from the database.
 * All fields are allocated on the provided talloc context.
 */
typedef struct {
    char *uuid;
    char *name;
    char *parent_uuid;
    char *fork_message_id;
    char *status;
    int64_t created_at;
    int64_t ended_at;  // 0 if still running
    char *provider;        // LLM provider (nullable)
    char *model;           // Model identifier (nullable)
    char *thinking_level;  // Thinking budget/level (nullable)
} ik_db_agent_row_t;

/**
 * Lookup agent by UUID
 *
 * Retrieves a single agent record by UUID. Returns error if agent not found.
 *
 * @param db_ctx Database context (must not be NULL)
 * @param ctx Talloc context for result allocation (must not be NULL)
 * @param uuid Agent UUID to lookup (must not be NULL)
 * @param out Output parameter for agent row (must not be NULL)
 * @return OK with agent row on success, ERR if not found or on failure
 */
res_t ik_db_agent_get(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, const char *uuid, ik_db_agent_row_t **out);

/**
 * List all running agents
 *
 * Returns all agents with status='running' ordered by created_at.
 *
 * @param db_ctx Database context (must not be NULL)
 * @param ctx Talloc context for result allocation (must not be NULL)
 * @param out Output parameter for array of agent row pointers (must not be NULL)
 * @param count Output parameter for array size (must not be NULL)
 * @return OK with agent rows on success, ERR on failure
 */
res_t ik_db_agent_list_running(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, ik_db_agent_row_t ***out, size_t *count);

/**
 * Get the last message ID for an agent
 *
 * Returns the maximum message ID for an agent. Used during fork to record
 * the fork point (the last message before the fork). Returns 0 if the agent
 * has no messages.
 *
 * @param db_ctx Database context (must not be NULL)
 * @param agent_uuid Agent UUID (must not be NULL)
 * @param out_message_id Output parameter for last message ID (must not be NULL)
 * @return OK on success, ERR on failure
 */
res_t ik_db_agent_get_last_message_id(ik_db_ctx_t *db_ctx, const char *agent_uuid, int64_t *out_message_id);

/**
 * Update agent provider configuration
 *
 * Updates the provider, model, and thinking_level fields for an agent.
 * All three fields are updated atomically. NULL values are allowed and will
 * clear the configuration. Returns OK if agent not found (UPDATE affects 0 rows).
 *
 * @param db_ctx Database context (must not be NULL)
 * @param uuid Agent UUID to update (must not be NULL)
 * @param provider Provider name (may be NULL)
 * @param model Model identifier (may be NULL)
 * @param thinking_level Thinking budget/level (may be NULL)
 * @return OK on success, ERR_DB_CONNECT on database error
 */
res_t ik_db_agent_update_provider(ik_db_ctx_t *db_ctx,
                                  const char *uuid,
                                  const char *provider,
                                  const char *model,
                                  const char *thinking_level);

#endif // IK_DB_AGENT_H
