#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/replay.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

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

START_TEST(test_complex_mark_rewind_scenario) {
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // user
    res = ik_db_message_insert(db, session_id, NULL, "user", "msg1", NULL);
    ck_assert(is_ok(&res));

    // mark1
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"mark1\"}");
    ck_assert(is_ok(&res));

    char *query = talloc_asprintf(test_ctx,
                                  "SELECT id FROM messages WHERE session_id = %" PRId64
                                  " AND kind = 'mark' AND data->>'label' = 'mark1' LIMIT 1",
                                  session_id);
    PGresult *mark_query = PQexec(db->conn, query);
    const char *mark1_id_str = PQgetvalue(mark_query, 0, 0);
    int64_t mark1_id = 0;
    sscanf(mark1_id_str, "%" SCNd64, &mark1_id);
    PQclear(mark_query);

    // assistant
    res = ik_db_message_insert(db, session_id, NULL, "assistant", "resp1", NULL);
    ck_assert(is_ok(&res));

    // mark2
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"mark2\"}");
    ck_assert(is_ok(&res));

    // user
    res = ik_db_message_insert(db, session_id, NULL, "user", "msg2", NULL);
    ck_assert(is_ok(&res));

    // rewind(mark1)
    char *rewind_data = talloc_asprintf(test_ctx, "{\"target_message_id\":%" PRId64 "}", mark1_id);
    res = ik_db_message_insert(db, session_id, NULL, "rewind", NULL, rewind_data);
    ck_assert(is_ok(&res));

    // user
    res = ik_db_message_insert(db, session_id, NULL, "user", "msg3", NULL);
    ck_assert(is_ok(&res));

    // mark3
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"mark3\"}");
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    // Expected: user(msg1) + mark1 + rewind + user(msg3) + mark3
    ck_assert_uint_eq(context->count, 5);

    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[0]->content, "msg1");

    ck_assert_str_eq(context->messages[1]->kind, "mark");

    ck_assert_str_eq(context->messages[2]->kind, "rewind");

    ck_assert_str_eq(context->messages[3]->kind, "user");
    ck_assert_str_eq(context->messages[3]->content, "msg3");

    ck_assert_str_eq(context->messages[4]->kind, "mark");
}
END_TEST
// Test rewind without target mark (mark not in stack) -> skip rewind, log error
START_TEST(test_rewind_with_missing_mark)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user
    res = ik_db_message_insert(db, session_id, NULL, "user", "Message", NULL);
    ck_assert(is_ok(&res));

    // Insert rewind with non-existent mark ID (999999)
    res = ik_db_message_insert(db, session_id, NULL, "rewind", NULL, "{\"target_message_id\":999999}");
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    // Rewind should be skipped, only user message remains
    ck_assert_uint_eq(context->count, 1);

    ck_assert_str_eq(context->messages[0]->kind, "user");
}

END_TEST
// Test rewind with malformed JSONB -> skip rewind, log error
START_TEST(test_rewind_with_malformed_json)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user
    res = ik_db_message_insert(db, session_id, NULL, "user", "Message", NULL);
    ck_assert(is_ok(&res));

    // Insert rewind with valid JSON but missing target_message_id field
    res = ik_db_message_insert(db, session_id, NULL, "rewind", NULL, "{\"other_field\":\"value\"}");
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    // Rewind should be skipped, only user message remains
    ck_assert_uint_eq(context->count, 1);

    ck_assert_str_eq(context->messages[0]->kind, "user");
}

END_TEST
// Test rewind with invalid target_message_id -> skip rewind, log error
START_TEST(test_rewind_with_invalid_target_id)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user
    res = ik_db_message_insert(db, session_id, NULL, "user", "Message", NULL);
    ck_assert(is_ok(&res));

    // Insert rewind with missing target_message_id field
    res = ik_db_message_insert(db, session_id, NULL, "rewind", NULL, "{\"other_field\":123}");
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    // Rewind should be skipped, only user message remains
    ck_assert_uint_eq(context->count, 1);

    ck_assert_str_eq(context->messages[0]->kind, "user");
}

END_TEST
// Test clear empties mark stack: [user, mark, clear, rewind] -> rewind fails (mark cleared)
START_TEST(test_clear_empties_mark_stack)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user
    res = ik_db_message_insert(db, session_id, NULL, "user", "Message", NULL);
    ck_assert(is_ok(&res));

    // Insert mark
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"checkpoint\"}");
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

    // Insert another clear (empties mark stack)
    res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert rewind to mark (should fail - mark cleared)
    char *rewind_data = talloc_asprintf(test_ctx, "{\"target_message_id\":%" PRId64 "}", mark_id);
    res = ik_db_message_insert(db, session_id, NULL, "rewind", NULL, rewind_data);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    // Context should be empty (clear emptied it, rewind was skipped)
    ck_assert_uint_eq(context->count, 0);
}

END_TEST
// Test mark stack geometric growth (add many marks, verify capacity doubling)
START_TEST(test_mark_stack_growth)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert many marks to trigger growth
    // Initial capacity should be 4, then 8, then 16, etc.
    for (int i = 0; i < 10; i++) {
        char *label = talloc_asprintf(test_ctx, "mark_%d", i);
        char *data = talloc_asprintf(test_ctx, "{\"label\":\"%s\"}", label);
        res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, data);
        ck_assert(is_ok(&res));
    }

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 10); // All 10 marks

    // Verify all are mark events
    for (size_t i = 0; i < context->count; i++) {
        ck_assert_str_eq(context->messages[i]->kind, "mark");
    }
}

END_TEST
// Test multi-launch with marks: marks persist across app launches and replays
START_TEST(test_marks_persist_across_launches)
{
    SKIP_IF_NO_DB();

    // First "app launch" - create some messages with marks
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    res = ik_db_message_insert(db, session_id, NULL, "user", "First", NULL);
    ck_assert(is_ok(&res));

    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"save1\"}");
    ck_assert(is_ok(&res));

    res = ik_db_message_insert(db, session_id, NULL, "assistant", "Response", NULL);
    ck_assert(is_ok(&res));

    // Simulate app restart by loading and replaying
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context1 = res.ok;
    ck_assert_uint_eq(context1->count, 3); // user + mark + assistant

    // Second "app launch" - add more messages
    res = ik_db_message_insert(db, session_id, NULL, "user", "Second", NULL);
    ck_assert(is_ok(&res));

    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"save2\"}");
    ck_assert(is_ok(&res));

    // Replay again (simulate another restart)
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context2 = res.ok;
    ck_assert_uint_eq(context2->count, 5); // user + mark + assistant + user + mark

    ck_assert_str_eq(context2->messages[0]->kind, "user");
    ck_assert_str_eq(context2->messages[1]->kind, "mark");
    ck_assert_str_eq(context2->messages[2]->kind, "assistant");
    ck_assert_str_eq(context2->messages[3]->kind, "user");
    ck_assert_str_eq(context2->messages[4]->kind, "mark");
}

END_TEST

// ========== Suite Configuration ==========

static Suite *replay_marks_advanced_suite(void)
{
    Suite *s = suite_create("Replay Marks Advanced");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_complex_mark_rewind_scenario);
    tcase_add_test(tc_core, test_rewind_with_missing_mark);
    tcase_add_test(tc_core, test_rewind_with_malformed_json);
    tcase_add_test(tc_core, test_rewind_with_invalid_target_id);
    tcase_add_test(tc_core, test_clear_empties_mark_stack);
    tcase_add_test(tc_core, test_mark_stack_growth);
    tcase_add_test(tc_core, test_marks_persist_across_launches);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = replay_marks_advanced_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
