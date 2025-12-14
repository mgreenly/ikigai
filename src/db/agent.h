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

#endif // IK_DB_AGENT_H
