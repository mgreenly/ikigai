#ifndef IK_DB_AGENT_ZERO_H
#define IK_DB_AGENT_ZERO_H

#include "connection.h"
#include "../error.h"

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

#endif // IK_DB_AGENT_ZERO_H
