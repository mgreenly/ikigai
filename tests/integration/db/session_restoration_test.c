/**
 * @file session_restoration_test.c
 * @brief Integration tests for session restoration across app launches (Model B)
 *
 * Uses per-file database isolation for parallel test execution.
 */

#include "../../../src/db/session.h"
#include "../../../src/db/message.h"
#include "../../../src/db/replay.h"
#include "../../../src/error.h"
#include "../../test_utils.h"
#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// ========== Test Database Setup ==========
// Each test file gets its own database for parallel execution

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;

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

// Per-test setup: Connect and begin transaction
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
#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while(0)

// ========== Tests ==========

// Test 1: Fresh start - no active session
START_TEST(test_fresh_start_no_active_session)
{
    SKIP_IF_NO_DB();

    // Simulate app launch - check for active session
    int64_t active_session_id = 0;
    res_t get_res = ik_db_session_get_active(db, &active_session_id);
    ck_assert(is_ok(&get_res));
    ck_assert_int_eq(active_session_id, 0);  // No active session

    // Create new session (as app would do)
    int64_t new_session_id = 0;
    res_t create_res = ik_db_session_create(db, &new_session_id);
    ck_assert(is_ok(&create_res));
    ck_assert_int_gt(new_session_id, 0);

    // Write initial clear event
    ik_db_message_insert(db, new_session_id, NULL, "clear", NULL, "{}");
}
END_TEST

// Test 2: Active session continuation
START_TEST(test_active_session_continuation)
{
    SKIP_IF_NO_DB();

    // Launch 1: Create session and add messages
    int64_t session_id = 0;
    ik_db_session_create(db, &session_id);
    ik_db_message_insert(db, session_id, NULL, "clear", NULL, "{}");
    ik_db_message_insert(db, session_id, NULL, "user", "Hello", "{}");
    ik_db_message_insert(db, session_id, NULL, "assistant", "Hi there", "{}");

    // Launch 2: Detect active session
    int64_t active_session_id = 0;
    res_t get_res = ik_db_session_get_active(db, &active_session_id);
    ck_assert(is_ok(&get_res));
    ck_assert_int_eq(active_session_id, session_id);

    // Replay to restore context
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, active_session_id);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;
    ck_assert_int_eq((int)context->count, 2);  // user + assistant (clear doesn't persist)
    ck_assert_str_eq(context->messages[0]->content, "Hello");
    ck_assert_str_eq(context->messages[1]->content, "Hi there");

    talloc_free(replay_ctx);
}
END_TEST

// Test 3: Multi-launch conversation
START_TEST(test_multi_launch_conversation)
{
    SKIP_IF_NO_DB();

    // Launch 1: Create session, add initial messages
    int64_t session_id = 0;
    ik_db_session_create(db, &session_id);
    ik_db_message_insert(db, session_id, NULL, "clear", NULL, "{}");
    ik_db_message_insert(db, session_id, NULL, "user", "Q1", "{}");
    ik_db_message_insert(db, session_id, NULL, "assistant", "A1", "{}");
    // Simulate app exit (session stays active)

    // Launch 2: Detect active session, add more messages
    int64_t active_id = 0;
    ik_db_session_get_active(db, &active_id);
    ck_assert_int_eq(active_id, session_id);

    // Replay to restore context
    TALLOC_CTX *replay_ctx1 = talloc_new(test_ctx);
    res_t replay1 = ik_db_messages_load(replay_ctx1, db, active_id);
    ik_replay_context_t *ctx1 = replay1.ok;
    ck_assert_int_eq((int)ctx1->count, 2);  // Q1, A1
    talloc_free(replay_ctx1);

    // Continue conversation
    ik_db_message_insert(db, active_id, NULL, "user", "Q2", "{}");
    ik_db_message_insert(db, active_id, NULL, "assistant", "A2", "{}");
    // Simulate app exit

    // Launch 3: Detect active session, verify full context
    int64_t active_id2 = 0;
    ik_db_session_get_active(db, &active_id2);
    ck_assert_int_eq(active_id2, session_id);

    TALLOC_CTX *replay_ctx2 = talloc_new(test_ctx);
    res_t replay2 = ik_db_messages_load(replay_ctx2, db, active_id2);
    ik_replay_context_t *ctx2 = replay2.ok;
    ck_assert_int_eq((int)ctx2->count, 4);  // Q1, A1, Q2, A2
    ck_assert_str_eq(ctx2->messages[0]->content, "Q1");
    ck_assert_str_eq(ctx2->messages[1]->content, "A1");
    ck_assert_str_eq(ctx2->messages[2]->content, "Q2");
    ck_assert_str_eq(ctx2->messages[3]->content, "A2");

    talloc_free(replay_ctx2);
}
END_TEST

// Test 4: Clear persists across launches
START_TEST(test_clear_persists_across_launches)
{
    SKIP_IF_NO_DB();

    // Launch 1: Create session with messages, then clear, then new message
    int64_t session_id = 0;
    ik_db_session_create(db, &session_id);
    ik_db_message_insert(db, session_id, NULL, "clear", NULL, "{}");
    ik_db_message_insert(db, session_id, NULL, "user", "Old message", "{}");
    ik_db_message_insert(db, session_id, NULL, "assistant", "Old response", "{}");
    ik_db_message_insert(db, session_id, NULL, "clear", NULL, "{}");
    ik_db_message_insert(db, session_id, NULL, "user", "New message", "{}");
    // Simulate app exit

    // Launch 2: Detect active session and replay
    int64_t active_id = 0;
    ik_db_session_get_active(db, &active_id);
    ck_assert_int_eq(active_id, session_id);

    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, active_id);
    ik_replay_context_t *context = replay_res.ok;

    // Only message after last clear should be in context
    ck_assert_int_eq((int)context->count, 1);
    ck_assert_str_eq(context->messages[0]->content, "New message");

    talloc_free(replay_ctx);
}
END_TEST

// Test 5: Active session detection with multiple sessions
START_TEST(test_active_session_with_multiple_sessions)
{
    SKIP_IF_NO_DB();

    // Create session 1 and end it
    int64_t session1_id = 0;
    ik_db_session_create(db, &session1_id);
    ik_db_message_insert(db, session1_id, NULL, "user", "Session 1", "{}");
    ik_db_session_end(db, session1_id);

    // Create session 2 (leave active)
    int64_t session2_id = 0;
    ik_db_session_create(db, &session2_id);
    ik_db_message_insert(db, session2_id, NULL, "user", "Session 2", "{}");

    // Get active session - should return session 2, not session 1
    int64_t active_id = 0;
    res_t get_res = ik_db_session_get_active(db, &active_id);
    ck_assert(is_ok(&get_res));
    ck_assert_int_eq(active_id, session2_id);
}
END_TEST

// Test 6: Ended sessions are not restored
START_TEST(test_ended_sessions_not_restored)
{
    SKIP_IF_NO_DB();

    // Create session and end it
    int64_t session_id = 0;
    ik_db_session_create(db, &session_id);
    ik_db_message_insert(db, session_id, NULL, "user", "Message", "{}");
    ik_db_session_end(db, session_id);

    // Try to get active session - should return 0 (none)
    int64_t active_id = 0;
    res_t get_res = ik_db_session_get_active(db, &active_id);
    ck_assert(is_ok(&get_res));
    ck_assert_int_eq(active_id, 0);  // No active session
}
END_TEST

// Test 7: Most recent active session is selected
START_TEST(test_most_recent_active_session)
{
    SKIP_IF_NO_DB();

    // Create session 1 (older, active)
    int64_t session1_id = 0;
    ik_db_session_create(db, &session1_id);
    ik_db_message_insert(db, session1_id, NULL, "user", "Session 1", "{}");

    // Create session 2 (newer, active)
    int64_t session2_id = 0;
    ik_db_session_create(db, &session2_id);
    ik_db_message_insert(db, session2_id, NULL, "user", "Session 2", "{}");

    // Get active session - should return most recent (session 2)
    int64_t active_id = 0;
    res_t get_res = ik_db_session_get_active(db, &active_id);
    ck_assert(is_ok(&get_res));
    ck_assert_int_eq(active_id, session2_id);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *session_restoration_suite(void)
{
    Suite *s = suite_create("Session Restoration");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_fresh_start_no_active_session);
    tcase_add_test(tc_core, test_active_session_continuation);
    tcase_add_test(tc_core, test_multi_launch_conversation);
    tcase_add_test(tc_core, test_clear_persists_across_launches);
    tcase_add_test(tc_core, test_active_session_with_multiple_sessions);
    tcase_add_test(tc_core, test_ended_sessions_not_restored);
    tcase_add_test(tc_core, test_most_recent_active_session);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = session_restoration_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
