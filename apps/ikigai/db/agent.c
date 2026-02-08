#include "apps/ikigai/db/agent.h"

#include "apps/ikigai/db/agent_row.h"
#include "apps/ikigai/db/pg_result.h"
#include "apps/ikigai/shared.h"

#include "shared/error.h"
#include "apps/ikigai/tmp_ctx.h"
#include "apps/ikigai/uuid.h"
#include "shared/wrapper.h"
#include "apps/ikigai/wrapper_postgres.h"

#include "shared/panic.h"
#include "shared/poison.h"

#include <assert.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <time.h>
res_t ik_db_agent_insert(ik_db_ctx_t *db_ctx, const ik_agent_ctx_t *agent)
{
    assert(db_ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(agent != NULL);         // LCOV_EXCL_BR_LINE
    assert(agent->uuid != NULL);   // LCOV_EXCL_BR_LINE
    assert(agent->shared != NULL);  // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query =
        "INSERT INTO agents (session_id, uuid, name, parent_uuid, status, created_at, fork_message_id, "
        "provider, model, thinking_level) "
        "VALUES ($1, $2, $3, $4, 'running', $5, $6, $7, $8, $9)";

    char session_id_str[32];
    char created_at_str[32];
    char fork_message_id_str[32];
    snprintf(session_id_str, sizeof(session_id_str), "%" PRId64, agent->shared->session_id);
    snprintf(created_at_str, sizeof(created_at_str), "%" PRId64, agent->created_at);
    snprintf(fork_message_id_str, sizeof(fork_message_id_str), "%" PRId64, agent->fork_message_id);

    const char *thinking_level_param = NULL;
    if (agent->thinking_level != 0) {
        switch (agent->thinking_level) {
            case 1: thinking_level_param = "low"; break;
            case 2: thinking_level_param = "med"; break;
            case 3: thinking_level_param = "high"; break;
            default: thinking_level_param = "none"; break;
        }
    }

    const char *param_values[9];
    param_values[0] = session_id_str;
    param_values[1] = agent->uuid;
    param_values[2] = agent->name;
    param_values[3] = agent->parent_uuid;
    param_values[4] = created_at_str;
    param_values[5] = fork_message_id_str;
    param_values[6] = agent->provider;
    param_values[7] = agent->model;
    param_values[8] = thinking_level_param;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 9, NULL,
                                                  param_values, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Failed to insert agent: %s", pq_err);
    }

    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_agent_mark_dead(ik_db_ctx_t *db_ctx, const char *uuid)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid != NULL);    // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    // Update agent status to 'dead' and set ended_at timestamp
    // Only update if status is 'running' to make it idempotent
    const char *query =
        "UPDATE agents SET status = 'dead', ended_at = $1 "
        "WHERE uuid = $2 AND status = 'running'";

    // Get current timestamp
    char ended_at_str[32];
    snprintf(ended_at_str, sizeof(ended_at_str), "%" PRId64, time(NULL));

    const char *param_values[2];
    param_values[0] = ended_at_str;
    param_values[1] = uuid;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 2, NULL,
                                                  param_values, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    // Check query execution status
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Failed to mark agent as dead: %s", pq_err);
    }

    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_agent_get(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                      const char *uuid, ik_db_agent_row_t **out)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(uuid != NULL);    // LCOV_EXCL_BR_LINE
    assert(out != NULL);     // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    // Query for agent by UUID
    const char *query =
        "SELECT uuid, name, parent_uuid, fork_message_id, status::text, "
        "created_at, COALESCE(ended_at, 0) as ended_at, "
        "provider, model, thinking_level, idle "
        "FROM agents WHERE uuid = $1";

    const char *param_values[1];
    param_values[0] = uuid;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 1, NULL,
                                                  param_values, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    // Check query execution status
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Failed to get agent: %s", pq_err);
    }

    // Check if agent found
    int num_rows = PQntuples(res);
    if (num_rows == 0) {
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Agent not found: %s", uuid);
    }

    // Parse row using shared function
    res_t parse_result = ik_db_agent_parse_row(db_ctx, ctx, res, 0, out);
    talloc_free(tmp);
    if (is_err(&parse_result)) {
        return parse_result;
    }

    return OK(NULL);
}

res_t ik_db_agent_list_running(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                               ik_db_agent_row_t ***out, size_t *count)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(out != NULL);     // LCOV_EXCL_BR_LINE
    assert(count != NULL);   // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    // Query for all running agents
    const char *query =
        "SELECT uuid, name, parent_uuid, fork_message_id, status::text, "
        "created_at, COALESCE(ended_at, 0) as ended_at, "
        "provider, model, thinking_level, idle "
        "FROM agents WHERE status = 'running' ORDER BY created_at";

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 0, NULL,
                                                  NULL, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    // Check query execution status
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Failed to list running agents: %s", pq_err);
    }

    // Get number of rows
    int num_rows = PQntuples(res);
    *count = (size_t)num_rows;

    if (num_rows == 0) {
        *out = NULL;
        talloc_free(tmp);
        return OK(NULL);
    }

    // Allocate array of pointers
    ik_db_agent_row_t **rows = talloc_zero_array(ctx, ik_db_agent_row_t *, (unsigned int)num_rows);
    if (rows == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Process each row
    for (int i = 0; i < num_rows; i++) {
        res_t parse_result = ik_db_agent_parse_row(db_ctx, rows, res, i, &rows[i]);
        if (is_err(&parse_result)) {
            talloc_free(tmp);
            return parse_result;
        }
    }

    *out = rows;
    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_agent_list_active(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, int64_t session_id,
                               ik_db_agent_row_t ***out, size_t *count)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(out != NULL);     // LCOV_EXCL_BR_LINE
    assert(count != NULL);   // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    // Query for all active agents (running and dead) for this session
    const char *query =
        "SELECT uuid, name, parent_uuid, fork_message_id, status::text, "
        "created_at, COALESCE(ended_at, 0) as ended_at, "
        "provider, model, thinking_level, idle "
        "FROM agents WHERE session_id = $1 AND status IN ('running', 'dead') ORDER BY created_at";

    // Convert session_id to string for parameter
    char session_id_str[32];
    snprintf(session_id_str, sizeof(session_id_str), "%" PRId64, session_id);

    const char *param_values[1];
    param_values[0] = session_id_str;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 1, NULL,
                                                  param_values, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    // Check query execution status
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Failed to list active agents: %s", pq_err);
    }

    // Get number of rows
    int num_rows = PQntuples(res);
    *count = (size_t)num_rows;

    if (num_rows == 0) {
        *out = NULL;
        talloc_free(tmp);
        return OK(NULL);
    }

    // Allocate array of pointers
    ik_db_agent_row_t **rows = talloc_zero_array(ctx, ik_db_agent_row_t *, (unsigned int)num_rows);
    if (rows == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Process each row
    for (int i = 0; i < num_rows; i++) {
        res_t parse_result = ik_db_agent_parse_row(db_ctx, rows, res, i, &rows[i]);
        if (is_err(&parse_result)) {
            talloc_free(tmp);
            return parse_result;
        }
    }

    *out = rows;
    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_agent_get_last_message_id(ik_db_ctx_t *db_ctx, const char *agent_uuid,
                                      int64_t *out_message_id)
{
    assert(db_ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(agent_uuid != NULL);      // LCOV_EXCL_BR_LINE
    assert(out_message_id != NULL);  // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    // Query for maximum message ID for this agent
    const char *query = "SELECT COALESCE(MAX(id), 0) FROM messages WHERE agent_uuid = $1";

    const char *param_values[1];
    param_values[0] = agent_uuid;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 1, NULL,
                                                  param_values, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    // Check query execution status
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Failed to get last message ID: %s", pq_err);
    }

    // Parse result
    const char *id_str = PQgetvalue_(res, 0, 0);
    if (sscanf(id_str, "%lld", (long long *)out_message_id) != 1) {
        talloc_free(tmp);
        return ERR(db_ctx, PARSE, "Failed to parse message ID");
    }

    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_agent_update_provider(ik_db_ctx_t *db_ctx, const char *uuid,
                                  const char *provider, const char *model,
                                  const char *thinking_level)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid != NULL);    // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    // Update provider configuration for agent
    const char *query =
        "UPDATE agents SET provider = $1, model = $2, thinking_level = $3 "
        "WHERE uuid = $4";

    const char *param_values[4];
    param_values[0] = provider;         // Can be NULL
    param_values[1] = model;            // Can be NULL
    param_values[2] = thinking_level;   // Can be NULL
    param_values[3] = uuid;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 4, NULL,
                                                  param_values, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    // Check query execution status
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Failed to update agent provider: %s", pq_err);
    }

    // Note: UPDATE affecting 0 rows (agent not found) is not an error
    // as per the task specification

    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_agent_set_idle(ik_db_ctx_t *db_ctx, const char *uuid, bool idle)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid != NULL);    // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query = "UPDATE agents SET idle = $1 WHERE uuid = $2";
    const char *idle_str = idle ? "true" : "false";

    const char *param_values[2];
    param_values[0] = idle_str;
    param_values[1] = uuid;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 2, NULL,
                                                  param_values, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Failed to set idle: %s", pq_err);
    }

    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_agent_mark_reaped(ik_db_ctx_t *db_ctx, const char *uuid)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid != NULL);    // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query = "UPDATE agents SET status = 'reaped' WHERE uuid = $1";

    const char *param_values[1];
    param_values[0] = uuid;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 1, NULL,
                                                  param_values, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Failed to mark agent as reaped: %s", pq_err);
    }

    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_agent_reap_all_dead(ik_db_ctx_t *db_ctx)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query = "UPDATE agents SET status = 'reaped' WHERE status = 'dead'";

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_(db_ctx->conn, query));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Failed to reap dead agents: %s", pq_err);
    }

    talloc_free(tmp);
    return OK(NULL);
}
