/**
 * @file session_test.c
 * @brief Database session tests using unified test utilities
 *
 * This file demonstrates the recommended pattern for database tests:
 * - Per-file database for parallel execution
 * - Transaction isolation between tests
 * - Proper setup/teardown using test utilities
 */

#include "../../../src/db/session.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../test_utils.h"
#include <check.h>
#include <libpq-fe.h>
#include <talloc.h>
#include <string.h>
#include <unistd.h>

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
#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// ========== Tests ==========

// Test: ik_db_session_create creates a new session with valid id
START_TEST(test_db_session_create_success) {
    SKIP_IF_NO_DB();

    int64_t session_id = 0;
    res_t res = ik_db_session_create(db, &session_id);

    ck_assert(is_ok(&res));
    ck_assert_int_gt(session_id, 0);
}
END_TEST
// Test: ik_db_session_get_active returns none when database is empty
START_TEST(test_db_session_get_active_no_sessions) {
    SKIP_IF_NO_DB();

    int64_t session_id = 0;
    res_t res = ik_db_session_get_active(db, &session_id);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(session_id, 0);
}

END_TEST
// Test: ik_db_session_get_active finds active session
START_TEST(test_db_session_get_active_with_active_session) {
    SKIP_IF_NO_DB();

    // Create a session
    int64_t created_id = 0;
    res_t create_res = ik_db_session_create(db, &created_id);
    ck_assert(is_ok(&create_res));

    // Get active session
    int64_t found_id = 0;
    res_t res = ik_db_session_get_active(db, &found_id);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(found_id, created_id);
}

END_TEST
// Test: ik_db_session_get_active returns none when all sessions are ended
START_TEST(test_db_session_get_active_only_ended_sessions) {
    SKIP_IF_NO_DB();

    // Create and end a session
    int64_t session_id = 0;
    res_t create_res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&create_res));

    // Directly update the session to mark it as ended (inline ik_db_session_end logic)
    const char *end_query = "UPDATE sessions SET ended_at = NOW() WHERE id = $1";
    const char *param_values[1];
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%lld", (long long)session_id);
    param_values[0] = id_str;

    PGresult *end_result = PQexecParams(db->conn, end_query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(end_result), PGRES_COMMAND_OK);
    PQclear(end_result);

    // Get active session - should find none
    int64_t found_id = 0;
    res_t res = ik_db_session_get_active(db, &found_id);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(found_id, 0);
}

END_TEST
// Test: ik_db_session_get_active returns most recent when multiple active sessions exist
START_TEST(test_db_session_get_active_multiple_sessions) {
    SKIP_IF_NO_DB();

    // Create first session
    int64_t session1_id = 0;
    res_t create1_res = ik_db_session_create(db, &session1_id);
    ck_assert(is_ok(&create1_res));

    // Small delay to ensure different timestamps
    usleep(10000);

    // Create second session
    int64_t session2_id = 0;
    res_t create2_res = ik_db_session_create(db, &session2_id);
    ck_assert(is_ok(&create2_res));

    // Get active session - should return most recent (session2)
    int64_t found_id = 0;
    res_t res = ik_db_session_get_active(db, &found_id);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(found_id, session2_id);
}

END_TEST
// Test: Full session round-trip (create -> active -> end -> no active)
START_TEST(test_db_session_round_trip) {
    SKIP_IF_NO_DB();

    // Step 1: Create session
    int64_t created_id = 0;
    res_t create_res = ik_db_session_create(db, &created_id);
    ck_assert(is_ok(&create_res));
    ck_assert_int_gt(created_id, 0);

    // Step 2: Verify it's active
    int64_t active_id = 0;
    res_t active_res = ik_db_session_get_active(db, &active_id);
    ck_assert(is_ok(&active_res));
    ck_assert_int_eq(active_id, created_id);

    // Step 3: End session (inline logic)
    const char *end_query = "UPDATE sessions SET ended_at = NOW() WHERE id = $1";
    const char *param_values[1];
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%lld", (long long)created_id);
    param_values[0] = id_str;

    PGresult *end_result = PQexecParams(db->conn, end_query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(end_result), PGRES_COMMAND_OK);
    PQclear(end_result);

    // Step 4: Verify no active session
    int64_t after_end_id = 0;
    res_t after_end_res = ik_db_session_get_active(db, &after_end_id);
    ck_assert(is_ok(&after_end_res));
    ck_assert_int_eq(after_end_id, 0);
}

END_TEST
// Test: Session ID is a valid positive integer (BIGSERIAL behavior)
START_TEST(test_db_session_id_valid_bigserial) {
    SKIP_IF_NO_DB();

    // Create multiple sessions and verify IDs are sequential
    int64_t id1 = 0, id2 = 0, id3 = 0;

    res_t res1 = ik_db_session_create(db, &id1);
    ck_assert(is_ok(&res1));
    ck_assert_int_gt(id1, 0);

    res_t res2 = ik_db_session_create(db, &id2);
    ck_assert(is_ok(&res2));
    ck_assert_int_gt(id2, id1);

    res_t res3 = ik_db_session_create(db, &id3);
    ck_assert(is_ok(&res3));
    ck_assert_int_gt(id3, id2);
}

END_TEST
// Test: started_at is automatically set on session creation
START_TEST(test_db_session_started_at_automatic) {
    SKIP_IF_NO_DB();

    // Create session
    int64_t session_id = 0;
    res_t create_res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&create_res));

    // Query to verify started_at is not NULL
    const char *query = "SELECT started_at FROM sessions WHERE id = $1";
    const char *param_values[1];
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%lld", (long long)session_id);
    param_values[0] = id_str;

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert(!PQgetisnull(result, 0, 0));

    PQclear(result);
}

END_TEST

// ========== Suite Configuration ==========

static Suite *db_session_suite(void)
{
    Suite *s = suite_create("db_session");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_db_session_create_success);
    tcase_add_test(tc_core, test_db_session_get_active_no_sessions);
    tcase_add_test(tc_core, test_db_session_get_active_with_active_session);
    tcase_add_test(tc_core, test_db_session_get_active_only_ended_sessions);
    tcase_add_test(tc_core, test_db_session_get_active_multiple_sessions);
    tcase_add_test(tc_core, test_db_session_round_trip);
    tcase_add_test(tc_core, test_db_session_id_valid_bigserial);
    tcase_add_test(tc_core, test_db_session_started_at_automatic);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = db_session_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
