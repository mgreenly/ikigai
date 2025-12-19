/**
 * @file replay_mark_rewind_test.c
 * @brief Integration tests for mark/rewind replay (checkpoint and rollback)
 *
 * Uses per-file database isolation for parallel test execution.
 */

#include "../../../src/db/message.h"
#include "../../../src/db/replay.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
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

// Test 1: Simple mark - mark appears in context
START_TEST(test_replay_simple_mark)
{
    SKIP_IF_NO_DB();

    // Insert [user, assistant, mark]
    ik_db_message_insert(db, session_id, NULL, "user", "Q1", "{}");
    ik_db_message_insert(db, session_id, NULL, "assistant", "A1", "{}");
    ik_db_message_insert(db, session_id, NULL, "mark", "checkpoint1",
                          "{\"label\":\"checkpoint1\"}");

    // Replay
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id, NULL);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;
    // Should have user, assistant, mark in context
    ck_assert_int_eq((int)context->count, 3);
    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[1]->kind, "assistant");
    ck_assert_str_eq(context->messages[2]->kind, "mark");

    // Mark stack should have 1 entry
    ck_assert_int_eq((int)context->mark_stack.count, 1);

    talloc_free(replay_ctx);
}
END_TEST

// Test 2: Rewind to mark - truncates context to mark position
START_TEST(test_replay_rewind_to_mark)
{
    SKIP_IF_NO_DB();

    // Insert [user, mark, assistant]
    ik_db_message_insert(db, session_id, NULL, "user", "Q1", "{}");
    ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"m1\"}");

    // Get mark's message ID (query for mark specifically)
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT id FROM messages WHERE session_id = %lld "
                                  "AND kind = 'mark' ORDER BY created_at DESC LIMIT 1",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_gt(PQntuples(result), 0);
    int64_t mark_id = 0;
    sscanf(PQgetvalue(result, 0, 0), "%lld", (long long *)&mark_id);
    PQclear(result);

    // Insert message after mark
    ik_db_message_insert(db, session_id, NULL, "assistant", "A1", "{}");

    // Insert rewind to mark
    char data[128];
    snprintf(data, sizeof(data), "{\"target_message_id\":%lld,\"label\":\"m1\"}",
             (long long)mark_id);
    ik_db_message_insert(db, session_id, NULL, "rewind", NULL, data);

    // Replay
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id, NULL);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;

    // Should have: user, mark, rewind (assistant is truncated)
    ck_assert_int_eq((int)context->count, 3);
    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[1]->kind, "mark");
    ck_assert_str_eq(context->messages[2]->kind, "rewind");

    talloc_free(replay_ctx);
}
END_TEST

// Test 3: Multiple marks with rewind to first
START_TEST(test_replay_multiple_marks_rewind_first)
{
    SKIP_IF_NO_DB();

    // Insert [user, mark('a'), assistant, mark('b'), user]
    ik_db_message_insert(db, session_id, NULL, "user", "U1", "{}");
    ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"a\"}");

    // Get first mark's ID (query for mark with label 'a')
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT id FROM messages WHERE session_id = %lld "
                                  "AND kind = 'mark' AND data->>'label' = 'a'",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_gt(PQntuples(result), 0);
    int64_t mark_a_id = 0;
    sscanf(PQgetvalue(result, 0, 0), "%lld", (long long *)&mark_a_id);
    PQclear(result);

    ik_db_message_insert(db, session_id, NULL, "assistant", "A1", "{}");
    ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"b\"}");
    ik_db_message_insert(db, session_id, NULL, "user", "U2", "{}");

    // Rewind to mark 'a'
    char data[128];
    snprintf(data, sizeof(data), "{\"target_message_id\":%lld,\"label\":\"a\"}",
             (long long)mark_a_id);
    ik_db_message_insert(db, session_id, NULL, "rewind", NULL, data);

    // Replay
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id, NULL);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;

    // Should have: user, mark('a'), rewind
    // (assistant, mark('b'), and second user are truncated)
    ck_assert_int_eq((int)context->count, 3);
    ck_assert_str_eq(context->messages[0]->content, "U1");
    ck_assert_str_eq(context->messages[1]->kind, "mark");
    ck_assert_str_eq(context->messages[2]->kind, "rewind");

    talloc_free(replay_ctx);
}
END_TEST

// Test 4: Rewind removes subsequent marks from mark stack
START_TEST(test_replay_rewind_removes_subsequent_marks)
{
    SKIP_IF_NO_DB();

    // Insert [user, mark1, user, mark2]
    ik_db_message_insert(db, session_id, NULL, "user", "U1", "{}");
    ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"m1\"}");

    // Get first mark's ID (query for mark with label 'm1')
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT id FROM messages WHERE session_id = %lld "
                                  "AND kind = 'mark' AND data->>'label' = 'm1'",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_gt(PQntuples(result), 0);
    int64_t mark1_id = 0;
    sscanf(PQgetvalue(result, 0, 0), "%lld", (long long *)&mark1_id);
    PQclear(result);

    ik_db_message_insert(db, session_id, NULL, "user", "U2", "{}");
    ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"m2\"}");

    // Rewind to mark1
    char data[128];
    snprintf(data, sizeof(data), "{\"target_message_id\":%lld,\"label\":\"m1\"}",
             (long long)mark1_id);
    ik_db_message_insert(db, session_id, NULL, "rewind", NULL, data);

    // Replay
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id, NULL);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;

    // Mark stack should only have mark1 (mark2 was removed by rewind)
    ck_assert_int_eq((int)context->mark_stack.count, 1);
    ck_assert_int_eq(context->mark_stack.marks[0].message_id, mark1_id);

    talloc_free(replay_ctx);
}
END_TEST

// Test 5: Mark labels preserved in mark stack
START_TEST(test_replay_mark_labels_preserved)
{
    SKIP_IF_NO_DB();

    // Insert marks with different labels
    ik_db_message_insert(db, session_id, NULL, "mark", NULL,
                          "{\"label\":\"alpha\"}");
    ik_db_message_insert(db, session_id, NULL, "mark", NULL,
                          "{\"label\":\"beta\"}");

    // Replay
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id, NULL);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;

    // Verify labels in mark stack
    ck_assert_int_eq((int)context->mark_stack.count, 2);
    ck_assert_str_eq(context->mark_stack.marks[0].label, "alpha");
    ck_assert_str_eq(context->mark_stack.marks[1].label, "beta");

    talloc_free(replay_ctx);
}
END_TEST

// Test 6: Mark without label (auto-numbered)
START_TEST(test_replay_mark_without_label)
{
    SKIP_IF_NO_DB();

    // Insert mark with no label
    ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{}");

    // Replay
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id, NULL);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;

    // Mark should be in stack with NULL label
    ck_assert_int_eq((int)context->mark_stack.count, 1);
    ck_assert(context->mark_stack.marks[0].label == NULL);

    talloc_free(replay_ctx);
}
END_TEST

// Test 7: Clear resets mark stack
START_TEST(test_replay_clear_resets_mark_stack)
{
    SKIP_IF_NO_DB();

    // Insert mark, then clear
    ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"m1\"}");
    ik_db_message_insert(db, session_id, NULL, "clear", NULL, "{}");
    ik_db_message_insert(db, session_id, NULL, "user", "After clear", "{}");

    // Replay
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id, NULL);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;

    // Mark stack should be empty (clear resets it)
    ck_assert_int_eq((int)context->mark_stack.count, 0);
    // Only user message after clear
    ck_assert_int_eq((int)context->count, 1);
    ck_assert_str_eq(context->messages[0]->content, "After clear");

    talloc_free(replay_ctx);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *replay_mark_rewind_suite(void)
{
    Suite *s = suite_create("Replay Mark/Rewind");

    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_replay_simple_mark);
    tcase_add_test(tc_core, test_replay_rewind_to_mark);
    tcase_add_test(tc_core, test_replay_multiple_marks_rewind_first);
    tcase_add_test(tc_core, test_replay_rewind_removes_subsequent_marks);
    tcase_add_test(tc_core, test_replay_mark_labels_preserved);
    tcase_add_test(tc_core, test_replay_mark_without_label);
    tcase_add_test(tc_core, test_replay_clear_resets_mark_stack);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = replay_mark_rewind_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
