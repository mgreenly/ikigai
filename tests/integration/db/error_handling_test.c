/**
 * @file error_handling_test.c
 * @brief Integration tests for database error handling
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

// Test 1: Message insert with invalid session_id (foreign key violation)
START_TEST(test_message_insert_invalid_session)
{
    SKIP_IF_NO_DB();

    // Try to insert message with non-existent session_id
    int64_t invalid_session_id = 999999;
    res_t insert_res = ik_db_message_insert(db, invalid_session_id,
                                             NULL, "user", "test", "{}");

    // Should return ERR due to foreign key violation
    ck_assert(is_err(&insert_res));
}
END_TEST

// Test 2: Replay with non-existent session (should succeed with empty context)
START_TEST(test_replay_nonexistent_session)
{
    SKIP_IF_NO_DB();

    // Try to replay non-existent session
    int64_t nonexistent_session = 999999;
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, nonexistent_session);

    // Should succeed with empty context
    ck_assert(is_ok(&replay_res));
    ik_replay_context_t *context = replay_res.ok;
    ck_assert_int_eq((int)context->count, 0);

    talloc_free(replay_ctx);
}
END_TEST

// Test 3: End non-existent session (should succeed silently)
START_TEST(test_end_nonexistent_session)
{
    SKIP_IF_NO_DB();

    // Try to end non-existent session
    int64_t nonexistent_session = 999999;
    res_t end_res = ik_db_session_end(db, nonexistent_session);

    // Should succeed (UPDATE with no matching rows)
    ck_assert(is_ok(&end_res));
}
END_TEST

// Test 4: Get active session from empty database
START_TEST(test_get_active_empty_database)
{
    SKIP_IF_NO_DB();

    // Try to get active session when none exist
    int64_t active_id = 0;
    res_t get_res = ik_db_session_get_active(db, &active_id);

    // Should succeed with session_id = 0
    ck_assert(is_ok(&get_res));
    ck_assert_int_eq(active_id, 0);
}
END_TEST

// Test 5: Multiple errors don't crash
START_TEST(test_multiple_errors_dont_crash)
{
    SKIP_IF_NO_DB();

    // Try multiple invalid operations - should all return ERR
    for (int i = 0; i < 5; i++) {
        res_t insert_res = ik_db_message_insert(db, 999999,
                                                 NULL, "user", "test", "{}");
        ck_assert(is_err(&insert_res));
    }
}
END_TEST

// ========== Suite Configuration ==========

static Suite *error_handling_suite(void)
{
    Suite *s = suite_create("Error Handling");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_message_insert_invalid_session);
    tcase_add_test(tc_core, test_replay_nonexistent_session);
    tcase_add_test(tc_core, test_end_nonexistent_session);
    tcase_add_test(tc_core, test_get_active_empty_database);
    tcase_add_test(tc_core, test_multiple_errors_dont_crash);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = error_handling_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
