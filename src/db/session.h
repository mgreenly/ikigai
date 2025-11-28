#ifndef IK_DB_SESSION_H
#define IK_DB_SESSION_H

#include "connection.h"
#include "../error.h"
#include <inttypes.h>

/**
 * Create a new session
 *
 * Inserts a new row into the sessions table with started_at set to NOW()
 * and ended_at set to NULL. Returns the new session ID via RETURNING clause.
 *
 * @param db_ctx Database context (must not be NULL)
 * @param session_id_out Output parameter for new session ID (must not be NULL)
 * @return OK with session_id on success, ERR on failure
 */
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out);

/**
 * Get the most recent active session
 *
 * Queries for a session where ended_at IS NULL, ordered by started_at DESC.
 * If no active session exists, returns OK with session_id=0 (not an error).
 *
 * @param db_ctx Database context (must not be NULL)
 * @param session_id_out Output parameter for session ID (must not be NULL)
 * @return OK with session_id (0 if none found), ERR on database failure
 */
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out);

/**
 * End a session
 *
 * Updates the specified session to set ended_at = NOW().
 * After this call, the session will no longer be returned by get_active.
 *
 * @param db_ctx Database context (must not be NULL)
 * @param session_id Session ID to end (must be > 0)
 * @return OK on success, ERR on failure
 */
res_t ik_db_session_end(ik_db_ctx_t *db_ctx, int64_t session_id);

#endif // IK_DB_SESSION_H
