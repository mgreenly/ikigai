#ifndef IK_DB_SESSION_SUMMARY_H
#define IK_DB_SESSION_SUMMARY_H

#include "shared/error.h"
#include "apps/ikigai/db/connection.h"

#include <stdint.h>
#include <talloc.h>

/**
 * A single session summary record.
 *
 * Represents one summarized epoch of a conversation for an agent.
 * Used by the sliding context window to fill the reserved summary portion.
 */
typedef struct {
    int64_t id;
    char *agent_uuid;
    char *summary;
    int64_t start_msg_id;
    int64_t end_msg_id;
    int token_count;
} ik_session_summary_t;

/**
 * Insert a session summary, enforcing a cap of 5 per agent.
 *
 * Inserts a new row into session_summaries. After insert, deletes the oldest
 * rows for the agent if the total exceeds 5, keeping only the 5 most recent.
 *
 * Fails if a summary for the same (agent_uuid, start_msg_id, end_msg_id)
 * already exists (unique constraint violation).
 *
 * @param db           Database connection context (must not be NULL)
 * @param agent_uuid   Agent UUID string (must not be NULL)
 * @param summary      Summary text (must not be NULL)
 * @param start_msg_id First message ID of the summarized epoch (must be > 0)
 * @param end_msg_id   Last message ID of the summarized epoch (must be > 0)
 * @param token_count  Token count of the summary text (must be >= 0)
 * @return             OK on success, ERR on failure
 */
res_t ik_db_session_summary_insert(ik_db_ctx_t *db,
                                   const char *agent_uuid,
                                   const char *summary,
                                   int64_t start_msg_id,
                                   int64_t end_msg_id,
                                   int token_count);

/**
 * Load all session summaries for an agent, ordered oldest-first.
 *
 * Allocates an array of ik_session_summary_t pointers as children of ctx.
 * Returns OK with *out = NULL and *count = 0 when no summaries exist.
 *
 * Memory ownership:
 * - *out is allocated on ctx; each element is a child of the array
 * - Single talloc_free(ctx) releases everything
 *
 * @param db         Database connection context (must not be NULL)
 * @param ctx        Talloc context for allocation (must not be NULL)
 * @param agent_uuid Agent UUID string (must not be NULL)
 * @param out        Output parameter: array of summary pointers (must not be NULL)
 * @param count      Output parameter: number of summaries (must not be NULL)
 * @return           OK on success, ERR on failure
 */
res_t ik_db_session_summary_load(ik_db_ctx_t *db,
                                 TALLOC_CTX *ctx,
                                 const char *agent_uuid,
                                 ik_session_summary_t ***out,
                                 size_t *count);

#endif // IK_DB_SESSION_SUMMARY_H
