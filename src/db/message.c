#include "message.h"

#include "pg_result.h"

#include "../error.h"
#include "../wrapper.h"

#include <assert.h>
#include <libpq-fe.h>
#include <string.h>

// Valid event kinds
static const char *VALID_KINDS[] = {
    "clear",
    "system",
    "user",
    "assistant",
    "mark",
    "rewind",
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
                            const char *kind,
                            const char *content,
                            const char *data_json) {
    // Preconditions
    assert(db != NULL);                         // LCOV_EXCL_BR_LINE
    assert(db->conn != NULL);                   // LCOV_EXCL_BR_LINE
    assert(session_id > 0);                     // LCOV_EXCL_BR_LINE
    assert(ik_db_message_is_valid_kind(kind));  // LCOV_EXCL_BR_LINE

    // Create temporary context for query parameters
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Build parameterized query
    const char *query =
        "INSERT INTO messages (session_id, kind, content, data) "
        "VALUES ($1, $2, $3, $4)";

    // Prepare parameters
    char *session_id_str = talloc_asprintf(tmp, "%lld", (long long)session_id);
    if (session_id_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    const char *params[4];
    params[0] = session_id_str;
    params[1] = kind;
    params[2] = content;      // Can be NULL
    params[3] = data_json;    // Can be NULL

    // Execute query and wrap result for automatic cleanup
    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 4, NULL, params, NULL, NULL, 0));
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
