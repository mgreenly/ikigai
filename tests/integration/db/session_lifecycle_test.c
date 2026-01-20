/**
 * @file session_lifecycle_test.c
 * @brief Integration tests for session lifecycle (create, get_active, end)
 *
 * Uses per-file database isolation for parallel test execution.
 */

#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"
#include <check.h>
#include <libpq-fe.h>
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

// Test 1: Session create returns valid ID
START_TEST(test_session_create_returns_valid_id)
{
    SKIP_IF_NO_DB();

    int64_t session_id = 0;
    res_t res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(session_id, 0);
}
END_TEST

// Test 2: Session has started_at timestamp
START_TEST(test_session_has_started_at)
{
    SKIP_IF_NO_DB();

    int64_t session_id = 0;
    res_t res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&res));

    // Query to verify started_at is set
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT started_at FROM sessions WHERE id = %lld",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    // Verify started_at is not NULL
    ck_assert(!PQgetisnull(result, 0, 0));

    PQclear(result);
}
END_TEST

// Test 3: Session has ended_at = NULL initially
START_TEST(test_session_ended_at_null_initially)
{
    SKIP_IF_NO_DB();

    int64_t session_id = 0;
    res_t res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&res));

    // Query to verify ended_at is NULL
    char *query = talloc_asprintf(test_ctx,
                                  "SELECT ended_at FROM sessions WHERE id = %lld",
                                  (long long)session_id);
    PGresult *result = PQexec(db->conn, query);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    // Verify ended_at IS NULL
    ck_assert(PQgetisnull(result, 0, 0));

    PQclear(result);
}
END_TEST

// Test 6: Transaction isolation - no data persists after rollback
START_TEST(test_transaction_isolation)
{
    SKIP_IF_NO_DB();

    // Create a session within this test's transaction
    int64_t session_id = 0;
    res_t res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(session_id, 0);

    // Verify session exists within the transaction
    PGresult *result = PQexec(db->conn, "SELECT COUNT(*) FROM sessions");
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    int count = atoi(PQgetvalue(result, 0, 0));
    ck_assert_int_eq(count, 1);
    PQclear(result);

    // The teardown will rollback, and the next test will start fresh
    // This test just verifies we can create a session within a transaction
}
END_TEST

// ========== Suite Configuration ==========

static Suite *session_lifecycle_suite(void)
{
    Suite *s = suite_create("Session Lifecycle");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_session_create_returns_valid_id);
    tcase_add_test(tc_core, test_session_has_started_at);
    tcase_add_test(tc_core, test_session_ended_at_null_initially);
    tcase_add_test(tc_core, test_transaction_isolation);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = session_lifecycle_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
