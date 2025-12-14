/**
 * @file rewind_target_index_test.c
 * @brief Integration test for Bug 4: Rewind target_message_index correctness
 *
 * Uses per-file database isolation for parallel test execution.
 */

#include "../../../src/commands.h"
#include "../../../src/agent.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/marks.h"
#include "../../../src/openai/client.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/vendor/yyjson/yyjson.h"
#include "../../test_utils.h"

#include <check.h>
#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;

// Suite-level setup
static void suite_setup(void)
{
    const char *skip_live = getenv("SKIP_LIVE_DB_TESTS");
    if (skip_live && strcmp(skip_live, "1") == 0) {
        db_available = false;
        return;
    }

    DB_NAME = ik_test_db_name(NULL, __FILE__);

    res_t res = ik_test_db_create(DB_NAME);
    if (is_err(&res)) {
        db_available = false;
        return;
    }

    res = ik_test_db_migrate(NULL, DB_NAME);
    if (is_err(&res)) {
        ik_test_db_destroy(DB_NAME);
        db_available = false;
        return;
    }

    db_available = true;
}

// Suite-level teardown
static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

// Per-test setup
static void test_setup(void)
{
    if (!db_available) {
        test_ctx = NULL;
        db = NULL;
        return;
    }

    test_ctx = talloc_new(NULL);
    res_t res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        return;
    }

    res = ik_test_db_begin(db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        return;
    }

    // Create a session for tests
    session_id = 0;
    res = ik_db_session_create(db, &session_id);
    if (is_err(&res)) {
        ik_test_db_rollback(db);
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

// Per-test teardown
static void test_teardown(void)
{
    if (test_ctx != NULL) {
        if (db != NULL) {
            ik_test_db_rollback(db);
        }
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while(0)

// ========== Tests ==========

// Test: Rewind persists correct target_message_index
START_TEST(test_rewind_persists_correct_target_message_index)
{
    SKIP_IF_NO_DB();

    // Create REPL context with database connection
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create shared context with config
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared->cfg = talloc_zero(test_ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(shared->cfg);
    repl->shared = shared;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    // Create scrollback
    repl->current->scrollback = ik_scrollback_create(repl, 80);
    ck_assert_ptr_nonnull(repl->current->scrollback);

    // Create conversation
    res_t conv_res = ik_openai_conversation_create(repl);
    ck_assert(is_ok(&conv_res));
    repl->current->conversation = conv_res.ok;

    // Initialize marks
    repl->current->marks = NULL;
    repl->current->mark_count = 0;

    // Set database context and session ID
    repl->shared->db_ctx = db;
    repl->shared->session_id = session_id;
    repl->shared->db_debug_pipe = NULL;

    // Build scenario from bug report:
    // DB Index 0: user message
    // DB Index 1: assistant message
    // DB Index 2: mark "test"
    // DB Index 3: mark "checkpoint-a" <- target
    // DB Index 4: user message
    // DB Index 5: assistant message

    // Persist user message (DB index 0, conversation index 0)
    ik_db_message_insert(db, session_id, "user", "Question 1", "{}");
    res_t msg_res = ik_openai_msg_create(repl->current->conversation, "user", "Question 1");
    ck_assert(is_ok(&msg_res));
    res_t add_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&add_res));

    // Persist assistant message (DB index 1, conversation index 1)
    ik_db_message_insert(db, session_id, "assistant", "Answer 1", "{}");
    msg_res = ik_openai_msg_create(repl->current->conversation, "assistant", "Answer 1");
    ck_assert(is_ok(&msg_res));
    add_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&add_res));

    // Create mark "test" (DB index 2, conversation message_index = 2)
    res_t mark_res = ik_cmd_dispatch(repl, repl, "/mark test");
    ck_assert(is_ok(&mark_res));

    // Create mark "checkpoint-a" (DB index 3, conversation message_index = 2)
    mark_res = ik_cmd_dispatch(repl, repl, "/mark checkpoint-a");
    ck_assert(is_ok(&mark_res));
    // Verify we have 2 marks now
    ck_assert_int_eq((int)repl->current->mark_count, 2);
    ck_assert_int_eq((int)repl->current->marks[1]->message_index, 2);  // Still 2, no new conversation messages

    // Add more messages after the marks (DB index 4, 5)
    ik_db_message_insert(db, session_id, "user", "Question 2", "{}");
    msg_res = ik_openai_msg_create(repl->current->conversation, "user", "Question 2");
    ck_assert(is_ok(&msg_res));
    add_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&add_res));

    ik_db_message_insert(db, session_id, "assistant", "Answer 2", "{}");
    msg_res = ik_openai_msg_create(repl->current->conversation, "assistant", "Answer 2");
    ck_assert(is_ok(&msg_res));
    add_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&add_res));

    // Query for the database ID of the "checkpoint-a" mark BEFORE rewinding
    const char *mark_query = "SELECT id FROM messages WHERE session_id = $1 AND kind = 'mark' AND data->>'label' = 'checkpoint-a'";
    const char *mark_params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *mark_result = PQexecParams(db->conn, mark_query, 1, NULL, mark_params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(mark_result), PGRES_TUPLES_OK);
    ck_assert_int_gt(PQntuples(mark_result), 0);
    int64_t expected_mark_id = 0;
    sscanf(PQgetvalue(mark_result, 0, 0), "%lld", (long long *)&expected_mark_id);
    PQclear(mark_result);

    // Now rewind to checkpoint-a
    res_t rewind_res = ik_cmd_dispatch(repl, repl, "/rewind checkpoint-a");
    ck_assert(is_ok(&rewind_res));

    // Query the database for the rewind event
    const char *query = "SELECT data FROM messages WHERE session_id = $1 AND kind = 'rewind' ORDER BY created_at DESC LIMIT 1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_gt(PQntuples(result), 0);

    // Parse the JSON data field
    const char *data_json = PQgetvalue(result, 0, 0);
    yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // BUG 4: The code writes "target_message_index" but should write "target_message_id"
    // The replay code expects "target_message_id" (the database ID of the mark)
    yyjson_val *id_val = yyjson_obj_get(root, "target_message_id");
    ck_assert_ptr_nonnull(id_val);  // This will FAIL - the field doesn't exist!
    ck_assert(yyjson_is_num(id_val));

    int64_t target_id = yyjson_get_sint(id_val);
    ck_assert_int_eq((int)target_id, (int)expected_mark_id);  // Should match the mark's DB ID

    // Verify target_label is "checkpoint-a"
    yyjson_val *label_val = yyjson_obj_get(root, "target_label");
    ck_assert_ptr_nonnull(label_val);
    ck_assert(yyjson_is_str(label_val));
    ck_assert_str_eq(yyjson_get_str(label_val), "checkpoint-a");

    yyjson_doc_free(doc);
    PQclear(result);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *rewind_target_index_suite(void)
{
    Suite *s = suite_create("Rewind Target Index");

    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_rewind_persists_correct_target_message_index);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = rewind_target_index_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
