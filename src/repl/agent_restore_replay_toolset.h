/**
 * @file agent_restore_replay_toolset.h
 * @brief Toolset replay logic for agent restoration
 */

#ifndef IK_REPL_AGENT_RESTORE_REPLAY_TOOLSET_H
#define IK_REPL_AGENT_RESTORE_REPLAY_TOOLSET_H

#include "../agent.h"
#include "../db/connection.h"
#include "../error.h"

/**
 * @brief Replay toolset filter for an agent
 *
 * Queries the most recent toolset command for the agent and replays it
 * to restore the toolset filter state.
 *
 * @param db Database context
 * @param agent Agent to replay toolset for
 * @return res_t OK on success, ERR on database error
 */
res_t ik_agent_replay_toolset(ik_db_ctx_t *db, ik_agent_ctx_t *agent);

#endif
