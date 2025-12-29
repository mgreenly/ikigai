/**
 * @file agent_thinking_level_test.c
 * @brief Tests for agent thinking_level handling and update_provider
 *
 * Tests coverage gaps in src/db/agent.c:
 * - Line 45-49: thinking_level switch cases (low, default)
 * - Line 408-411: update_provider error handling
 */

#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../../src/agent.h"
#include "../../../src/wrapper.h"
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

// ========== Thinking Level Tests ==========

// Test: Insert agent with thinking_level = 1 (low)
START_TEST(test_insert_agent_thinking_level_low) {
    SKIP_IF_NO_DB();

    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "thinking-low-uuid");
    agent.name = talloc_strdup(test_ctx, "Thinking Low Agent");
    agent.parent_uuid = NULL;
    agent.created_at = time(NULL);
    agent.fork_message_id = 0;
    agent.thinking_level = 1;  // Low

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));

    // Verify thinking_level in database
    const char *query = "SELECT thinking_level FROM agents WHERE uuid = $1";
    const char *param_values[1] = {agent.uuid};

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    const char *thinking_level = PQgetvalue(result, 0, 0);
    ck_assert_str_eq(thinking_level, "low");

    PQclear(result);
}
END_TEST
// Test: Insert agent with thinking_level = 99 (default/invalid -> "none")
START_TEST(test_insert_agent_thinking_level_default)
{
    SKIP_IF_NO_DB();

    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "thinking-default-uuid");
    agent.name = talloc_strdup(test_ctx, "Thinking Default Agent");
    agent.parent_uuid = NULL;
    agent.created_at = time(NULL);
    agent.fork_message_id = 0;
    agent.thinking_level = 99;  // Invalid value -> default case

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));

    // Verify thinking_level in database (should be "none")
    const char *query = "SELECT thinking_level FROM agents WHERE uuid = $1";
    const char *param_values[1] = {agent.uuid};

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    const char *thinking_level = PQgetvalue(result, 0, 0);
    ck_assert_str_eq(thinking_level, "none");

    PQclear(result);
}

END_TEST
// ========== Update Provider Tests ==========

// Test: update_provider succeeds with valid data
START_TEST(test_update_provider_success)
{
    SKIP_IF_NO_DB();

    // Insert an agent first
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "update-provider-uuid");
    agent.name = talloc_strdup(test_ctx, "Update Provider Agent");
    agent.parent_uuid = NULL;
    agent.created_at = time(NULL);
    agent.fork_message_id = 0;

    res_t insert_res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&insert_res));

    // Update provider
    res_t update_res = ik_db_agent_update_provider(db, agent.uuid,
                                                   "anthropic", "claude-3-5-sonnet", "med");
    ck_assert(is_ok(&update_res));

    // Verify the update
    const char *query = "SELECT provider, model, thinking_level FROM agents WHERE uuid = $1";
    const char *param_values[1] = {agent.uuid};

    PGresult *result = PQexecParams(db->conn, query, 1, NULL, param_values, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(result), 1);

    const char *provider = PQgetvalue(result, 0, 0);
    const char *model = PQgetvalue(result, 0, 1);
    const char *thinking_level = PQgetvalue(result, 0, 2);

    ck_assert_str_eq(provider, "anthropic");
    ck_assert_str_eq(model, "claude-3-5-sonnet");
    ck_assert_str_eq(thinking_level, "med");

    PQclear(result);
}

END_TEST
// Test: update_provider on nonexistent agent succeeds (0 rows affected)
START_TEST(test_update_provider_nonexistent)
{
    SKIP_IF_NO_DB();

    // Try to update a nonexistent agent (should succeed per spec)
    res_t update_res = ik_db_agent_update_provider(db, "nonexistent-uuid-12345",
                                                   "provider", "model", "low");
    ck_assert(is_ok(&update_res));
}

END_TEST

// ========== Suite Configuration ==========

static Suite *agent_thinking_level_suite(void)
{
    Suite *s = suite_create("Agent Thinking Level");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    // Thinking level tests
    tcase_add_test(tc_core, test_insert_agent_thinking_level_low);
    tcase_add_test(tc_core, test_insert_agent_thinking_level_default);

    // Update provider tests
    tcase_add_test(tc_core, test_update_provider_success);
    tcase_add_test(tc_core, test_update_provider_nonexistent);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_thinking_level_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
