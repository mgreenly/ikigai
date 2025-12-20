/**
 * @file agent_registry_test.c
 * @brief Agent registry database tests for insert and mark_dead operations
 *
 * Tests for ik_db_agent_insert() and ik_db_agent_mark_dead() functions.
 * Query tests (get, list_running, etc.) are in agent_registry_queries_test.c.
 * Follows the unified test utilities pattern for database tests.
 */

#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../../src/agent.h"
#include "../../test_utils.h"
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

// ========== Insert Tests ==========

// Test: Insert root agent (parent_uuid = NULL) succeeds
START_TEST(test_insert_root_agent_success) {
    SKIP_IF_NO_DB();

    // Create minimal agent context for testing
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "test-root-uuid-123456");
    agent.name = talloc_strdup(test_ctx, "Root Agent");
    agent.parent_uuid = NULL;  // Root agent has no parent
    agent.created_at = time(NULL);
    agent.fork_message_id = 0;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));
}
END_TEST
// Test: Insert child agent (parent_uuid set) succeeds
START_TEST(test_insert_child_agent_success)
{
    SKIP_IF_NO_DB();

    // First insert parent
    ik_agent_ctx_t parent = {0};
    parent.uuid = talloc_strdup(test_ctx, "parent-uuid-123456789");
    parent.name = talloc_strdup(test_ctx, "Parent Agent");
    parent.parent_uuid = NULL;
    parent.created_at = time(NULL);
    parent.fork_message_id = 0;

    res_t parent_res = ik_db_agent_insert(db, &parent);
    ck_assert(is_ok(&parent_res));

    // Then insert child
    ik_agent_ctx_t child = {0};
    child.uuid = talloc_strdup(test_ctx, "child-uuid-987654321");
    child.name = talloc_strdup(test_ctx, "Child Agent");
    child.parent_uuid = talloc_strdup(test_ctx, "parent-uuid-123456789");
    child.created_at = time(NULL);
    child.fork_message_id = 42;

    res_t child_res = ik_db_agent_insert(db, &child);
    ck_assert(is_ok(&child_res));
}

END_TEST
// Test: Inserted record has status = 'running'
START_TEST(test_insert_agent_status_running)
{
    SKIP_IF_NO_DB();

    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "status-test-uuid");
    agent.name = talloc_strdup(test_ctx, "Status Test Agent");
    agent.parent_uuid = NULL;
    agent.created_at = time(NULL);
    agent.fork_message_id = 0;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));

    // Query to verify status
    const char *query = "SELECT status::text FROM agents WHERE uuid = $1";
    const char *param_values[1] = {agent.uuid};

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    const char *status = PQgetvalue(result, 0, 0);
    ck_assert_str_eq(status, "running");

    PQclear(result);
}

END_TEST
// Test: Inserted record has correct created_at
START_TEST(test_insert_agent_created_at)
{
    SKIP_IF_NO_DB();

    int64_t expected_timestamp = time(NULL);

    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "created-at-test-uuid");
    agent.name = talloc_strdup(test_ctx, "Created At Test");
    agent.parent_uuid = NULL;
    agent.created_at = expected_timestamp;
    agent.fork_message_id = 0;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));

    // Query to verify created_at
    const char *query = "SELECT created_at FROM agents WHERE uuid = $1";
    const char *param_values[1] = {agent.uuid};

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    const char *created_at_str = PQgetvalue(result, 0, 0);
    int64_t actual_timestamp = strtoll(created_at_str, NULL, 10);

    // Timestamps should match (within 1 second tolerance)
    ck_assert_int_ge(actual_timestamp, expected_timestamp - 1);
    ck_assert_int_le(actual_timestamp, expected_timestamp + 1);

    PQclear(result);
}

END_TEST
// Test: Duplicate uuid fails (PRIMARY KEY violation)
START_TEST(test_insert_duplicate_uuid_fails)
{
    SKIP_IF_NO_DB();

    ik_agent_ctx_t agent1 = {0};
    agent1.uuid = talloc_strdup(test_ctx, "duplicate-uuid-test");
    agent1.name = talloc_strdup(test_ctx, "First Agent");
    agent1.parent_uuid = NULL;
    agent1.created_at = time(NULL);
    agent1.fork_message_id = 0;

    res_t res1 = ik_db_agent_insert(db, &agent1);
    ck_assert(is_ok(&res1));

    // Try to insert another agent with same uuid
    ik_agent_ctx_t agent2 = {0};
    agent2.uuid = talloc_strdup(test_ctx, "duplicate-uuid-test");  // Same UUID
    agent2.name = talloc_strdup(test_ctx, "Second Agent");
    agent2.parent_uuid = NULL;
    agent2.created_at = time(NULL);
    agent2.fork_message_id = 0;

    res_t res2 = ik_db_agent_insert(db, &agent2);
    ck_assert(is_err(&res2));
}

END_TEST
// Test: Agent with NULL name succeeds (name is optional)
START_TEST(test_insert_agent_null_name)
{
    SKIP_IF_NO_DB();

    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "null-name-uuid");
    agent.name = NULL;  // NULL name is allowed
    agent.parent_uuid = NULL;
    agent.created_at = time(NULL);
    agent.fork_message_id = 0;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));

    // Verify name is NULL in database
    const char *query = "SELECT name FROM agents WHERE uuid = $1";
    const char *param_values[1] = {agent.uuid};

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);
    ck_assert(PQgetisnull(result, 0, 0));

    PQclear(result);
}

END_TEST
// Test: fork_message_id is correctly stored
START_TEST(test_insert_agent_fork_message_id)
{
    SKIP_IF_NO_DB();

    // Insert parent first
    ik_agent_ctx_t parent = {0};
    parent.uuid = talloc_strdup(test_ctx, "parent-fork-test");
    parent.name = NULL;
    parent.parent_uuid = NULL;
    parent.created_at = time(NULL);
    parent.fork_message_id = 0;

    res_t parent_res = ik_db_agent_insert(db, &parent);
    ck_assert(is_ok(&parent_res));

    // Insert child with specific fork_message_id
    ik_agent_ctx_t child = {0};
    child.uuid = talloc_strdup(test_ctx, "child-fork-test");
    child.name = NULL;
    child.parent_uuid = talloc_strdup(test_ctx, "parent-fork-test");
    child.created_at = time(NULL);
    child.fork_message_id = 123456;

    res_t child_res = ik_db_agent_insert(db, &child);
    ck_assert(is_ok(&child_res));

    // Verify fork_message_id
    const char *query = "SELECT fork_message_id FROM agents WHERE uuid = $1";
    const char *param_values[1] = {child.uuid};

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    const char *fork_id_str = PQgetvalue(result, 0, 0);
    int64_t fork_id = strtoll(fork_id_str, NULL, 10);
    ck_assert_int_eq(fork_id, 123456);

    PQclear(result);
}

END_TEST
// ========== Mark Dead Tests ==========

// Test: mark_dead updates status to 'dead'
START_TEST(test_mark_dead_updates_status)
{
    SKIP_IF_NO_DB();

    // First insert an agent with status='running'
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "mark-dead-status-test");
    agent.name = talloc_strdup(test_ctx, "Status Test Agent");
    agent.parent_uuid = NULL;
    agent.created_at = time(NULL);
    agent.fork_message_id = 0;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));

    // Mark the agent as dead
    res_t mark_res = ik_db_agent_mark_dead(db, agent.uuid);
    ck_assert(is_ok(&mark_res));

    // Verify status is now 'dead'
    const char *query = "SELECT status::text FROM agents WHERE uuid = $1";
    const char *param_values[1] = {agent.uuid};

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    const char *status = PQgetvalue(result, 0, 0);
    ck_assert_str_eq(status, "dead");

    PQclear(result);
}

END_TEST
// Test: mark_dead sets ended_at timestamp
START_TEST(test_mark_dead_sets_ended_at)
{
    SKIP_IF_NO_DB();

    // Insert an agent
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "mark-dead-ended-at-test");
    agent.name = talloc_strdup(test_ctx, "Ended At Test Agent");
    agent.parent_uuid = NULL;
    agent.created_at = time(NULL);
    agent.fork_message_id = 0;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));

    // Record time before marking dead
    int64_t before_time = time(NULL);

    // Mark the agent as dead
    res_t mark_res = ik_db_agent_mark_dead(db, agent.uuid);
    ck_assert(is_ok(&mark_res));

    // Record time after marking dead
    int64_t after_time = time(NULL);

    // Verify ended_at is set and within reasonable range
    const char *query = "SELECT ended_at FROM agents WHERE uuid = $1";
    const char *param_values[1] = {agent.uuid};

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    // ended_at should not be NULL
    ck_assert(!PQgetisnull(result, 0, 0));

    const char *ended_at_str = PQgetvalue(result, 0, 0);
    int64_t ended_at = strtoll(ended_at_str, NULL, 10);

    // ended_at should be within the time range
    ck_assert_int_ge(ended_at, before_time);
    ck_assert_int_le(ended_at, after_time);

    PQclear(result);
}

END_TEST
// Test: mark_dead on already-dead agent is no-op (idempotent)
START_TEST(test_mark_dead_idempotent)
{
    SKIP_IF_NO_DB();

    // Insert an agent
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "mark-dead-idempotent-test");
    agent.name = talloc_strdup(test_ctx, "Idempotent Test Agent");
    agent.parent_uuid = NULL;
    agent.created_at = time(NULL);
    agent.fork_message_id = 0;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));

    // Mark the agent as dead first time
    res_t mark_res1 = ik_db_agent_mark_dead(db, agent.uuid);
    ck_assert(is_ok(&mark_res1));

    // Get the ended_at timestamp after first mark
    const char *query = "SELECT ended_at FROM agents WHERE uuid = $1";
    const char *param_values[1] = {agent.uuid};

    PGresult *result1 = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result1), PGRES_TUPLES_OK);
    const char *ended_at_str1 = PQgetvalue(result1, 0, 0);
    int64_t ended_at_1 = strtoll(ended_at_str1, NULL, 10);
    PQclear(result1);

    // Mark the agent as dead second time (should be idempotent)
    res_t mark_res2 = ik_db_agent_mark_dead(db, agent.uuid);
    ck_assert(is_ok(&mark_res2));

    // Get the ended_at timestamp after second mark - should be unchanged
    PGresult *result2 = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result2), PGRES_TUPLES_OK);
    const char *ended_at_str2 = PQgetvalue(result2, 0, 0);
    int64_t ended_at_2 = strtoll(ended_at_str2, NULL, 10);
    PQclear(result2);

    // ended_at should remain unchanged
    ck_assert_int_eq(ended_at_1, ended_at_2);

    // Status should still be 'dead'
    const char *status_query = "SELECT status::text FROM agents WHERE uuid = $1";
    PGresult *result3 = PQexecParams(db->conn, status_query, 1, NULL, param_values, NULL, NULL, 0);
    const char *status = PQgetvalue(result3, 0, 0);
    ck_assert_str_eq(status, "dead");
    PQclear(result3);
}

END_TEST
// Test: mark_dead on non-existent uuid returns error
START_TEST(test_mark_dead_nonexistent_uuid)
{
    SKIP_IF_NO_DB();

    // Try to mark a non-existent agent as dead
    res_t mark_res = ik_db_agent_mark_dead(db, "nonexistent-uuid-12345");

    // Should succeed (0 rows affected is not an error, just a no-op)
    ck_assert(is_ok(&mark_res));
}

END_TEST

// ========== Suite Configuration ==========

static Suite *agent_registry_suite(void)
{
    Suite *s = suite_create("Agent Registry");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    // Insert tests
    tcase_add_test(tc_core, test_insert_root_agent_success);
    tcase_add_test(tc_core, test_insert_child_agent_success);
    tcase_add_test(tc_core, test_insert_agent_status_running);
    tcase_add_test(tc_core, test_insert_agent_created_at);
    tcase_add_test(tc_core, test_insert_duplicate_uuid_fails);
    tcase_add_test(tc_core, test_insert_agent_null_name);
    tcase_add_test(tc_core, test_insert_agent_fork_message_id);

    // Mark dead tests
    tcase_add_test(tc_core, test_mark_dead_updates_status);
    tcase_add_test(tc_core, test_mark_dead_sets_ended_at);
    tcase_add_test(tc_core, test_mark_dead_idempotent);
    tcase_add_test(tc_core, test_mark_dead_nonexistent_uuid);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_registry_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
