#ifndef IK_REPL_AGENT_RESTORE_H
#define IK_REPL_AGENT_RESTORE_H

#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/repl.h"
#include "shared/error.h"

/**
 * Restore all running agents from database on startup
 *
 * Queries all agents with status='running', sorts by created_at (oldest first),
 * and restores each one (replay history, populate conversation/scrollback).
 *
 * Sorting by created_at ensures parents are restored before children, since
 * a parent must exist before a child can be forked from it.
 *
 * Agent 0 (root agent with parent_uuid=NULL) uses the existing repl->current
 * context. Its conversation, scrollback, and marks are populated from the
 * replayed history using the same agent-based replay as other agents.
 *
 * @param repl REPL context (must not be NULL, must have current agent set)
 * @param db_ctx Database context (must not be NULL)
 * @return OK on success, ERR on failure
 */
res_t ik_repl_restore_agents(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx);

#endif // IK_REPL_AGENT_RESTORE_H
