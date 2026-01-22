#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/replay.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

// ========== Test Database Setup ==========
// Each test file gets its own database for parallel execution

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;

// Suite-level setup: Create and migrate database (runs once)
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

// Suite-level teardown: Destroy database (runs once)
static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

// Per-test setup: Connect, begin transaction, create session
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

// Per-test teardown: Rollback and cleanup
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

// Helper macro to skip test if DB not available
#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// ========== Tests ==========

START_TEST(test_simple_mark) {
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user
    res = ik_db_message_insert(db, session_id, NULL, "user", "Hello", NULL);
    ck_assert(is_ok(&res));

    // Insert assistant
    res = ik_db_message_insert(db, session_id, NULL, "assistant", "Hi there!", NULL);
    ck_assert(is_ok(&res));

    // Insert mark with label
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"checkpoint1\"}");
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 3); // user + assistant + mark

    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[1]->kind, "assistant");
    ck_assert_str_eq(context->messages[2]->kind, "mark");
}
END_TEST
// Test rewind to mark: [user, assistant, mark, user, assistant, rewind] -> [user, assistant, mark, rewind]
START_TEST(test_rewind_to_mark) {
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user + assistant
    res = ik_db_message_insert(db, session_id, NULL, "user", "Message 1", NULL);
    ck_assert(is_ok(&res));

    res = ik_db_message_insert(db, session_id, NULL, "assistant", "Response 1", NULL);
    ck_assert(is_ok(&res));

    // Insert mark
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"checkpoint1\"}");
    ck_assert(is_ok(&res));

    // Query to get the mark's message ID
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT id FROM messages WHERE session_id = %" PRId64
                                  " AND kind = 'mark' ORDER BY created_at DESC LIMIT 1",
                                  session_id);
    PGresult *mark_query = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(mark_query), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(mark_query), 1);

    const char *mark_id_str = PQgetvalue(mark_query, 0, 0);
    int64_t mark_id = 0;
    sscanf(mark_id_str, "%" SCNd64, &mark_id);
    PQclear(mark_query);

    // Insert more messages after mark
    res = ik_db_message_insert(db, session_id, NULL, "user", "Message 2", NULL);
    ck_assert(is_ok(&res));

    res = ik_db_message_insert(db, session_id, NULL, "assistant", "Response 2", NULL);
    ck_assert(is_ok(&res));

    // Insert rewind pointing to the mark
    char *rewind_data = talloc_asprintf(test_ctx, "{\"target_message_id\":%" PRId64 "}", mark_id);
    res = ik_db_message_insert(db, session_id, NULL, "rewind", NULL, rewind_data);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 4); // user + assistant + mark + rewind

    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[0]->content, "Message 1");

    ck_assert_str_eq(context->messages[1]->kind, "assistant");
    ck_assert_str_eq(context->messages[1]->content, "Response 1");

    ck_assert_str_eq(context->messages[2]->kind, "mark");

    ck_assert_str_eq(context->messages[3]->kind, "rewind");
}

END_TEST
// Test rewind truncates correctly: [user, mark, assistant, user, rewind] -> verify assistant and second user not in context
START_TEST(test_rewind_truncates_messages) {
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user
    res = ik_db_message_insert(db, session_id, NULL, "user", "Before mark", NULL);
    ck_assert(is_ok(&res));

    // Insert mark
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"save_point\"}");
    ck_assert(is_ok(&res));

    // Get mark ID
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT id FROM messages WHERE session_id = %" PRId64
                                  " AND kind = 'mark' ORDER BY created_at DESC LIMIT 1",
                                  session_id);
    PGresult *mark_query = PQexec(db->conn, query);
    const char *mark_id_str = PQgetvalue(mark_query, 0, 0);
    int64_t mark_id = 0;
    sscanf(mark_id_str, "%" SCNd64, &mark_id);
    PQclear(mark_query);

    // Insert messages after mark (these should be truncated)
    res = ik_db_message_insert(db, session_id, NULL, "assistant", "After mark", NULL);
    ck_assert(is_ok(&res));

    res = ik_db_message_insert(db, session_id, NULL, "user", "More after mark", NULL);
    ck_assert(is_ok(&res));

    // Insert rewind
    char *rewind_data = talloc_asprintf(test_ctx, "{\"target_message_id\":%" PRId64 "}", mark_id);
    res = ik_db_message_insert(db, session_id, NULL, "rewind", NULL, rewind_data);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 3); // user + mark + rewind (assistant and second user truncated)

    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[0]->content, "Before mark");

    ck_assert_str_eq(context->messages[1]->kind, "mark");

    ck_assert_str_eq(context->messages[2]->kind, "rewind");
}

END_TEST
// Test multiple marks: [user, mark('a'), assistant, mark('b'), user, rewind('a')] -> [user, mark('a'), rewind]
START_TEST(test_multiple_marks) {
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user
    res = ik_db_message_insert(db, session_id, NULL, "user", "First message", NULL);
    ck_assert(is_ok(&res));

    // Insert mark 'a'
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"a\"}");
    ck_assert(is_ok(&res));

    // Get mark 'a' ID
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT id FROM messages WHERE session_id = %" PRId64
                                  " AND kind = 'mark' AND data->>'label' = 'a' LIMIT 1",
                                  session_id);
    PGresult *mark_query = PQexec(db->conn, query);
    const char *mark_a_id_str = PQgetvalue(mark_query, 0, 0);
    int64_t mark_a_id = 0;
    sscanf(mark_a_id_str, "%" SCNd64, &mark_a_id);
    PQclear(mark_query);

    // Insert assistant
    res = ik_db_message_insert(db, session_id, NULL, "assistant", "Response", NULL);
    ck_assert(is_ok(&res));

    // Insert mark 'b'
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"b\"}");
    ck_assert(is_ok(&res));

    // Insert user
    res = ik_db_message_insert(db, session_id, NULL, "user", "Another message", NULL);
    ck_assert(is_ok(&res));

    // Rewind to mark 'a'
    char *rewind_data = talloc_asprintf(test_ctx, "{\"target_message_id\":%" PRId64 "}", mark_a_id);
    res = ik_db_message_insert(db, session_id, NULL, "rewind", NULL, rewind_data);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 3); // user + mark('a') + rewind

    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[1]->kind, "mark");
    ck_assert_str_eq(context->messages[2]->kind, "rewind");
}

END_TEST
// Test rewind removes subsequent marks from mark stack
START_TEST(test_rewind_removes_subsequent_marks) {
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user
    res = ik_db_message_insert(db, session_id, NULL, "user", "Message", NULL);
    ck_assert(is_ok(&res));

    // Insert mark1
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"mark1\"}");
    ck_assert(is_ok(&res));

    // Get mark1 ID
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT id FROM messages WHERE session_id = %" PRId64
                                  " AND kind = 'mark' AND data->>'label' = 'mark1' LIMIT 1",
                                  session_id);
    PGresult *mark_query = PQexec(db->conn, query);
    const char *mark1_id_str = PQgetvalue(mark_query, 0, 0);
    int64_t mark1_id = 0;
    sscanf(mark1_id_str, "%" SCNd64, &mark1_id);
    PQclear(mark_query);

    // Insert user
    res = ik_db_message_insert(db, session_id, NULL, "user", "Another", NULL);
    ck_assert(is_ok(&res));

    // Insert mark2
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"mark2\"}");
    ck_assert(is_ok(&res));

    // Get mark2 ID
    query = talloc_asprintf(test_ctx,
                            "SELECT id FROM messages WHERE session_id = %" PRId64
                            " AND kind = 'mark' AND data->>'label' = 'mark2' LIMIT 1",
                            session_id);
    mark_query = PQexec(db->conn, query);
    const char *mark2_id_str = PQgetvalue(mark_query, 0, 0);
    int64_t mark2_id = 0;
    sscanf(mark2_id_str, "%" SCNd64, &mark2_id);
    PQclear(mark_query);

    // Rewind to mark1
    char *rewind_data = talloc_asprintf(test_ctx, "{\"target_message_id\":%" PRId64 "}", mark1_id);
    res = ik_db_message_insert(db, session_id, NULL, "rewind", NULL, rewind_data);
    ck_assert(is_ok(&res));

    // Now try to rewind to mark2 (should fail because mark2 was removed from stack)
    char *rewind_data2 = talloc_asprintf(test_ctx, "{\"target_message_id\":%" PRId64 "}", mark2_id);
    res = ik_db_message_insert(db, session_id, NULL, "rewind", NULL, rewind_data2);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    // After first rewind: user + mark1 + rewind
    // Second rewind should be skipped (mark2 not in stack) but the rewind event is still appended
    // Actually, according to design, invalid rewinds are skipped entirely (don't truncate, don't append)
    // So we should have: user + mark1 + rewind (from first rewind only)
    ck_assert_uint_eq(context->count, 3);
}

END_TEST
// Test mark labels: named marks vs auto-numbered marks
START_TEST(test_mark_labels) {
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user
    res = ik_db_message_insert(db, session_id, NULL, "user", "Message", NULL);
    ck_assert(is_ok(&res));

    // Insert mark with label
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"checkpoint\"}");
    ck_assert(is_ok(&res));

    // Insert mark with auto number
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"number\":1}");
    ck_assert(is_ok(&res));

    // Insert mark with NULL label (auto-generated)
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{}");
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 4); // user + 3 marks

    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[1]->kind, "mark");
    ck_assert_str_eq(context->messages[2]->kind, "mark");
    ck_assert_str_eq(context->messages[3]->kind, "mark");
}

END_TEST

// ========== Suite Configuration ==========

static Suite *replay_marks_basic_suite(void)
{
    Suite *s = suite_create("Replay Marks Basic");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_simple_mark);
    tcase_add_test(tc_core, test_rewind_to_mark);
    tcase_add_test(tc_core, test_rewind_truncates_messages);
    tcase_add_test(tc_core, test_multiple_marks);
    tcase_add_test(tc_core, test_rewind_removes_subsequent_marks);
    tcase_add_test(tc_core, test_mark_labels);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = replay_marks_basic_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/db/replay_marks_basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
