#ifndef IK_DB_AGENT_REPLAY_H
#define IK_DB_AGENT_REPLAY_H

#include "connection.h"
#include "replay.h"
#include "../error.h"

/**
 * Find most recent clear event for an agent
 *
 * Searches for the most recent 'clear' event in the messages table for the
 * specified agent, optionally limiting by maximum message ID.
 *
 * @param db_ctx Database context (must not be NULL)
 * @param agent_uuid Agent UUID to search (must not be NULL)
 * @param max_id Maximum message ID to consider (0 = no limit)
 * @param clear_id_out Output for clear message ID (0 if not found)
 * @return OK on success, ERR on database failure
 */
res_t ik_agent_find_clear(ik_db_ctx_t *db_ctx, const char *agent_uuid,
                          int64_t max_id, int64_t *clear_id_out);

/**
 * Build replay ranges by walking ancestor chain
 *
 * Implements the "walk backwards, play forwards" algorithm:
 * 1. Start at leaf agent, end_id=0
 * 2. For each agent, find most recent clear (within range)
 * 3. If clear found: add range starting after clear, terminate walk
 * 4. If no clear: add range from beginning, continue to parent
 * 5. Reverse array for chronological order (root first)
 *
 * @param db_ctx Database context (must not be NULL)
 * @param mem_ctx Talloc context for result allocation (must not be NULL)
 * @param agent_uuid Leaf agent UUID to start from (must not be NULL)
 * @param ranges_out Output for array of replay ranges (must not be NULL)
 * @param count_out Output for number of ranges (must not be NULL)
 * @return OK on success, ERR on failure
 */
res_t ik_agent_build_replay_ranges(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                                    const char *agent_uuid,
                                    ik_replay_range_t **ranges_out,
                                    size_t *count_out);

/**
 * Query messages for a single replay range
 *
 * Retrieves messages for the specified agent within the given ID range.
 * Uses the query: WHERE agent_uuid=$1 AND id>$2 AND ($3=0 OR id<=$3)
 *
 * @param db_ctx Database context (must not be NULL)
 * @param mem_ctx Talloc context for result allocation (must not be NULL)
 * @param range Replay range specification (must not be NULL)
 * @param messages_out Output for array of message pointers (must not be NULL)
 * @param count_out Output for number of messages (must not be NULL)
 * @return OK on success, ERR on failure
 */
res_t ik_agent_query_range(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                            const ik_replay_range_t *range,
                            ik_message_t ***messages_out,
                            size_t *count_out);

/**
 * Replay history for an agent using range-based algorithm
 *
 * High-level function that combines build_replay_ranges and query_range
 * to reconstruct an agent's full conversation context on startup.
 *
 * @param db_ctx Database context (must not be NULL)
 * @param mem_ctx Talloc context for result allocation (must not be NULL)
 * @param agent_uuid Agent UUID to replay (must not be NULL)
 * @param ctx_out Output for replay context (must not be NULL)
 * @return OK on success, ERR on failure
 */
res_t ik_agent_replay_history(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                               const char *agent_uuid,
                               ik_replay_context_t **ctx_out);

#endif // IK_DB_AGENT_REPLAY_H
