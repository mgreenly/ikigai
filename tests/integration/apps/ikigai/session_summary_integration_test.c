/**
 * @file session_summary_integration_test.c
 * @brief Integration tests for session_summary database operations
 *
 * Tests insert with cap enforcement, load ordering, and unique constraint.
 * Uses per-file database isolation for parallel test execution.
 */

#include "apps/ikigai/db/session_summary.h"
#include "apps/ikigai/db/connection.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;

// A real agent UUID inserted before each test
static const char *TEST_AGENT_UUID = "test-agent-uuid-001";

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

    // Insert a test agent so FK constraints are satisfied
    const char *agent_sql =
        "INSERT INTO agents (uuid, name, status, created_at) "
        "VALUES ($1, 'test-agent', 'running', 0)";
    const char *agent_params[1] = { TEST_AGENT_UUID };
    PGresult *agent_res = PQexecParams(db->conn, agent_sql, 1, NULL,
                                       agent_params, NULL, NULL, 0);
    if (PQresultStatus(agent_res) != PGRES_COMMAND_OK) {
        PQclear(agent_res);
        ik_test_db_rollback(db);
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        return;
    }
    PQclear(agent_res);
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

#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// ========== Tests ==========

// Test: insert 6 summaries for same agent, verify only 5 remain (oldest deleted)
START_TEST(test_insert_cap_enforcement) {
    SKIP_IF_NO_DB();

    // Insert 6 summaries
    for (int i = 1; i <= 6; i++) {
        char summary_text[64];
        snprintf(summary_text, sizeof(summary_text), "Summary number %d", i);

        res_t res = ik_db_session_summary_insert(
            db, TEST_AGENT_UUID,
            summary_text,
            (int64_t)(i * 10),
            (int64_t)(i * 10 + 9),
            100 + i);
        ck_assert_msg(is_ok(&res), "Insert %d failed", i);
    }

    // Load and verify only 5 remain
    ik_session_summary_t **summaries = NULL;
    size_t count = 0;
    res_t res = ik_db_session_summary_load(db, test_ctx, TEST_AGENT_UUID,
                                           &summaries, &count);
    ck_assert_msg(is_ok(&res), "Load failed after cap enforcement");
    ck_assert_int_eq((int)count, 5);

    // Verify the oldest (summary 1) was deleted; summaries 2-6 remain
    // Load is ordered oldest-first, so summaries[0] should be summary #2
    ck_assert_str_eq(summaries[0]->summary, "Summary number 2");
    ck_assert_str_eq(summaries[4]->summary, "Summary number 6");
}
END_TEST

// Test: insert summaries, load, verify order (oldest-first) and content
START_TEST(test_load_order_and_content) {
    SKIP_IF_NO_DB();

    res_t res;

    res = ik_db_session_summary_insert(
        db, TEST_AGENT_UUID, "First epoch summary", 1, 10, 50);
    ck_assert(is_ok(&res));

    res = ik_db_session_summary_insert(
        db, TEST_AGENT_UUID, "Second epoch summary", 11, 20, 75);
    ck_assert(is_ok(&res));

    res = ik_db_session_summary_insert(
        db, TEST_AGENT_UUID, "Third epoch summary", 21, 30, 90);
    ck_assert(is_ok(&res));

    ik_session_summary_t **summaries = NULL;
    size_t count = 0;
    res = ik_db_session_summary_load(db, test_ctx, TEST_AGENT_UUID,
                                     &summaries, &count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)count, 3);

    // Verify oldest-first order
    ck_assert_str_eq(summaries[0]->summary, "First epoch summary");
    ck_assert_int_eq(summaries[0]->start_msg_id, 1);
    ck_assert_int_eq(summaries[0]->end_msg_id, 10);
    ck_assert_int_eq(summaries[0]->token_count, 50);

    ck_assert_str_eq(summaries[1]->summary, "Second epoch summary");
    ck_assert_int_eq(summaries[1]->start_msg_id, 11);
    ck_assert_int_eq(summaries[1]->token_count, 75);

    ck_assert_str_eq(summaries[2]->summary, "Third epoch summary");
    ck_assert_int_eq(summaries[2]->token_count, 90);
}
END_TEST

// Test: load returns empty when no summaries exist
START_TEST(test_load_empty) {
    SKIP_IF_NO_DB();

    ik_session_summary_t **summaries = NULL;
    size_t count = 0;
    res_t res = ik_db_session_summary_load(db, test_ctx, TEST_AGENT_UUID,
                                           &summaries, &count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)count, 0);
    ck_assert_ptr_null(summaries);
}
END_TEST

// Test: unique constraint rejects duplicate epoch
START_TEST(test_unique_constraint_rejects_duplicate) {
    SKIP_IF_NO_DB();

    res_t res = ik_db_session_summary_insert(
        db, TEST_AGENT_UUID, "Epoch summary", 1, 10, 50);
    ck_assert(is_ok(&res));

    // Attempt to insert the same epoch again — must fail
    res_t res2 = ik_db_session_summary_insert(
        db, TEST_AGENT_UUID, "Duplicate epoch summary", 1, 10, 60);
    ck_assert_msg(is_err(&res2), "Expected unique constraint violation");
}
END_TEST

static Suite *session_summary_integration_suite(void)
{
    Suite *s = suite_create("Session Summary Integration");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_insert_cap_enforcement);
    tcase_add_test(tc_core, test_load_order_and_content);
    tcase_add_test(tc_core, test_load_empty);
    tcase_add_test(tc_core, test_unique_constraint_rejects_duplicate);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = session_summary_integration_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
                    "reports/check/integration/apps/ikigai/session_summary_integration_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
