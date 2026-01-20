/**
 * @file agent_zero_test.c
 * @brief Agent 0 registration tests
 *
 * Tests for ik_db_ensure_agent_zero() function that ensures Agent 0
 * (the root agent) exists in the registry on startup.
 */

#include "../../../src/db/agent.h"
#include "../../../src/db/agent_zero.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"
#include <check.h>
#include <libpq-fe.h>
#include <talloc.h>
#include <string.h>
#include <time.h>

// ========== Test Database Setup ==========

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

// Test: Creates Agent 0 on empty registry
START_TEST(test_ensure_agent_zero_creates_on_empty) {
    SKIP_IF_NO_DB();

    char *uuid = NULL;
    res_t res = ik_db_ensure_agent_zero(db, &uuid);
    if (is_err(&res)) {
        fprintf(stderr, "ERROR in test_ensure_agent_zero_creates_on_empty: %s\n", res.err->msg);
    }
    ck_assert(is_ok(&res));
    ck_assert(uuid != NULL);
    ck_assert_int_eq((int)strlen(uuid), 22);  // base64url UUID is 22 chars
}
END_TEST
// Test: Returns existing Agent 0 UUID if present
START_TEST(test_ensure_agent_zero_returns_existing) {
    SKIP_IF_NO_DB();

    // First call creates Agent 0
    char *uuid1 = NULL;
    res_t res1 = ik_db_ensure_agent_zero(db, &uuid1);
    ck_assert(is_ok(&res1));
    ck_assert(uuid1 != NULL);

    // Second call returns same UUID
    char *uuid2 = NULL;
    res_t res2 = ik_db_ensure_agent_zero(db, &uuid2);
    ck_assert(is_ok(&res2));
    ck_assert(uuid2 != NULL);
    ck_assert_str_eq(uuid1, uuid2);
}

END_TEST
// Test: Agent 0 has parent_uuid = NULL
START_TEST(test_agent_zero_has_null_parent) {
    SKIP_IF_NO_DB();

    char *uuid = NULL;
    res_t res = ik_db_ensure_agent_zero(db, &uuid);
    ck_assert(is_ok(&res));

    // Query to verify parent_uuid is NULL
    const char *query = "SELECT parent_uuid FROM agents WHERE uuid = $1";
    const char *param_values[1] = {uuid};

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert(PQgetisnull(result, 0, 0));  // parent_uuid is NULL

    PQclear(result);
}

END_TEST
// Test: Agent 0 has status = 'running'
START_TEST(test_agent_zero_status_running) {
    SKIP_IF_NO_DB();

    char *uuid = NULL;
    res_t res = ik_db_ensure_agent_zero(db, &uuid);
    ck_assert(is_ok(&res));

    // Query to verify status
    const char *query = "SELECT status::text FROM agents WHERE uuid = $1";
    const char *param_values[1] = {uuid};

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    const char *status = PQgetvalue(result, 0, 0);
    ck_assert_str_eq(status, "running");

    PQclear(result);
}

END_TEST
// Test: Upgrade scenario - adopts orphan messages
START_TEST(test_ensure_agent_zero_adopts_orphans) {
    SKIP_IF_NO_DB();

    // Check if agent_uuid column exists (pre-condition from messages-agent-uuid.md task)
    const char *check_column =
        "SELECT 1 FROM information_schema.columns "
        "WHERE table_name = 'messages' AND column_name = 'agent_uuid'";
    PGresult *column_res = PQexecParams(db->conn, check_column, 0, NULL, NULL, NULL, NULL, 0);
    bool has_agent_uuid = (PQresultStatus(column_res) == PGRES_TUPLES_OK && PQntuples(column_res) > 0);
    PQclear(column_res);

    // Skip test if agent_uuid column doesn't exist yet (pre-condition not met)
    if (!has_agent_uuid) {
        return;
    }

    // Create a session first (messages FK requires valid session_id)
    int64_t session_id = 0;
    res_t sess_res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&sess_res));
    ck_assert(session_id > 0);

    // Insert orphan messages (messages with no agent_uuid)
    char insert_orphan[512];
    snprintf(insert_orphan, sizeof(insert_orphan),
             "INSERT INTO messages (session_id, kind, content, created_at, agent_uuid) "
             "VALUES (%lld, 'user', 'orphan message 1', NOW(), NULL), "
             "       (%lld, 'assistant', 'orphan message 2', NOW(), NULL)",
             (long long)session_id, (long long)session_id);

    PGresult *orphan_res = PQexecParams(db->conn, insert_orphan, 0, NULL,
                                        NULL, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(orphan_res), PGRES_COMMAND_OK);
    PQclear(orphan_res);

    // Ensure Agent 0 (should adopt orphans)
    char *uuid = NULL;
    res_t res = ik_db_ensure_agent_zero(db, &uuid);
    ck_assert(is_ok(&res));
    ck_assert(uuid != NULL);

    // Verify orphan messages now have agent_uuid set
    const char *check_orphans = "SELECT COUNT(*) FROM messages WHERE agent_uuid IS NULL";
    PGresult *check_res = PQexecParams(db->conn, check_orphans, 0, NULL, NULL, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(check_res), PGRES_TUPLES_OK);

    const char *orphan_count = PQgetvalue(check_res, 0, 0);
    ck_assert_str_eq(orphan_count, "0");  // No orphans remain
    PQclear(check_res);

    // Verify messages are now owned by Agent 0
    const char *check_agent = "SELECT COUNT(*) FROM messages WHERE agent_uuid = $1";
    const char *agent_params[1] = {uuid};
    PGresult *agent_res = PQexecParams(db->conn, check_agent, 1, NULL,
                                       agent_params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(agent_res), PGRES_TUPLES_OK);

    const char *agent_count = PQgetvalue(agent_res, 0, 0);
    ck_assert_str_eq(agent_count, "2");  // Agent 0 owns both messages
    PQclear(agent_res);
}

END_TEST
// Test: Idempotent - multiple calls return same UUID
START_TEST(test_ensure_agent_zero_idempotent) {
    SKIP_IF_NO_DB();

    // Call three times
    char *uuid1 = NULL;
    res_t res1 = ik_db_ensure_agent_zero(db, &uuid1);
    ck_assert(is_ok(&res1));

    char *uuid2 = NULL;
    res_t res2 = ik_db_ensure_agent_zero(db, &uuid2);
    ck_assert(is_ok(&res2));

    char *uuid3 = NULL;
    res_t res3 = ik_db_ensure_agent_zero(db, &uuid3);
    ck_assert(is_ok(&res3));

    // All should return same UUID
    ck_assert_str_eq(uuid1, uuid2);
    ck_assert_str_eq(uuid2, uuid3);

    // Verify only one root agent exists
    const char *count_query = "SELECT COUNT(*) FROM agents WHERE parent_uuid IS NULL";
    PGresult *result = PQexecParams(db->conn, count_query, 0, NULL, NULL, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);

    const char *count = PQgetvalue(result, 0, 0);
    ck_assert_str_eq(count, "1");  // Only one root agent
    PQclear(result);
}

END_TEST

// ========== Suite Configuration ==========

static Suite *agent_zero_suite(void)
{
    Suite *s = suite_create("Agent 0 Registration");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_ensure_agent_zero_creates_on_empty);
    tcase_add_test(tc_core, test_ensure_agent_zero_returns_existing);
    tcase_add_test(tc_core, test_agent_zero_has_null_parent);
    tcase_add_test(tc_core, test_agent_zero_status_running);
    tcase_add_test(tc_core, test_ensure_agent_zero_adopts_orphans);
    tcase_add_test(tc_core, test_ensure_agent_zero_idempotent);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_zero_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
