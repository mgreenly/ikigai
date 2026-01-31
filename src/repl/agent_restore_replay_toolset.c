/**
 * @file agent_restore_replay_toolset.c
 * @brief Toolset replay logic for agent restoration
 */

#include "agent_restore_replay_toolset.h"

#include "../agent.h"
#include "../db/connection.h"
#include "../db/pg_result.h"
#include "../debug_log.h"
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

// Replay toolset from JSON array (used for fork-inherited toolsets)
static void replay_toolset_from_json_array(ik_agent_ctx_t *agent, yyjson_val *arr)
{
    if (agent->toolset_filter != NULL) {
        talloc_free(agent->toolset_filter);
        agent->toolset_filter = NULL;
        agent->toolset_count = 0;
    }

    if (!yyjson_is_arr(arr)) return;

    size_t arr_len = yyjson_arr_size(arr);
    if (arr_len == 0) return;

    char **tools = talloc_array(agent, char *, (unsigned int)arr_len);
    if (tools == NULL) return;  // LCOV_EXCL_LINE

    size_t count = 0;
    yyjson_val *val;
    yyjson_arr_iter iter;
    yyjson_arr_iter_init(arr, &iter);

    while ((val = yyjson_arr_iter_next(&iter)) != NULL) {
        if (yyjson_is_str(val)) {
            const char *tool_name = yyjson_get_str(val);
            tools[count] = talloc_strdup(agent, tool_name);
            if (tools[count] == NULL) {  // LCOV_EXCL_LINE
                talloc_free(tools);      // LCOV_EXCL_LINE
                return;                  // LCOV_EXCL_LINE
            }
            count++;
        }
    }

    if (count > 0) {
        agent->toolset_filter = talloc_realloc(agent, tools, char *, (unsigned int)count);
        if (agent->toolset_filter == NULL) {  // LCOV_EXCL_LINE
            talloc_free(tools);               // LCOV_EXCL_LINE
            return;                           // LCOV_EXCL_LINE
        }
        agent->toolset_count = count;
    } else {
        talloc_free(tools);
    }
}

res_t ik_agent_replay_toolset(ik_db_ctx_t *db, ik_agent_ctx_t *agent)
{
    assert(db != NULL);     // LCOV_EXCL_BR_LINE
    assert(agent != NULL);     // LCOV_EXCL_BR_LINE
    assert(agent->uuid != NULL);     // LCOV_EXCL_BR_LINE

    DEBUG_LOG("replay_toolset: agent_uuid=%s parent_uuid=%s",
              agent->uuid, agent->parent_uuid ? agent->parent_uuid : "(null)");

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
    DEBUG_LOG("replay_toolset: found %d toolset command rows", cmd_rows);

    if (cmd_rows > 0) {
        const char *data_json = PQgetvalue_(cmd_res, 0, 0);
        DEBUG_LOG("replay_toolset: data_json=%s", data_json);
        yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
        if (doc != NULL) {
            yyjson_val *root = yyjson_doc_get_root_(doc);
            yyjson_val *args = yyjson_obj_get_(root, "args");
            if (args != NULL && yyjson_is_str(args)) {
                const char *args_str = yyjson_get_str(args);
                DEBUG_LOG("replay_toolset: replaying args=%s", args_str);
                replay_toolset_command(agent, args_str);
            }
            yyjson_doc_free(doc);
        }
    } else if (agent->parent_uuid != NULL) {
        // No explicit toolset command - check fork message for inherited toolset
        DEBUG_LOG("replay_toolset: no toolset command, checking fork message for inherited toolset");

        const char *fork_query =
            "SELECT data "
            "FROM messages "
            "WHERE agent_uuid = $1 "
            "  AND kind = 'fork' "
            "ORDER BY created_at ASC "
            "LIMIT 1";

        ik_pg_result_wrapper_t *fork_wrapper =
            ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, fork_query, 1, NULL,
                                                      params, NULL, NULL, 0));
        PGresult *fork_res = fork_wrapper->pg_result;

        if (PQresultStatus(fork_res) == PGRES_TUPLES_OK && PQntuples(fork_res) > 0) {
            const char *fork_data = PQgetvalue_(fork_res, 0, 0);
            DEBUG_LOG("replay_toolset: fork data=%s", fork_data);

            yyjson_doc *fork_doc = yyjson_read(fork_data, strlen(fork_data), 0);
            if (fork_doc != NULL) {
                yyjson_val *fork_root = yyjson_doc_get_root_(fork_doc);
                yyjson_val *toolset_arr = yyjson_obj_get_(fork_root, "toolset_filter");
                if (toolset_arr != NULL && yyjson_is_arr(toolset_arr)) {
                    DEBUG_LOG("replay_toolset: found toolset_filter in fork message, count=%zu",
                              yyjson_arr_size(toolset_arr));
                    replay_toolset_from_json_array(agent, toolset_arr);
                }
                yyjson_doc_free(fork_doc);
            }
        }
    }

    DEBUG_LOG("replay_toolset: after replay toolset_filter=%p toolset_count=%zu",
              (void *)agent->toolset_filter, agent->toolset_count);

    talloc_free(tmp);
    return OK(NULL);
}
