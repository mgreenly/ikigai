#include "session.h"

#include "pg_result.h"

#include "../error.h"
#include "../panic.h"
#include "../tmp_ctx.h"
#include "../wrapper.h"

#include <assert.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>


#include "poison.h"
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    assert(db_ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(session_id_out != NULL); // LCOV_EXCL_BR_LINE

    // Create temporary context for query result
    TALLOC_CTX *tmp = tmp_ctx_create();

    // Insert new session and return its ID
    const char *query = "INSERT INTO sessions DEFAULT VALUES RETURNING id";

    ik_pg_result_wrapper_t *res_wrapper = ik_db_wrap_pg_result(tmp, pq_exec_(db_ctx->conn, query));
    PGresult *res = res_wrapper->pg_result;

    // Check query execution status
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);  // Destructor automatically calls PQclear
        return ERR(db_ctx, IO, "Failed to create session: %s", pq_err);
    }

    // Verify we got exactly one row back
    if (PQntuples(res) != 1) {                                   // LCOV_EXCL_BR_LINE
        PANIC("Session creation returned unexpected row count"); // LCOV_EXCL_LINE
    }

    // Extract session ID from result
    const char *id_str = PQgetvalue(res, 0, 0);
    int64_t session_id = strtoll(id_str, NULL, 10);

    talloc_free(tmp);  // Destructor automatically calls PQclear

    *session_id_out = session_id;
    return OK(NULL);
}

res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    assert(db_ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(session_id_out != NULL); // LCOV_EXCL_BR_LINE

    // Create temporary context for query result
    TALLOC_CTX *tmp = tmp_ctx_create();

    // Query for most recent active session (ended_at IS NULL)
    // Order by started_at DESC, then id DESC for tiebreaker (within same transaction)
    const char *query = "SELECT id FROM sessions WHERE ended_at IS NULL "
                        "ORDER BY started_at DESC, id DESC LIMIT 1";

    ik_pg_result_wrapper_t *res_wrapper = ik_db_wrap_pg_result(tmp, pq_exec_(db_ctx->conn, query));
    PGresult *res = res_wrapper->pg_result;

    // Check query execution status
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);  // Destructor automatically calls PQclear
        return ERR(db_ctx, IO, "Failed to get active session: %s", pq_err);
    }

    // If no rows returned, there is no active session
    if (PQntuples(res) == 0) {
        talloc_free(tmp);  // Destructor automatically calls PQclear
        *session_id_out = 0; // No active session
        return OK(NULL);
    }

    // Extract session ID from result
    const char *id_str = PQgetvalue(res, 0, 0);
    int64_t session_id = strtoll(id_str, NULL, 10);

    talloc_free(tmp);  // Destructor automatically calls PQclear

    *session_id_out = session_id;
    return OK(NULL);
}
