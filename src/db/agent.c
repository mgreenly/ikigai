#include "agent.h"

#include "pg_result.h"

#include "../error.h"
#include "../panic.h"
#include "../wrapper.h"

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

    // Create temporary context for query result
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Insert agent into registry with status='running'
    const char *query =
        "INSERT INTO agents (uuid, name, parent_uuid, status, created_at, fork_message_id) "
        "VALUES ($1, $2, $3, 'running', $4, $5)";

    // Convert created_at and fork_message_id to strings for parameters
    char created_at_str[32];
    char fork_message_id_str[32];
    snprintf(created_at_str, sizeof(created_at_str), "%" PRId64, agent->created_at);
    snprintf(fork_message_id_str, sizeof(fork_message_id_str), "%" PRId64, agent->fork_message_id);

    const char *param_values[5];
    param_values[0] = agent->uuid;
    param_values[1] = agent->name;            // Can be NULL
    param_values[2] = agent->parent_uuid;     // Can be NULL for root agent
    param_values[3] = created_at_str;
    param_values[4] = fork_message_id_str;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 5, NULL,
                                                   param_values, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    // Check query execution status
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);  // Destructor automatically calls PQclear
        return ERR(db_ctx, IO, "Failed to insert agent: %s", pq_err);
    }

    talloc_free(tmp);  // Destructor automatically calls PQclear
    return OK(NULL);
}

res_t ik_db_agent_mark_dead(ik_db_ctx_t *db_ctx, const char *uuid)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid != NULL);    // LCOV_EXCL_BR_LINE

    // Create temporary context for query result
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

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
        talloc_free(tmp);  // Destructor automatically calls PQclear
        return ERR(db_ctx, IO, "Failed to mark agent as dead: %s", pq_err);
    }

    talloc_free(tmp);  // Destructor automatically calls PQclear
    return OK(NULL);
}

res_t ik_db_agent_get(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                      const char *uuid, ik_db_agent_row_t **out)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(mem_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid != NULL);     // LCOV_EXCL_BR_LINE
    assert(out != NULL);      // LCOV_EXCL_BR_LINE

    // Create temporary context for query
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Query for agent by UUID
    const char *query =
        "SELECT uuid, name, parent_uuid, fork_message_id, status::text, "
        "created_at, COALESCE(ended_at, 0) as ended_at "
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

    // Allocate row on provided context
    ik_db_agent_row_t *row = talloc_zero(mem_ctx, ik_db_agent_row_t);
    if (row == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Extract fields
    row->uuid = talloc_strdup(row, PQgetvalue_(res, 0, 0));
    if (row->uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!PQgetisnull(res, 0, 1)) {
        row->name = talloc_strdup(row, PQgetvalue_(res, 0, 1));
        if (row->name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        row->name = NULL;
    }

    if (!PQgetisnull(res, 0, 2)) {
        row->parent_uuid = talloc_strdup(row, PQgetvalue_(res, 0, 2));
        if (row->parent_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        row->parent_uuid = NULL;
    }

    row->fork_message_id = talloc_strdup(row, PQgetvalue_(res, 0, 3));
    if (row->fork_message_id == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    row->status = talloc_strdup(row, PQgetvalue_(res, 0, 4));
    if (row->status == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    const char *created_at_str = PQgetvalue_(res, 0, 5);
    if (sscanf(created_at_str, "%lld", (long long *)&row->created_at) != 1) {
        talloc_free(tmp);
        return ERR(db_ctx, PARSE, "Failed to parse created_at");
    }

    const char *ended_at_str = PQgetvalue_(res, 0, 6);
    if (sscanf(ended_at_str, "%lld", (long long *)&row->ended_at) != 1) {
        talloc_free(tmp);
        return ERR(db_ctx, PARSE, "Failed to parse ended_at");
    }

    *out = row;
    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_agent_list_running(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                               ik_db_agent_row_t ***out, size_t *count)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(mem_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);      // LCOV_EXCL_BR_LINE
    assert(count != NULL);    // LCOV_EXCL_BR_LINE

    // Create temporary context for query
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Query for all running agents
    const char *query =
        "SELECT uuid, name, parent_uuid, fork_message_id, status::text, "
        "created_at, COALESCE(ended_at, 0) as ended_at "
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
    ik_db_agent_row_t **rows = talloc_array(mem_ctx, ik_db_agent_row_t *, (unsigned int)num_rows);
    if (rows == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Process each row
    for (int i = 0; i < num_rows; i++) {
        ik_db_agent_row_t *row = talloc_zero(rows, ik_db_agent_row_t);
        if (row == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        // Extract fields
        row->uuid = talloc_strdup(row, PQgetvalue_(res, i, 0));
        if (row->uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        if (!PQgetisnull(res, i, 1)) {
            row->name = talloc_strdup(row, PQgetvalue_(res, i, 1));
            if (row->name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        } else {
            row->name = NULL;
        }

        if (!PQgetisnull(res, i, 2)) {
            row->parent_uuid = talloc_strdup(row, PQgetvalue_(res, i, 2));
            if (row->parent_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        } else {
            row->parent_uuid = NULL;
        }

        row->fork_message_id = talloc_strdup(row, PQgetvalue_(res, i, 3));
        if (row->fork_message_id == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        row->status = talloc_strdup(row, PQgetvalue_(res, i, 4));
        if (row->status == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        const char *created_at_str = PQgetvalue_(res, i, 5);
        if (sscanf(created_at_str, "%lld", (long long *)&row->created_at) != 1) {
            talloc_free(tmp);
            return ERR(db_ctx, PARSE, "Failed to parse created_at");
        }

        const char *ended_at_str = PQgetvalue_(res, i, 6);
        if (sscanf(ended_at_str, "%lld", (long long *)&row->ended_at) != 1) {
            talloc_free(tmp);
            return ERR(db_ctx, PARSE, "Failed to parse ended_at");
        }

        rows[i] = row;
    }

    *out = rows;
    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_agent_get_children(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                               const char *parent_uuid,
                               ik_db_agent_row_t ***out, size_t *count)
{
    assert(db_ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(mem_ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(parent_uuid != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);          // LCOV_EXCL_BR_LINE
    assert(count != NULL);        // LCOV_EXCL_BR_LINE

    // Create temporary context for query
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Query for children of parent_uuid
    const char *query =
        "SELECT uuid, name, parent_uuid, fork_message_id, status::text, "
        "created_at, COALESCE(ended_at, 0) as ended_at "
        "FROM agents WHERE parent_uuid = $1 ORDER BY created_at";

    const char *param_values[1];
    param_values[0] = parent_uuid;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 1, NULL,
                                                   param_values, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    // Check query execution status
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Failed to get children: %s", pq_err);
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
    ik_db_agent_row_t **rows = talloc_array(mem_ctx, ik_db_agent_row_t *, (unsigned int)num_rows);
    if (rows == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Process each row
    for (int i = 0; i < num_rows; i++) {
        ik_db_agent_row_t *row = talloc_zero(rows, ik_db_agent_row_t);
        if (row == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        // Extract fields
        row->uuid = talloc_strdup(row, PQgetvalue_(res, i, 0));
        if (row->uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        if (!PQgetisnull(res, i, 1)) {
            row->name = talloc_strdup(row, PQgetvalue_(res, i, 1));
            if (row->name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        } else {
            row->name = NULL;
        }

        if (!PQgetisnull(res, i, 2)) {
            row->parent_uuid = talloc_strdup(row, PQgetvalue_(res, i, 2));
            if (row->parent_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        } else {
            row->parent_uuid = NULL;
        }

        row->fork_message_id = talloc_strdup(row, PQgetvalue_(res, i, 3));
        if (row->fork_message_id == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        row->status = talloc_strdup(row, PQgetvalue_(res, i, 4));
        if (row->status == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        const char *created_at_str = PQgetvalue_(res, i, 5);
        if (sscanf(created_at_str, "%lld", (long long *)&row->created_at) != 1) {
            talloc_free(tmp);
            return ERR(db_ctx, PARSE, "Failed to parse created_at");
        }

        const char *ended_at_str = PQgetvalue_(res, i, 6);
        if (sscanf(ended_at_str, "%lld", (long long *)&row->ended_at) != 1) {
            talloc_free(tmp);
            return ERR(db_ctx, PARSE, "Failed to parse ended_at");
        }

        rows[i] = row;
    }

    *out = rows;
    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_agent_get_parent(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                              const char *uuid, ik_db_agent_row_t **out)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(mem_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid != NULL);     // LCOV_EXCL_BR_LINE
    assert(out != NULL);      // LCOV_EXCL_BR_LINE

    // Create temporary context for query
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Query for parent via JOIN
    const char *query =
        "SELECT p.uuid, p.name, p.parent_uuid, p.fork_message_id, p.status::text, "
        "p.created_at, COALESCE(p.ended_at, 0) as ended_at "
        "FROM agents c "
        "JOIN agents p ON c.parent_uuid = p.uuid "
        "WHERE c.uuid = $1";

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
        return ERR(db_ctx, IO, "Failed to get parent: %s", pq_err);
    }

    // Check if parent found
    int num_rows = PQntuples(res);
    if (num_rows == 0) {
        // No parent (root agent)
        *out = NULL;
        talloc_free(tmp);
        return OK(NULL);
    }

    // Allocate row on provided context
    ik_db_agent_row_t *row = talloc_zero(mem_ctx, ik_db_agent_row_t);
    if (row == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Extract fields
    row->uuid = talloc_strdup(row, PQgetvalue_(res, 0, 0));
    if (row->uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!PQgetisnull(res, 0, 1)) {
        row->name = talloc_strdup(row, PQgetvalue_(res, 0, 1));
        if (row->name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        row->name = NULL;
    }

    if (!PQgetisnull(res, 0, 2)) {
        row->parent_uuid = talloc_strdup(row, PQgetvalue_(res, 0, 2));
        if (row->parent_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        row->parent_uuid = NULL;
    }

    row->fork_message_id = talloc_strdup(row, PQgetvalue_(res, 0, 3));
    if (row->fork_message_id == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    row->status = talloc_strdup(row, PQgetvalue_(res, 0, 4));
    if (row->status == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    const char *created_at_str = PQgetvalue_(res, 0, 5);
    if (sscanf(created_at_str, "%lld", (long long *)&row->created_at) != 1) {
        talloc_free(tmp);
        return ERR(db_ctx, PARSE, "Failed to parse created_at");
    }

    const char *ended_at_str = PQgetvalue_(res, 0, 6);
    if (sscanf(ended_at_str, "%lld", (long long *)&row->ended_at) != 1) {
        talloc_free(tmp);
        return ERR(db_ctx, PARSE, "Failed to parse ended_at");
    }

    *out = row;
    talloc_free(tmp);
    return OK(NULL);
}
