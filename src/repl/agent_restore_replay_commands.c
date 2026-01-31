// Agent restoration command replay helpers
#include "agent_restore_replay.h"

#include "../db/connection.h"
#include "../db/pg_result.h"
#include "../error.h"
#include "../logger.h"
#include "../providers/provider.h"
#include "../tmp_ctx.h"
#include "../wrapper.h"
#include "../wrapper_json.h"

#include <assert.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

// Forward declarations
static void replay_fork_event(ik_agent_ctx_t *agent, yyjson_val *root);
static void replay_fork_toolset(ik_agent_ctx_t *agent, yyjson_val *toolset_val);
static void replay_model_command(ik_agent_ctx_t *agent, const char *args, ik_logger_t *logger);
static void replay_pin_command(ik_agent_ctx_t *agent, const char *args);
static void replay_unpin_command(ik_agent_ctx_t *agent, const char *args);

// Helper: Replay fork event - extract pinned_paths from child fork event
static void replay_fork_event(ik_agent_ctx_t *agent, yyjson_val *root)
{
    yyjson_val *role_val = yyjson_obj_get_(root, "role");
    const char *role = yyjson_get_str(role_val);

    if (role == NULL || strcmp(role, "child") != 0) {     // LCOV_EXCL_BR_LINE
        return;     // LCOV_EXCL_LINE
    }

    yyjson_val *pins_val = yyjson_obj_get_(root, "pinned_paths");
    if (yyjson_is_arr(pins_val)) {
        size_t arr_size = yyjson_arr_size(pins_val);

        if (agent->pinned_paths != NULL) {     // LCOV_EXCL_BR_LINE
            talloc_free(agent->pinned_paths);     // LCOV_EXCL_LINE
            agent->pinned_paths = NULL;     // LCOV_EXCL_LINE
            agent->pinned_count = 0;     // LCOV_EXCL_LINE
        }

        if (arr_size > 0) {
            agent->pinned_paths = talloc_zero_array(agent, char *, (unsigned int)arr_size);
            if (agent->pinned_paths == NULL) return;  // LCOV_EXCL_LINE

            size_t idx = 0;
            size_t max_idx = 0;
            yyjson_val *path_val = NULL;
            yyjson_arr_foreach(pins_val, idx, max_idx, path_val) {     // LCOV_EXCL_BR_LINE
                const char *path_str = yyjson_get_str(path_val);
                if (path_str != NULL) {     // LCOV_EXCL_BR_LINE
                    agent->pinned_paths[agent->pinned_count] = talloc_strdup(agent, path_str);
                    if (agent->pinned_paths[agent->pinned_count] == NULL) return;  // LCOV_EXCL_LINE
                    agent->pinned_count++;
                }
            }
        }
    }

    yyjson_val *toolset_val = yyjson_obj_get_(root, "toolset_filter");
    replay_fork_toolset(agent, toolset_val);
}

// Helper: Replay toolset from fork event
static void replay_fork_toolset(ik_agent_ctx_t *agent, yyjson_val *toolset_val)
{
    if (!yyjson_is_arr(toolset_val)) {
        return;
    }

    size_t arr_size = yyjson_arr_size(toolset_val);

    if (agent->toolset_filter != NULL) {     // LCOV_EXCL_BR_LINE
        talloc_free(agent->toolset_filter);     // LCOV_EXCL_LINE
        agent->toolset_filter = NULL;     // LCOV_EXCL_LINE
        agent->toolset_count = 0;     // LCOV_EXCL_LINE
    }

    if (arr_size > 0) {
        agent->toolset_filter = talloc_zero_array(agent, char *, (unsigned int)arr_size);
        if (agent->toolset_filter == NULL) return;  // LCOV_EXCL_LINE

        size_t idx = 0;
        size_t max_idx = 0;
        yyjson_val *tool_val = NULL;
        yyjson_arr_foreach(toolset_val, idx, max_idx, tool_val) {     // LCOV_EXCL_BR_LINE
            const char *tool_str = yyjson_get_str(tool_val);
            if (tool_str != NULL) {     // LCOV_EXCL_BR_LINE
                agent->toolset_filter[agent->toolset_count] = talloc_strdup(agent, tool_str);
                if (agent->toolset_filter[agent->toolset_count] == NULL) return;  // LCOV_EXCL_LINE
                agent->toolset_count++;
            }
        }
    }
}

// Helper: Replay model command
static void replay_model_command(ik_agent_ctx_t *agent, const char *args, ik_logger_t *logger)
{
    char *model_copy = talloc_strdup(agent, args);
    if (model_copy == NULL) return;  // LCOV_EXCL_LINE

    char *slash = strchr(model_copy, '/');
    if (slash != NULL) {
        *slash = '\0';
    }

    const char *provider = ik_infer_provider(model_copy);

    if (agent->provider != NULL) {
        talloc_free(agent->provider);
    }
    if (agent->model != NULL) {
        talloc_free(agent->model);
    }

    agent->provider = talloc_strdup(agent, provider ? provider : "openai");  // LCOV_EXCL_BR_LINE
    agent->model = talloc_strdup(agent, model_copy);  // LCOV_EXCL_BR_LINE

    if (agent->provider_instance != NULL) {
        talloc_free(agent->provider_instance);
        agent->provider_instance = NULL;
    }

    yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_BR_LINE
    if (log_doc != NULL) {  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "replay_model_command");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "provider", agent->provider);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "model", agent->model);  // LCOV_EXCL_LINE
        ik_logger_info_json(logger, log_doc);  // LCOV_EXCL_LINE
    }  // LCOV_EXCL_LINE

    talloc_free(model_copy);
}

// Helper: Replay pin command
static void replay_pin_command(ik_agent_ctx_t *agent, const char *args)
{
    for (size_t i = 0; i < agent->pinned_count; i++) {
        if (strcmp(agent->pinned_paths[i], args) == 0) {     // LCOV_EXCL_BR_LINE
            return;     // LCOV_EXCL_LINE
        }
    }

    char **new_paths = talloc_realloc(agent, agent->pinned_paths,
                                       char *, (unsigned int)(agent->pinned_count + 1));
    if (new_paths == NULL) return;  // LCOV_EXCL_LINE
    agent->pinned_paths = new_paths;

    agent->pinned_paths[agent->pinned_count] = talloc_strdup(agent, args);
    if (agent->pinned_paths[agent->pinned_count] == NULL) return;  // LCOV_EXCL_LINE
    agent->pinned_count++;
}

// Helper: Replay unpin command
static void replay_unpin_command(ik_agent_ctx_t *agent, const char *args)
{
    int64_t found_index = -1;
    for (size_t i = 0; i < agent->pinned_count; i++) {
        if (strcmp(agent->pinned_paths[i], args) == 0) {
            found_index = (int64_t)i;
            break;
        }
    }

    if (found_index < 0) {     // LCOV_EXCL_BR_LINE
        return;     // LCOV_EXCL_LINE
    }

    talloc_free(agent->pinned_paths[found_index]);
    for (size_t i = (size_t)found_index; i < agent->pinned_count - 1; i++) {
        agent->pinned_paths[i] = agent->pinned_paths[i + 1];
    }
    agent->pinned_count--;

    if (agent->pinned_count == 0) {     // LCOV_EXCL_BR_LINE
        talloc_free(agent->pinned_paths);     // LCOV_EXCL_LINE
        agent->pinned_paths = NULL;     // LCOV_EXCL_LINE
    } else {
        char **new_paths = talloc_realloc(agent, agent->pinned_paths,
                                           char *, (unsigned int)agent->pinned_count);
        if (new_paths != NULL) {  // LCOV_EXCL_BR_LINE
            agent->pinned_paths = new_paths;
        }
    }
}

// Replay command side effects
// Some commands (like /model) have side effects that need to be re-applied
// when replaying history to restore agent state.
void ik_agent_restore_replay_command_effects(ik_agent_ctx_t *agent, ik_msg_t *msg, ik_logger_t *logger)
{
    assert(agent != NULL);      // LCOV_EXCL_BR_LINE
    assert(msg != NULL);        // LCOV_EXCL_BR_LINE

    if (msg->data_json == NULL) {
        return;
    }

    // Parse data_json
    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    if (doc == NULL) {
        return;
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (root == NULL) {
        yyjson_doc_free(doc);
        return;
    }

    // Check if this is a fork event
    if (msg->kind != NULL && strcmp(msg->kind, "fork") == 0) {
        replay_fork_event(agent, root);
        yyjson_doc_free(doc);
        return;
    }

    // Handle command events
    yyjson_val *cmd_val = yyjson_obj_get_(root, "command");
    yyjson_val *args_val = yyjson_obj_get_(root, "args");

    const char *cmd_name = yyjson_get_str(cmd_val);
    const char *args = yyjson_get_str(args_val);  // May be NULL

    if (cmd_name == NULL) {
        yyjson_doc_free(doc);
        return;
    }

    // Handle /model command
    if (strcmp(cmd_name, "model") == 0 && args != NULL) {
        replay_model_command(agent, args, logger);
    }

    yyjson_doc_free(doc);
}

// Replay all pin/unpin commands for an agent (independent of clear boundaries)
res_t ik_agent_replay_pins(ik_db_ctx_t *db, ik_agent_ctx_t *agent)
{
    assert(db != NULL);     // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    // 1. Query the fork event for this agent to get initial pinned_paths snapshot
    const char *fork_query =
        "SELECT data FROM messages WHERE agent_uuid = $1 AND kind = 'fork' ORDER BY id LIMIT 1";
    const char *fork_params[1] = {agent->uuid};

    ik_pg_result_wrapper_t *fork_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, fork_query, 1, NULL,
                                                  fork_params, NULL, NULL, 0));
    PGresult *fork_res = fork_wrapper->pg_result;

    if (PQresultStatus(fork_res) != PGRES_TUPLES_OK) {     // LCOV_EXCL_BR_LINE
        const char *pq_err = PQerrorMessage(db->conn);     // LCOV_EXCL_LINE
        talloc_free(tmp);     // LCOV_EXCL_LINE
        return ERR(db, IO, "Failed to query fork event: %s", pq_err);     // LCOV_EXCL_LINE
    }

    // 2. Extract pinned_paths from fork event data_json (if exists)
    int fork_rows = PQntuples(fork_res);
    if (fork_rows > 0) {     // LCOV_EXCL_BR_LINE - Fork event path tested in integration
        const char *fork_json = PQgetvalue_(fork_res, 0, 0);     // LCOV_EXCL_LINE
        if (fork_json != NULL) {     // LCOV_EXCL_BR_LINE LCOV_EXCL_LINE
            yyjson_doc *fork_doc = yyjson_read(fork_json, strlen(fork_json), 0);     // LCOV_EXCL_LINE
            if (fork_doc != NULL) {     // LCOV_EXCL_BR_LINE LCOV_EXCL_LINE
                yyjson_val *fork_root = yyjson_doc_get_root_(fork_doc);     // LCOV_EXCL_LINE
                if (fork_root != NULL) {     // LCOV_EXCL_BR_LINE LCOV_EXCL_LINE
                    replay_fork_event(agent, fork_root);     // LCOV_EXCL_LINE
                }
                yyjson_doc_free(fork_doc);     // LCOV_EXCL_LINE
            }
        }
    }

    // 3. Query ALL command events with pin or unpin (no clear boundary)
    const char *cmd_query =
        "SELECT data FROM messages "
        "WHERE agent_uuid = $1 AND kind = 'command' "
        "AND (data->>'command' = 'pin' OR data->>'command' = 'unpin') "
        "ORDER BY id";
    const char *cmd_params[1] = {agent->uuid};

    ik_pg_result_wrapper_t *cmd_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, cmd_query, 1, NULL,
                                                  cmd_params, NULL, NULL, 0));
    PGresult *cmd_res = cmd_wrapper->pg_result;

    if (PQresultStatus(cmd_res) != PGRES_TUPLES_OK) {     // LCOV_EXCL_BR_LINE
        const char *pq_err = PQerrorMessage(db->conn);     // LCOV_EXCL_LINE
        talloc_free(tmp);     // LCOV_EXCL_LINE
        return ERR(db, IO, "Failed to query pin/unpin commands: %s", pq_err);     // LCOV_EXCL_LINE
    }

    // 4. Apply pin/unpin commands chronologically
    int cmd_rows = PQntuples(cmd_res);
    for (int i = 0; i < cmd_rows; i++) {     // LCOV_EXCL_BR_LINE
        const char *data_json = PQgetvalue_(cmd_res, i, 0);
        if (data_json == NULL) continue;     // LCOV_EXCL_BR_LINE LCOV_EXCL_LINE

        yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
        if (doc == NULL) continue;     // LCOV_EXCL_BR_LINE LCOV_EXCL_LINE

        yyjson_val *root = yyjson_doc_get_root_(doc);
        if (root == NULL) {     // LCOV_EXCL_BR_LINE
            yyjson_doc_free(doc);     // LCOV_EXCL_LINE
            continue;     // LCOV_EXCL_LINE
        }

        yyjson_val *cmd_val = yyjson_obj_get_(root, "command");
        yyjson_val *args_val = yyjson_obj_get_(root, "args");

        const char *cmd_name = yyjson_get_str(cmd_val);     // LCOV_EXCL_BR_LINE
        const char *args = yyjson_get_str(args_val);

        if (cmd_name != NULL && args != NULL) {     // LCOV_EXCL_BR_LINE
            if (strcmp(cmd_name, "pin") == 0) {
                replay_pin_command(agent, args);
            } else if (strcmp(cmd_name, "unpin") == 0) {     // LCOV_EXCL_BR_LINE
                replay_unpin_command(agent, args);
            }
        }

        yyjson_doc_free(doc);
    }

    talloc_free(tmp);
    return OK(NULL);
}
