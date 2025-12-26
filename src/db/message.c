#include "message.h"
#include "replay.h"

#include "pg_result.h"

#include "../error.h"
#include "../tmp_ctx.h"
#include "../wrapper.h"

#include <assert.h>
#include <libpq-fe.h>
#include <stdbool.h>
#include <string.h>
#include <talloc.h>
#include "vendor/yyjson/yyjson.h"

// Valid event kinds
static const char *VALID_KINDS[] = {
    "clear",
    "system",
    "user",
    "assistant",
    "tool_call",
    "tool_result",
    "mark",
    "rewind",
    "agent_killed",
    "command",
    "fork",
    "usage",
    NULL
};

// Validate event kind
bool ik_db_message_is_valid_kind(const char *kind) {
    if (kind == NULL) {
        return false;
    }

    for (size_t i = 0; VALID_KINDS[i] != NULL; i++) {
        if (strcmp(kind, VALID_KINDS[i]) == 0) {
            return true;
        }
    }

    return false;
}

res_t ik_db_message_insert(ik_db_ctx_t *db,
                            int64_t session_id,
                            const char *agent_uuid,
                            const char *kind,
                            const char *content,
                            const char *data_json) {
    // Preconditions
    assert(db != NULL);                         // LCOV_EXCL_BR_LINE
    assert(db->conn != NULL);                   // LCOV_EXCL_BR_LINE
    assert(session_id > 0);                     // LCOV_EXCL_BR_LINE
    assert(ik_db_message_is_valid_kind(kind));  // LCOV_EXCL_BR_LINE

    // Create temporary context for query parameters
    TALLOC_CTX *tmp = tmp_ctx_create();

    // Build parameterized query
    const char *query =
        "INSERT INTO messages (session_id, agent_uuid, kind, content, data) "
        "VALUES ($1, $2, $3, $4, $5)";

    // Prepare parameters
    char *session_id_str = talloc_asprintf(tmp, "%lld", (long long)session_id);
    if (session_id_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    const char *params[5];
    params[0] = session_id_str;
    params[1] = agent_uuid;   // Can be NULL
    params[2] = kind;
    params[3] = content;      // Can be NULL
    params[4] = data_json;    // Can be NULL

    // Execute query and wrap result for automatic cleanup
    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 5, NULL, params, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    // Check result
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        // Bug 9 fix: Allocate error on db context (outlives function call)
        // not tmp context (freed below, leaving dangling pointer)
        const char *pq_err = PQerrorMessage(db->conn);
        res_t error_res = ERR(db, IO, "Message insert failed: %s", pq_err);

        talloc_free(tmp);  // Destructor automatically calls PQclear

        return error_res;
    }

    talloc_free(tmp);  // Destructor automatically calls PQclear

    return OK(NULL);
}

ik_msg_t *ik_msg_create_tool_result(void *parent,
                                    const char *tool_call_id,
                                    const char *name,
                                    const char *output,
                                    bool success,
                                    const char *content) {
    assert(tool_call_id != NULL);  // LCOV_EXCL_BR_LINE
    assert(name != NULL);           // LCOV_EXCL_BR_LINE
    assert(output != NULL);         // LCOV_EXCL_BR_LINE
    assert(content != NULL);        // LCOV_EXCL_BR_LINE

    // Allocate message struct
    ik_msg_t *msg = talloc_zero(parent, ik_msg_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Initialize ID to 0 for in-memory message
    msg->id = 0;

    // Set kind to "tool_result"
    msg->kind = talloc_strdup(msg, "tool_result");
    if (!msg->kind) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Set content to human-readable summary
    msg->content = talloc_strdup(msg, content);
    if (!msg->content) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Build data_json with yyjson
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (!root) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);

    // Add tool_call_id
    if (!yyjson_mut_obj_add_str(doc, root, "tool_call_id", tool_call_id)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    // Add name
    if (!yyjson_mut_obj_add_str(doc, root, "name", name)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    // Add output
    if (!yyjson_mut_obj_add_str(doc, root, "output", output)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    // Add success
    if (!yyjson_mut_obj_add_bool(doc, root, "success", success)) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    // Convert to JSON string
    size_t len = 0;
    char *json_str = yyjson_mut_write(doc, 0, &len);
    if (!json_str) {  // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc);  // LCOV_EXCL_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    // Copy JSON string to message (child of msg)
    msg->data_json = talloc_strdup(msg, json_str);
    if (!msg->data_json) {  // LCOV_EXCL_BR_LINE
        free(json_str);  // LCOV_EXCL_LINE
        yyjson_mut_doc_free(doc);  // LCOV_EXCL_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    // Free the JSON string allocated by yyjson_mut_write
    free(json_str);

    // Clean up yyjson document
    yyjson_mut_doc_free(doc);

    return msg;
}
