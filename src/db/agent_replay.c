#include "agent_replay.h"

#include "agent.h"
#include "pg_result.h"

#include "../error.h"
#include "../panic.h"
#include "../tmp_ctx.h"
#include "../wrapper.h"

#include <assert.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

res_t ik_agent_find_clear(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                          const char *agent_uuid, int64_t max_id,
                          int64_t *clear_id_out)
{
    assert(db_ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(agent_uuid != NULL);   // LCOV_EXCL_BR_LINE
    assert(clear_id_out != NULL); // LCOV_EXCL_BR_LINE

    // Create temporary context for query result
    TALLOC_CTX *tmp = tmp_ctx_create();

    // Query for most recent clear event, optionally limited by max_id
    // max_id = 0 means no limit
    const char *query;
    const char *param_values[2];
    int num_params;

    if (max_id > 0) {
        query = "SELECT MAX(id) FROM messages "
                "WHERE agent_uuid = $1 AND kind = 'clear' AND id <= $2";
        char max_id_str[32];
        snprintf(max_id_str, sizeof(max_id_str), "%" PRId64, max_id);
        param_values[0] = agent_uuid;
        param_values[1] = talloc_strdup(tmp, max_id_str);
        if (param_values[1] == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        num_params = 2;
    } else {
        query = "SELECT MAX(id) FROM messages "
                "WHERE agent_uuid = $1 AND kind = 'clear'";
        param_values[0] = agent_uuid;
        num_params = 1;
    }

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, num_params, NULL,
                                                  param_values, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    // Check query execution status
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(ctx, IO, "Failed to find clear: %s", pq_err);
    }

    // Parse result - MAX() returns NULL if no rows match
    if (PQntuples(res) == 0 || PQgetisnull(res, 0, 0)) { // LCOV_EXCL_BR_LINE
        *clear_id_out = 0;  // No clear found
    } else {
        const char *id_str = PQgetvalue_(res, 0, 0);
        if (sscanf(id_str, "%lld", (long long *)clear_id_out) != 1) {
            talloc_free(tmp);
            return ERR(ctx, PARSE, "Failed to parse clear ID");
        }
    }

    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_agent_build_replay_ranges(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                                   const char *agent_uuid,
                                   ik_replay_range_t **ranges_out,
                                   size_t *count_out)
{
    assert(db_ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);         // LCOV_EXCL_BR_LINE
    assert(agent_uuid != NULL);  // LCOV_EXCL_BR_LINE
    assert(ranges_out != NULL);  // LCOV_EXCL_BR_LINE
    assert(count_out != NULL);   // LCOV_EXCL_BR_LINE

    // Create temporary context for intermediate allocations
    TALLOC_CTX *tmp = tmp_ctx_create();

    // Dynamic array for ranges (will be reversed at the end)
    size_t capacity = 8;
    size_t count = 0;
    ik_replay_range_t *ranges = talloc_array(tmp, ik_replay_range_t, (unsigned int)capacity);
    if (ranges == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Walk backwards from leaf agent
    const char *current_uuid = agent_uuid;
    int64_t end_id = 0;  // Leaf has no upper bound

    while (current_uuid != NULL) { // LCOV_EXCL_BR_LINE
        // Find most recent clear for this agent (within range)
        int64_t clear_id = 0;
        res_t res = ik_agent_find_clear(db_ctx, ctx, current_uuid, end_id, &clear_id);
        if (is_err(&res)) { // LCOV_EXCL_BR_LINE
            talloc_free(tmp); // LCOV_EXCL_LINE
            return res; // LCOV_EXCL_LINE
        }

        // Ensure capacity
        if (count >= capacity) {
            capacity *= 2;
            ik_replay_range_t *new_ranges = talloc_realloc(tmp, ranges, ik_replay_range_t,
                                                           (unsigned int)capacity);
            if (new_ranges == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            ranges = new_ranges;
        }

        // Add range entry
        ranges[count].agent_uuid = talloc_strdup(tmp, current_uuid);
        if (ranges[count].agent_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        if (clear_id > 0) {
            // Clear found - start after clear, this terminates the walk
            ranges[count].start_id = clear_id;
            ranges[count].end_id = end_id;
            count++;
            break;
        } else {
            // No clear - include all messages from beginning
            ranges[count].start_id = 0;
            ranges[count].end_id = end_id;
            count++;

            // Get agent record to find parent
            ik_db_agent_row_t *agent_row = NULL;
            res = ik_db_agent_get(db_ctx, tmp, current_uuid, &agent_row);
            if (is_err(&res)) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp); // LCOV_EXCL_LINE
                return res; // LCOV_EXCL_LINE
            }

            if (agent_row->parent_uuid == NULL) {
                // Root agent reached
                break;
            }

            // Move to parent - parent's range ends at this agent's fork point
            int64_t fork_id = 0;
            if (sscanf(agent_row->fork_message_id, "%lld", (long long *)&fork_id) != 1) { // LCOV_EXCL_BR_LINE
                talloc_free(tmp); // LCOV_EXCL_LINE
                return ERR(ctx, PARSE, "Failed to parse fork_message_id"); // LCOV_EXCL_LINE
            }
            end_id = fork_id;
            current_uuid = agent_row->parent_uuid;
        }
    }

    // Reverse the array to get chronological order (root first)
    ik_replay_range_t *result = talloc_array(ctx, ik_replay_range_t, (unsigned int)count);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < count; i++) {
        size_t src_idx = count - 1 - i;
        result[i].agent_uuid = talloc_strdup(result, ranges[src_idx].agent_uuid);
        if (result[i].agent_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        result[i].start_id = ranges[src_idx].start_id;
        result[i].end_id = ranges[src_idx].end_id;
    }

    *ranges_out = result;
    *count_out = count;
    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_agent_query_range(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                           const ik_replay_range_t *range,
                           ik_msg_t ***messages_out,
                           size_t *count_out)
{
    assert(db_ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(range != NULL);        // LCOV_EXCL_BR_LINE
    assert(messages_out != NULL); // LCOV_EXCL_BR_LINE
    assert(count_out != NULL);    // LCOV_EXCL_BR_LINE

    // Create temporary context for query
    TALLOC_CTX *tmp = tmp_ctx_create();

    // Build parameterized query
    // WHERE agent_uuid=$1 AND id>$2 AND ($3=0 OR id<=$3) ORDER BY created_at
    const char *query =
        "SELECT id, kind, content, data FROM messages "
        "WHERE agent_uuid = $1 AND id > $2 AND ($3 = 0 OR id <= $3) "
        "ORDER BY created_at";

    char start_id_str[32];
    char end_id_str[32];
    snprintf(start_id_str, sizeof(start_id_str), "%" PRId64, range->start_id);
    snprintf(end_id_str, sizeof(end_id_str), "%" PRId64, range->end_id);

    const char *param_values[3];
    param_values[0] = range->agent_uuid;
    param_values[1] = start_id_str;
    param_values[2] = end_id_str;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 3, NULL,
                                                  param_values, NULL, NULL, 0));
    PGresult *pg_res = res_wrapper->pg_result;

    // Check query execution status
    if (PQresultStatus(pg_res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(ctx, IO, "Failed to query range: %s", pq_err);
    }

    // Process results
    int num_rows = PQntuples(pg_res);
    if (num_rows == 0) {
        *messages_out = NULL;
        *count_out = 0;
        talloc_free(tmp);
        return OK(NULL);
    }

    // Allocate message array
    ik_msg_t **messages = talloc_array(ctx, ik_msg_t *, (unsigned int)num_rows);
    if (messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (int i = 0; i < num_rows; i++) {
        ik_msg_t *msg = talloc_zero(messages, ik_msg_t);
        if (msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        // Parse ID
        const char *id_str = PQgetvalue_(pg_res, i, 0);
        if (sscanf(id_str, "%lld", (long long *)&msg->id) != 1) {
            talloc_free(tmp);
            return ERR(ctx, PARSE, "Failed to parse message ID");
        }

        // Kind
        msg->kind = talloc_strdup(msg, PQgetvalue_(pg_res, i, 1));
        if (msg->kind == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        // Content (may be NULL)
        if (!PQgetisnull(pg_res, i, 2)) {
            msg->content = talloc_strdup(msg, PQgetvalue_(pg_res, i, 2));
            if (msg->content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        } else {
            msg->content = NULL;
        }

        // Data JSON (may be NULL)
        if (!PQgetisnull(pg_res, i, 3)) {
            msg->data_json = talloc_strdup(msg, PQgetvalue_(pg_res, i, 3));
            if (msg->data_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        } else {
            msg->data_json = NULL;
        }

        messages[i] = msg;
    }

    *messages_out = messages;
    *count_out = (size_t)num_rows;
    talloc_free(tmp);
    return OK(NULL);
}

void ik_agent_replay_append_messages(ik_replay_context_t *replay_ctx,
                                     ik_msg_t **src_msgs,
                                     size_t count)
{
    assert(replay_ctx != NULL);  // LCOV_EXCL_BR_LINE

    if (count == 0) return;
    assert(src_msgs != NULL);    // LCOV_EXCL_BR_LINE

    for (size_t j = 0; j < count; j++) {
        // Ensure capacity
        if (replay_ctx->count >= replay_ctx->capacity) {
            size_t new_capacity = replay_ctx->capacity == 0 ? 16 : replay_ctx->capacity * 2;
            ik_msg_t **new_messages = talloc_realloc(replay_ctx, replay_ctx->messages, ik_msg_t *,
                                                     (unsigned int)new_capacity);
            if (new_messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            replay_ctx->messages = new_messages;
            replay_ctx->capacity = new_capacity;
        }

        // Copy message to context
        ik_msg_t *src_msg = src_msgs[j];
        ik_msg_t *msg = talloc_zero(replay_ctx, ik_msg_t);
        if (msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        msg->id = src_msg->id;

        msg->kind = talloc_strdup(msg, src_msg->kind);
        if (msg->kind == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        if (src_msg->content != NULL) {
            msg->content = talloc_strdup(msg, src_msg->content);
            if (msg->content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        } else {
            msg->content = NULL;
        }

        if (src_msg->data_json != NULL) {
            msg->data_json = talloc_strdup(msg, src_msg->data_json);
            if (msg->data_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        } else {
            msg->data_json = NULL;
        }

        replay_ctx->messages[replay_ctx->count] = msg;
        replay_ctx->count++;
    }
}

void ik_agent_replay_filter_interrupted(ik_replay_context_t *replay_ctx)
{
    assert(replay_ctx != NULL);  // LCOV_EXCL_BR_LINE

    size_t last_user_idx = 0;

    // First pass: identify interrupted turns and mark messages for removal
    for (size_t i = 0; i < replay_ctx->count; i++) {
        ik_msg_t *msg = replay_ctx->messages[i];
        if (msg == NULL) continue; // LCOV_EXCL_BR_LINE

        bool is_interrupted = (strcmp(msg->kind, "interrupted") == 0);
        bool is_user = (strcmp(msg->kind, "user") == 0);

        if (is_interrupted) {
            // Mark all messages from last_user_idx to i (inclusive) for removal
            for (size_t j = last_user_idx; j <= i; j++) {
                ik_msg_t *to_free = replay_ctx->messages[j];
                if (to_free == NULL) continue; // LCOV_EXCL_BR_LINE
                talloc_free(to_free);
                replay_ctx->messages[j] = NULL;
            }
            continue;
        }

        if (is_user) {
            last_user_idx = i;
        }
    }

    // Second pass: compact the array by removing NULL entries
    size_t write_idx = 0;
    for (size_t i = 0; i < replay_ctx->count; i++) {
        ik_msg_t *msg = replay_ctx->messages[i];
        if (msg == NULL) continue; // LCOV_EXCL_BR_LINE
        replay_ctx->messages[write_idx] = msg;
        write_idx++;
    }
    replay_ctx->count = write_idx;
}

res_t ik_agent_replay_history(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                              const char *agent_uuid,
                              ik_replay_context_t **ctx_out)
{
    assert(db_ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(agent_uuid != NULL); // LCOV_EXCL_BR_LINE
    assert(ctx_out != NULL);    // LCOV_EXCL_BR_LINE

    // Create temporary context for intermediate allocations
    TALLOC_CTX *tmp = tmp_ctx_create();

    // Build replay ranges
    ik_replay_range_t *ranges = NULL;
    size_t range_count = 0;
    res_t res = ik_agent_build_replay_ranges(db_ctx, tmp, agent_uuid, &ranges, &range_count);
    if (is_err(&res)) { // LCOV_EXCL_BR_LINE
        talloc_steal(ctx, res.err); // LCOV_EXCL_LINE
        talloc_free(tmp); // LCOV_EXCL_LINE
        return res; // LCOV_EXCL_LINE
    }

    // Create replay context
    ik_replay_context_t *replay_ctx = talloc_zero(ctx, ik_replay_context_t);
    if (replay_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Initialize context
    replay_ctx->messages = NULL;
    replay_ctx->count = 0;
    replay_ctx->capacity = 0;
    replay_ctx->mark_stack.marks = NULL;
    replay_ctx->mark_stack.count = 0;
    replay_ctx->mark_stack.capacity = 0;

    // Process each range in chronological order
    for (size_t i = 0; i < range_count; i++) {
        ik_msg_t **range_msgs = NULL;
        size_t range_msg_count = 0;

        res = ik_agent_query_range(db_ctx, tmp, &ranges[i], &range_msgs, &range_msg_count);
        if (is_err(&res)) { // LCOV_EXCL_BR_LINE
            talloc_steal(ctx, res.err); // LCOV_EXCL_LINE
            talloc_free(tmp); // LCOV_EXCL_LINE
            return res; // LCOV_EXCL_LINE
        }

        ik_agent_replay_append_messages(replay_ctx, range_msgs, range_msg_count);
    }

    // Filter out interrupted turns
    ik_agent_replay_filter_interrupted(replay_ctx);

    *ctx_out = replay_ctx;
    talloc_free(tmp);
    return OK(NULL);
}
