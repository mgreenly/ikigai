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
} ik_db_agent_row_t;

/**
 * Lookup agent by UUID
 *
 * Retrieves a single agent record by UUID. Returns error if agent not found.
 *
 * @param db_ctx Database context (must not be NULL)
 * @param mem_ctx Talloc context for result allocation (must not be NULL)
 * @param uuid Agent UUID to lookup (must not be NULL)
 * @param out Output parameter for agent row (must not be NULL)
 * @return OK with agent row on success, ERR if not found or on failure
 */
res_t ik_db_agent_get(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                      const char *uuid, ik_db_agent_row_t **out);

/**
 * List all running agents
 *
 * Returns all agents with status='running' ordered by created_at.
 *
 * @param db_ctx Database context (must not be NULL)
 * @param mem_ctx Talloc context for result allocation (must not be NULL)
 * @param out Output parameter for array of agent row pointers (must not be NULL)
 * @param count Output parameter for array size (must not be NULL)
 * @return OK with agent rows on success, ERR on failure
 */
res_t ik_db_agent_list_running(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                               ik_db_agent_row_t ***out, size_t *count);

/**
 * Get children of an agent
 *
 * Returns all agents whose parent_uuid matches the given UUID, ordered by created_at.
 *
 * @param db_ctx Database context (must not be NULL)
 * @param mem_ctx Talloc context for result allocation (must not be NULL)
 * @param parent_uuid Parent agent UUID (must not be NULL)
 * @param out Output parameter for array of agent row pointers (must not be NULL)
 * @param count Output parameter for array size (must not be NULL)
 * @return OK with agent rows on success, ERR on failure
 */
res_t ik_db_agent_get_children(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                               const char *parent_uuid,
                               ik_db_agent_row_t ***out, size_t *count);

/**
 * Get parent agent
 *
 * Retrieves the parent agent record for a given agent. For walking ancestry chain.
 * Sets *out to NULL if agent has no parent (root agent).
 *
 * @param db_ctx Database context (must not be NULL)
 * @param mem_ctx Talloc context for result allocation (must not be NULL)
 * @param uuid Child agent UUID (must not be NULL)
 * @param out Output parameter for parent agent row (must not be NULL)
 * @return OK with parent row (or NULL for root) on success, ERR on failure
 */
res_t ik_db_agent_get_parent(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                              const char *uuid, ik_db_agent_row_t **out);

/**
 * Ensure Agent 0 exists in registry
 *
 * Creates Agent 0 if missing, retrieves UUID if present.
 * Called once during ik_repl_init().
 *
 * On fresh install: Creates root agent with parent_uuid=NULL, status='running'
 * On upgrade: If messages exist but no agents, creates Agent 0 and adopts orphan messages
 *
 * @param db Database context (must not be NULL)
 * @param out_uuid Output parameter for Agent 0's UUID (must not be NULL)
 * @return OK with UUID on success, ERR on failure
 */
res_t ik_db_ensure_agent_zero(ik_db_ctx_t *db, char **out_uuid);

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
res_t ik_db_agent_get_last_message_id(ik_db_ctx_t *db_ctx, const char *agent_uuid,
                                       int64_t *out_message_id);

#endif // IK_DB_AGENT_H
