#include "replay.h"

#include "../error.h"
#include "../logger.h"
#include "../tmp_ctx.h"
#include "../vendor/yyjson/yyjson.h"
#include "../wrapper.h"
#include "pg_result.h"

#include <assert.h>
#include <libpq-fe.h>
#include <string.h>

// Initial capacity for context array (as specified in design)
#define INITIAL_CAPACITY 16

// Initial capacity for mark stack (as specified in design)
#define MARK_STACK_INITIAL_CAPACITY 4

// Helper: ensure context array has capacity for at least one more element
// Uses geometric growth (capacity *= 2) as specified in design
static void ensure_capacity(ik_replay_context_t *context)
{
    assert(context != NULL); // LCOV_EXCL_BR_LINE

    // If count < capacity, we have room
    if (context->count < context->capacity) {
        return;
    }

    // Need to grow - use geometric growth
    size_t new_capacity;
    if (context->capacity == 0) {
        new_capacity = INITIAL_CAPACITY;
    } else {
        new_capacity = context->capacity * 2;
    }

    // Reallocate messages array
    // talloc_realloc expects unsigned int, cast is safe for reasonable message
    // counts
    ik_msg_t **new_messages = talloc_realloc(
        context, context->messages, ik_msg_t *, (unsigned int)new_capacity);
    if (new_messages == NULL) // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE

    context->messages = new_messages;
    context->capacity = new_capacity;
}

// Helper: ensure mark stack has capacity for at least one more element
static void ensure_mark_stack_capacity(ik_replay_context_t *context)
{
    assert(context != NULL); // LCOV_EXCL_BR_LINE

    // If count < capacity, we have room
    if (context->mark_stack.count < context->mark_stack.capacity) {
        return;
    }

    // Need to grow - use geometric growth
    size_t new_capacity;
    if (context->mark_stack.capacity == 0) {
        new_capacity = MARK_STACK_INITIAL_CAPACITY;
    } else {
        new_capacity = context->mark_stack.capacity * 2;
    }

    // Reallocate marks array
    ik_replay_mark_t *new_marks =
        talloc_realloc(context, context->mark_stack.marks, ik_replay_mark_t,
                       (unsigned int)new_capacity);
    if (new_marks == NULL)  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE

    context->mark_stack.marks = new_marks;
    context->mark_stack.capacity = new_capacity;
}

// Helper: find mark in mark stack by message ID
static ik_replay_mark_t *find_mark(ik_replay_context_t *context,
                                   int64_t message_id)
{
    assert(context != NULL); // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < context->mark_stack.count; i++) {
        if (context->mark_stack.marks[i].message_id == message_id) {
            return &context->mark_stack.marks[i];
        }
    }
    return NULL;
}

// Helper: append message to context array
static res_t append_message(ik_replay_context_t *context, int64_t id,
                            const char *kind, const char *content,
                            const char *data_json)
{
    assert(context != NULL); // LCOV_EXCL_BR_LINE
    assert(kind != NULL);  // LCOV_EXCL_BR_LINE

    // Ensure capacity (never fails - PANICs on OOM)
    ensure_capacity(context);

    // Allocate message structure as child of context
    ik_msg_t *msg = talloc_zero(context, ik_msg_t);
    if (msg == NULL)        // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE

    // Copy message data
    msg->id = id;

    msg->kind = talloc_strdup(msg, kind);
    if (msg->kind == NULL)  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE

    if (content != NULL) {
        msg->content = talloc_strdup(msg, content);
        if (msg->content == NULL) // LCOV_EXCL_BR_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
    } else {
        msg->content = NULL;
    }

    if (data_json != NULL) {
        msg->data_json = talloc_strdup(msg, data_json);
        if (msg->data_json == NULL) // LCOV_EXCL_BR_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
    } else {
        msg->data_json = NULL;
    }

    // Add to array
    context->messages[context->count] = msg;
    context->count++;

    return OK(NULL);
}

// Helper: extract label from mark event data_json
static char *extract_mark_label(ik_replay_context_t *context, const char *data_json)
{
    if (data_json == NULL) {
        return NULL;
    }

    yyjson_doc *doc = yyjson_read_(data_json, strlen(data_json), 0);
    if (doc == NULL) {
        return NULL;
    }

    char *label = NULL;
    yyjson_val *root = yyjson_doc_get_root_(doc);
    yyjson_val *label_val = yyjson_obj_get_(root, "label");
    if (label_val != NULL && yyjson_is_str(label_val)) {
        const char *label_str = yyjson_get_str_(label_val);
        if (label_str != NULL) {
            label = talloc_strdup(context, label_str);
            if (label == NULL) // LCOV_EXCL_BR_LINE
                PANIC("Out of memory"); // LCOV_EXCL_LINE
        }
    }
    yyjson_doc_free(doc);
    return label;
}

// Helper: process mark event
static res_t process_mark_event(ik_replay_context_t *context, int64_t id,
                                const char *kind, const char *content,
                                const char *data_json)
{
    res_t res = append_message(context, id, kind, content, data_json);
    if (is_err(&res)) { // LCOV_EXCL_BR_LINE - never returns error (PANICs)
        return res; // LCOV_EXCL_LINE
    }

    char *label = extract_mark_label(context, data_json);

    ensure_mark_stack_capacity(context);
    size_t idx = context->mark_stack.count;
    context->mark_stack.marks[idx].message_id = id;
    context->mark_stack.marks[idx].label = label;
    context->mark_stack.marks[idx].context_idx = context->count - 1;
    context->mark_stack.count++;

    return OK(NULL);
}

// Helper: log rewind error with message and id
static void log_rewind_error(ik_logger_t *logger, const char *message, int64_t id)
{
    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);
    if (root == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(log_doc, root, "message", message)) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_int(log_doc, root, "id", (int64_t)id)) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_logger_error_json(logger, log_doc);
}

// Helper: parse target_message_id from rewind event data
static int64_t parse_rewind_target(const char *data_json, int64_t id,
                                   ik_logger_t *logger, bool *valid)
{
    *valid = false;

    if (data_json == NULL) {
        log_rewind_error(logger, "Malformed rewind event: missing data field", id);
        return 0;
    }

    yyjson_doc *doc = yyjson_read_(data_json, strlen(data_json), 0);
    if (doc == NULL) {
        log_rewind_error(logger, "Malformed rewind event: invalid JSON in data field", id);
        return 0;
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    yyjson_val *target_val = yyjson_obj_get_(root, "target_message_id");
    if (target_val == NULL || !yyjson_is_int(target_val)) {
        log_rewind_error(logger, "Malformed rewind event: missing or invalid target_message_id", id);
        yyjson_doc_free(doc);
        return 0;
    }

    int64_t target_message_id = yyjson_get_sint_(target_val);
    yyjson_doc_free(doc);
    *valid = true;
    return target_message_id;
}

// Helper: process rewind event
static res_t process_rewind_event(ik_replay_context_t *context, int64_t id,
                                  const char *kind, const char *content,
                                  const char *data_json, ik_logger_t *logger)
{
    bool valid;
    int64_t target_message_id = parse_rewind_target(data_json, id, logger, &valid);
    if (!valid) {
        return OK(NULL);
    }

    ik_replay_mark_t *mark = find_mark(context, target_message_id);
    if (mark == NULL) {
        yyjson_mut_doc *log_doc = ik_log_create();
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);
        if (log_root == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_str(log_doc, log_root, "message",
                                    "Invalid rewind event: target mark not found")) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_int(log_doc, log_root, "id", (int64_t)id)) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_int(log_doc, log_root, "target_message_id",
                                    target_message_id)) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        ik_logger_error_json(logger, log_doc);
        return OK(NULL);
    }

    context->count = mark->context_idx + 1;
    size_t mark_idx = (size_t)(mark - context->mark_stack.marks);
    context->mark_stack.count = mark_idx + 1;

    return append_message(context, id, kind, content, data_json);
}

// Helper: check if event kind is a message type
static bool is_message_event(const char *kind)
{
    return strcmp(kind, "system") == 0 || strcmp(kind, "user") == 0 ||
           strcmp(kind, "assistant") == 0 || strcmp(kind, "tool_call") == 0 ||
           strcmp(kind, "tool_result") == 0;
}

// Helper: process a single event according to replay algorithm
static res_t process_event(ik_replay_context_t *context, int64_t id,
                           const char *kind, const char *content,
                           const char *data_json, ik_logger_t *logger)
{
    assert(context != NULL); // LCOV_EXCL_BR_LINE
    assert(kind != NULL);  // LCOV_EXCL_BR_LINE

    if (strcmp(kind, "clear") == 0) {
        context->count = 0;
        context->mark_stack.count = 0;
        return OK(NULL);
    }

    if (is_message_event(kind)) {
        return append_message(context, id, kind, content, data_json);
    }

    if (strcmp(kind, "mark") == 0) {
        return process_mark_event(context, id, kind, content, data_json);
    }

    if (strcmp(kind, "rewind") == 0) {
        return process_rewind_event(context, id, kind, content, data_json, logger);
    }

    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);
    if (log_root == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(log_doc, log_root, "message", "Unknown event kind")) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(log_doc, log_root, "kind", kind)) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_int(log_doc, log_root, "id", (int64_t)id)) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_logger_error_json(logger, log_doc);
    return OK(NULL);
}

// TEST-ONLY FUNCTION
// This function queries messages by session_id only (not agent_uuid).
// Production code uses ik_agent_replay_history() for agent-based replay.
// Retained for test convenience - has no production callers.
res_t ik_db_messages_load(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx,
                          int64_t session_id, ik_logger_t *logger)
{
    // Preconditions
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(db_ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(db_ctx->conn != NULL); // LCOV_EXCL_BR_LINE
    assert(session_id > 0);     // LCOV_EXCL_BR_LINE

    // Create temporary context for query
    TALLOC_CTX *tmp = tmp_ctx_create();

    // Allocate context structure on provided context
    ik_replay_context_t *context = talloc_zero(ctx, ik_replay_context_t);
    if (context == NULL)    // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE

    // Initialize context (capacity = 0, count = 0, messages = NULL)
    context->messages = NULL;
    context->count = 0;
    context->capacity = 0;

    // Initialize mark stack (capacity = 0, count = 0, marks = NULL)
    context->mark_stack.marks = NULL;
    context->mark_stack.count = 0;
    context->mark_stack.capacity = 0;

    // Prepare query: SELECT id, kind, content, data FROM messages
    //                WHERE session_id = $1 ORDER BY created_at
    const char *query = "SELECT id, kind, content, data FROM messages "
                        "WHERE session_id = $1 ORDER BY created_at";

    // Prepare session_id parameter
    char *session_id_str = talloc_asprintf(tmp, "%lld", (long long)session_id);
    if (session_id_str == NULL) // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE

    const char *params[1];
    params[0] = session_id_str;

    // Execute query and wrap result for automatic cleanup
    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, 1, NULL,
                                                  params, NULL, NULL, 0));
    PGresult *pg_res = res_wrapper->pg_result;

    // Check for database error
    if (PQresultStatus(pg_res) != PGRES_TUPLES_OK) {
        const char *errmsg = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(ctx, IO, "Database query failed: %s", errmsg);
    }

    // Process each row (event)
    int num_rows = PQntuples(pg_res);
    for (int i = 0; i < num_rows; i++) {
        // Extract fields
        const char *id_str = PQgetvalue_(pg_res, i, 0);
        const char *kind = PQgetvalue_(pg_res, i, 1);
        const char *content =
            PQgetisnull(pg_res, i, 2) ? NULL : PQgetvalue_(pg_res, i, 2);
        const char *data_json =
            PQgetisnull(pg_res, i, 3) ? NULL : PQgetvalue_(pg_res, i, 3);

        // Parse ID
        int64_t id = 0;
        if (sscanf(id_str, "%lld", (long long *)&id) != 1) {
            talloc_free(tmp);
            return ERR(ctx, PARSE, "Failed to parse message ID");
        }

        // Process event
        res_t res = process_event(context, id, kind, content, data_json, logger);
        if (is_err(&res)) { // LCOV_EXCL_BR_LINE - never returns error
            talloc_free(tmp); // LCOV_EXCL_LINE
            return res; // LCOV_EXCL_LINE
        }
    }

    // Cleanup
    talloc_free(tmp);

    return OK(context);
}
