#include "agent_zero.h"

#include "pg_result.h"

#include "../error.h"
#include "../panic.h"
#include "../uuid.h"
#include "../wrapper.h"

#include <assert.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <talloc.h>
#include <time.h>

res_t ik_db_ensure_agent_zero(ik_db_ctx_t *db, char **out_uuid)
{
    assert(db != NULL);         // LCOV_EXCL_BR_LINE
    assert(out_uuid != NULL);   // LCOV_EXCL_BR_LINE

    // Create temporary context for query results
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Check for existing root agent (parent_uuid IS NULL)
    const char *query_root = "SELECT uuid FROM agents WHERE parent_uuid IS NULL";
    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query_root, 0, NULL,
                                                   NULL, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        talloc_free(tmp);
        return ERR(db, IO, "Failed to query for root agent: %s", pq_err);
    }

    // If root agent exists, return its UUID
    int num_rows = PQntuples(res);
    if (num_rows > 0) {
        const char *existing_uuid = PQgetvalue_(res, 0, 0);
        *out_uuid = talloc_strdup(db, existing_uuid);
        if (*out_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        talloc_free(tmp);
        return OK(*out_uuid);
    }

    // No root agent found - need to create one
    // Generate new UUID for Agent 0
    char *uuid = ik_generate_uuid(db);
    if (uuid == NULL) {  // LCOV_EXCL_BR_LINE
        talloc_free(tmp);     // LCOV_EXCL_LINE
        return ERR(db, OUT_OF_MEMORY, "Failed to generate UUID for Agent 0");     // LCOV_EXCL_LINE
    }

    // Check if agent_uuid column exists in messages table
    // This column is added by messages-agent-uuid.md migration
    const char *check_column =
        "SELECT 1 FROM information_schema.columns "
        "WHERE table_name = 'messages' AND column_name = 'agent_uuid'";
    ik_pg_result_wrapper_t *column_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, check_column, 0, NULL,
                                                   NULL, NULL, NULL, 0));
    PGresult *column_res = column_wrapper->pg_result;

    bool agent_uuid_exists = false;
    if (PQresultStatus(column_res) == PGRES_TUPLES_OK && PQntuples(column_res) > 0) {     // LCOV_EXCL_BR_LINE
        agent_uuid_exists = true;
    }

    // Check for orphan messages only if agent_uuid column exists
    bool has_orphans = false;
    if (agent_uuid_exists) {     // LCOV_EXCL_BR_LINE
        const char *check_orphans = "SELECT 1 FROM messages WHERE agent_uuid IS NULL LIMIT 1";
        ik_pg_result_wrapper_t *orphan_wrapper =
            ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, check_orphans, 0, NULL,
                                                       NULL, NULL, NULL, 0));
        PGresult *orphan_res = orphan_wrapper->pg_result;

        if (PQresultStatus(orphan_res) == PGRES_TUPLES_OK) {     // LCOV_EXCL_BR_LINE
            has_orphans = (PQntuples(orphan_res) > 0);
        }
    }

    // Insert Agent 0 with parent_uuid=NULL, status='running'
    // Note: No explicit transaction handling - caller should ensure proper transaction context
    const char *insert_query =
        "INSERT INTO agents (uuid, name, parent_uuid, status, created_at, fork_message_id) "
        "VALUES ($1, NULL, NULL, 'running', $2, 0)";

    char created_at_str[32];
    snprintf(created_at_str, sizeof(created_at_str), "%" PRId64, time(NULL));

    const char *insert_params[2];
    insert_params[0] = uuid;
    insert_params[1] = created_at_str;

    ik_pg_result_wrapper_t *insert_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, insert_query, 2, NULL,
                                                   insert_params, NULL, NULL, 0));
    PGresult *insert_res = insert_wrapper->pg_result;

    if (PQresultStatus(insert_res) != PGRES_COMMAND_OK) {     // LCOV_EXCL_BR_LINE
        const char *pq_err = PQerrorMessage(db->conn);     // LCOV_EXCL_LINE
        talloc_free(tmp);     // LCOV_EXCL_LINE
        return ERR(db, IO, "Failed to insert Agent 0: %s", pq_err);     // LCOV_EXCL_LINE
    }

    // Adopt orphan messages if any exist (only if agent_uuid column exists)
    if (has_orphans && agent_uuid_exists) {     // LCOV_EXCL_BR_LINE
        const char *adopt_query = "UPDATE messages SET agent_uuid = $1 WHERE agent_uuid IS NULL";
        const char *adopt_params[1] = {uuid};

        ik_pg_result_wrapper_t *adopt_wrapper =
            ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, adopt_query, 1, NULL,
                                                       adopt_params, NULL, NULL, 0));
        PGresult *adopt_res = adopt_wrapper->pg_result;

        if (PQresultStatus(adopt_res) != PGRES_COMMAND_OK) {     // LCOV_EXCL_BR_LINE
            const char *pq_err = PQerrorMessage(db->conn);     // LCOV_EXCL_LINE
            talloc_free(tmp);     // LCOV_EXCL_LINE
            return ERR(db, IO, "Failed to adopt orphan messages: %s", pq_err);     // LCOV_EXCL_LINE
        }
    }

    *out_uuid = uuid;
    talloc_free(tmp);
    return OK(*out_uuid);
}
