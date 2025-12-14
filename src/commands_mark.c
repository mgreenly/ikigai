/**
 * @file commands_mark.c
 * @brief Mark and rewind command implementations
 */

#include "commands_mark.h"

#include "db/message.h"
#include "db/pg_result.h"
#include "marks.h"
#include "panic.h"
#include "repl.h"
#include "agent.h"
#include "scrollback.h"
#include "shared.h"
#include "wrapper.h"

#include <assert.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

// Helper to query database for mark's message ID
static int64_t get_mark_db_id(TALLOC_CTX *ctx, ik_repl_ctx_t *repl, const char *label)
{
    if (repl->shared->db_ctx == NULL || repl->shared->session_id <= 0) {
        return 0;
    }

    const char *query;
    const char *params[2];
    int32_t param_count;
    char *session_id_str = talloc_asprintf(ctx, "%lld", (long long)repl->shared->session_id);
    if (session_id_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (label != NULL) {
        query =
            "SELECT id FROM messages WHERE session_id = $1 AND kind = 'mark' AND data->>'label' = $2 ORDER BY created_at DESC LIMIT 1";
        params[0] = session_id_str;
        params[1] = label;
        param_count = 2;
    } else {
        query = "SELECT id FROM messages WHERE session_id = $1 AND kind = 'mark' ORDER BY created_at DESC LIMIT 1";
        params[0] = session_id_str;
        param_count = 1;
    }

    int64_t mark_id = 0;
    ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, pq_exec_params_(repl->shared->db_ctx->conn,
                                                                                       query,
                                                                                       param_count,
                                                                                       NULL,
                                                                                       params,
                                                                                       NULL,
                                                                                       NULL,
                                                                                       0));
    PGresult *result = result_wrapper->pg_result;
    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
        const char *id_str = PQgetvalue(result, 0, 0);
        if (sscanf(id_str, "%lld", (long long *)&mark_id) != 1) {
            mark_id = 0;
        }
    }
    talloc_free(session_id_str);
    return mark_id;
}

res_t ik_cmd_mark(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)ctx;

    // Parse optional label from args
    // Note: dispatcher ensures args is either NULL or points to non-empty string
    const char *label = args;

    // Create the mark
    res_t result = ik_mark_create(repl, label);
    if (is_err(&result)) {
        return result;
    }

    // Persist mark event to database (Integration Point 4)
    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {
        // Build data JSON with label (may be NULL for auto-numbered marks)
        char *data_json = NULL;
        if (label != NULL) {
            data_json = talloc_asprintf(repl, "{\"label\":\"%s\"}", label);
        } else {
            data_json = talloc_strdup(repl, "{}");
        }

        res_t db_res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                                            NULL, "mark", NULL, data_json);
        if (is_err(&db_res)) {
            // Log error but don't crash - memory state is authoritative
            if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
                fprintf(repl->shared->db_debug_pipe->write_end,
                        "Warning: Failed to persist mark event to database: %s\n",
                        error_message(db_res.err));
            }
            talloc_free(db_res.err);
        }
        talloc_free(data_json);
    }

    return OK(NULL);
}

res_t ik_cmd_rewind(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE

    // Parse optional label from args
    const char *label = args;

    // Find the target mark before rewinding (to get message_index for database)
    ik_mark_t *target_mark = NULL;
    res_t find_result = ik_mark_find(repl, label, &target_mark);
    if (is_err(&find_result)) {
        // Show error message in scrollback
        char *err_msg = talloc_asprintf(ctx, "Error: %s", find_result.err->msg);
        if (err_msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        talloc_free(err_msg);
        talloc_free(find_result.err);
        return OK(NULL);  // Don't propagate error, just show it
    }

    // Save mark info before rewind (rewind will remove the mark)
    char *target_label = target_mark->label ? talloc_strdup(ctx, target_mark->label) : NULL;

    // Query database for the mark's message ID (needed for rewind event)
    int64_t target_message_id = get_mark_db_id(ctx, repl, target_label);

    // Rewind to the mark (use the mark we already found to avoid redundant lookup)
    res_t result = ik_mark_rewind_to_mark(repl, target_mark);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE - scrollback operations never fail, only panic
        if (target_label != NULL) talloc_free(target_label);  // LCOV_EXCL_LINE
        return result;  // LCOV_EXCL_LINE
    }

    // Persist rewind event to database (Integration Point 5)
    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0 && target_message_id > 0) {
        // Build data JSON with target message ID and label
        char *data_json = talloc_asprintf(repl,
                                          "{\"target_message_id\":%lld,\"target_label\":%s}",
                                          (long long)target_message_id,
                                          target_label ? talloc_asprintf(repl, "\"%s\"", target_label) : "null");

        res_t db_res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                                            NULL, "rewind", NULL, data_json);
        if (is_err(&db_res)) {
            // Log error but don't crash - memory state is authoritative
            if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
                fprintf(repl->shared->db_debug_pipe->write_end,
                        "Warning: Failed to persist rewind event to database: %s\n",
                        error_message(db_res.err));
            }
            talloc_free(db_res.err);
        }
        talloc_free(data_json);
    }

    if (target_label != NULL) talloc_free(target_label);
    return OK(NULL);
}
