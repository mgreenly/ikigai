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
