#include "apps/ikigai/db/session_summary.h"
#include "apps/ikigai/db/pg_result.h"
#include "apps/ikigai/summary.h"
#include "shared/error.h"
#include "apps/ikigai/tmp_ctx.h"
#include "shared/wrapper.h"
#include "apps/ikigai/wrapper_postgres.h"

#include "shared/poison.h"

#include <assert.h>
#include <libpq-fe.h>
#include <stdlib.h>
#include <talloc.h>

res_t ik_db_session_summary_insert(ik_db_ctx_t *db,
                                   const char *agent_uuid,
                                   const char *summary,
                                   int64_t start_msg_id,
                                   int64_t end_msg_id,
                                   int32_t token_count)
{
    assert(db != NULL);           // LCOV_EXCL_BR_LINE
    assert(db->conn != NULL);     // LCOV_EXCL_BR_LINE
    assert(agent_uuid != NULL);   // LCOV_EXCL_BR_LINE
    assert(summary != NULL);      // LCOV_EXCL_BR_LINE
    assert(start_msg_id > 0);     // LCOV_EXCL_BR_LINE
    assert(end_msg_id > 0);       // LCOV_EXCL_BR_LINE
    assert(token_count >= 0);     // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    // Insert the new summary
    const char *insert_query =
        "INSERT INTO session_summaries "
        "  (agent_uuid, summary, start_msg_id, end_msg_id, token_count) "
        "VALUES ($1, $2, $3, $4, $5)";

    char *start_str = talloc_asprintf(tmp, "%lld", (long long)start_msg_id);
    if (start_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *end_str = talloc_asprintf(tmp, "%lld", (long long)end_msg_id);
    if (end_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *token_str = talloc_asprintf(tmp, "%d", token_count);
    if (token_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    const char *params[5];
    params[0] = agent_uuid;
    params[1] = summary;
    params[2] = start_str;
    params[3] = end_str;
    params[4] = token_str;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp,
                             pq_exec_params_(db->conn, insert_query, 5,
                                             NULL, params, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus_(res) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        res_t error_res = ERR(db, IO, "Session summary insert failed: %s", pq_err);
        talloc_free(tmp);
        return error_res;
    }

    // Enforce cap: delete oldest rows if count exceeds IK_SUMMARY_PREVIOUS_SESSION_MAX_COUNT
    char *cap_query_str = talloc_asprintf(tmp,
        "DELETE FROM session_summaries "
        "WHERE agent_uuid = $1 "
        "  AND id NOT IN ( "
        "    SELECT id FROM session_summaries "
        "    WHERE agent_uuid = $1 "
        "    ORDER BY created_at DESC "
        "    LIMIT %d "
        "  )",
        IK_SUMMARY_PREVIOUS_SESSION_MAX_COUNT);
    if (cap_query_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    const char *cap_params[1];
    cap_params[0] = agent_uuid;

    ik_pg_result_wrapper_t *cap_wrapper =
        ik_db_wrap_pg_result(tmp,
                             pq_exec_params_(db->conn, cap_query_str, 1,
                                             NULL, cap_params, NULL, NULL, 0));
    PGresult *cap_res = cap_wrapper->pg_result;

    if (PQresultStatus_(cap_res) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        res_t error_res = ERR(db, IO, "Session summary cap enforcement failed: %s", pq_err);
        talloc_free(tmp);
        return error_res;
    }

    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_session_summary_load(ik_db_ctx_t *db,
                                 TALLOC_CTX *ctx,
                                 const char *agent_uuid,
                                 ik_session_summary_t ***out,
                                 size_t *count)
{
    assert(db != NULL);          // LCOV_EXCL_BR_LINE
    assert(db->conn != NULL);    // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);         // LCOV_EXCL_BR_LINE
    assert(agent_uuid != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);         // LCOV_EXCL_BR_LINE
    assert(count != NULL);       // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query =
        "SELECT id, agent_uuid, summary, start_msg_id, end_msg_id, token_count "
        "FROM session_summaries "
        "WHERE agent_uuid = $1 "
        "ORDER BY created_at ASC";

    const char *params[1];
    params[0] = agent_uuid;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp,
                             pq_exec_params_(db->conn, query, 1,
                                             NULL, params, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus_(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        res_t error_res = ERR(db, IO, "Session summary load failed: %s", pq_err);
        talloc_free(tmp);
        return error_res;
    }

    int nrows = PQntuples(res);
    *count = (size_t)nrows;

    if (nrows == 0) {
        *out = NULL;
        talloc_free(tmp);
        return OK(NULL);
    }

    ik_session_summary_t **summaries =
        talloc_zero_array(ctx, ik_session_summary_t *, (unsigned int)nrows);
    if (summaries == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (int i = 0; i < nrows; i++) {
        ik_session_summary_t *s = talloc_zero(summaries, ik_session_summary_t);
        if (s == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        s->id = strtoll(PQgetvalue(res, i, 0), NULL, 10);

        s->agent_uuid = talloc_strdup(s, PQgetvalue(res, i, 1));
        if (s->agent_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        s->summary = talloc_strdup(s, PQgetvalue(res, i, 2));
        if (s->summary == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        s->start_msg_id = strtoll(PQgetvalue(res, i, 3), NULL, 10);
        s->end_msg_id = strtoll(PQgetvalue(res, i, 4), NULL, 10);
        s->token_count = (int32_t)strtol(PQgetvalue(res, i, 5), NULL, 10);

        summaries[i] = s;
    }

    *out = summaries;
    talloc_free(tmp);
    return OK(NULL);
}
