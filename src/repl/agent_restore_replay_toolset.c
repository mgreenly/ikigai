/**
 * @file agent_restore_replay_toolset.c
 * @brief Toolset replay logic for agent restoration
 */

#include "agent_restore_replay_toolset.h"

#include "../agent.h"
#include "../db/connection.h"
#include "../db/pg_result.h"
#include "../error.h"
#include "../tmp_ctx.h"
#include "../wrapper.h"
#include "../wrapper_json.h"

#include <assert.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

static void replay_toolset_command(ik_agent_ctx_t *agent, const char *args)
{
    if (agent->toolset_filter != NULL) {
        talloc_free(agent->toolset_filter);
        agent->toolset_filter = NULL;
        agent->toolset_count = 0;
    }

    char *work = talloc_strdup(agent, args);
    if (work == NULL) return;  // LCOV_EXCL_LINE

    size_t capacity = 16;
    char **tools = talloc_array(agent, char *, (unsigned int)capacity);
    if (tools == NULL) {     // LCOV_EXCL_LINE
        talloc_free(work);     // LCOV_EXCL_LINE
        return;     // LCOV_EXCL_LINE
    }
    size_t count = 0;

    char *saveptr = NULL;
    char *token = strtok_r(work, " ,", &saveptr);
    while (token != NULL) {
        if (count >= capacity) {
            capacity *= 2;
            tools = talloc_realloc(agent, tools, char *, (unsigned int)capacity);
            if (tools == NULL) {     // LCOV_EXCL_LINE
                talloc_free(work);     // LCOV_EXCL_LINE
                return;     // LCOV_EXCL_LINE
            }
        }

        tools[count] = talloc_strdup(agent, token);
        if (tools[count] == NULL) {     // LCOV_EXCL_LINE
            talloc_free(work);     // LCOV_EXCL_LINE
            return;     // LCOV_EXCL_LINE
        }
        count++;

        token = strtok_r(NULL, " ,", &saveptr);
    }

    agent->toolset_filter = talloc_realloc(agent, tools, char *, (unsigned int)count);
    if (count > 0 && agent->toolset_filter == NULL) {     // LCOV_EXCL_LINE
        talloc_free(work);     // LCOV_EXCL_LINE
        return;     // LCOV_EXCL_LINE
    }
    agent->toolset_count = count;

    talloc_free(work);
}

res_t ik_agent_replay_toolset(ik_db_ctx_t *db, ik_agent_ctx_t *agent)
{
    assert(db != NULL);     // LCOV_EXCL_BR_LINE
    assert(agent != NULL);     // LCOV_EXCL_BR_LINE
    assert(agent->uuid != NULL);     // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query =
        "SELECT data "
        "FROM messages "
        "WHERE agent_uuid = $1 "
        "  AND kind = 'command' "
        "  AND data->>'command' = 'toolset' "
        "ORDER BY created_at DESC "
        "LIMIT 1";

    const char *params[1] = {agent->uuid};

    ik_pg_result_wrapper_t *cmd_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 1, NULL,
                                                  params, NULL, NULL, 0));
    PGresult *cmd_res = cmd_wrapper->pg_result;

    if (PQresultStatus(cmd_res) != PGRES_TUPLES_OK) {     // LCOV_EXCL_BR_LINE
        const char *pq_err = PQerrorMessage(db->conn);     // LCOV_EXCL_LINE
        talloc_free(tmp);     // LCOV_EXCL_LINE
        return ERR(db, IO, "Failed to query toolset commands: %s", pq_err);     // LCOV_EXCL_LINE
    }

    int cmd_rows = PQntuples(cmd_res);
    if (cmd_rows > 0) {
        const char *data_json = PQgetvalue_(cmd_res, 0, 0);
        yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
        if (doc != NULL) {
            yyjson_val *root = yyjson_doc_get_root_(doc);
            yyjson_val *args = yyjson_obj_get_(root, "args");
            if (args != NULL && yyjson_is_str(args)) {
                const char *args_str = yyjson_get_str(args);
                replay_toolset_command(agent, args_str);
            }
            yyjson_doc_free(doc);
        }
    }

    talloc_free(tmp);
    return OK(NULL);
}
