/**
 * @file agent_idle_test.c
 * @brief Tests for ik_db_agent_set_idle and thinking_level default branch
 *
 * Covers:
 * - ik_db_agent_set_idle (set idle true/false and verify via ik_db_agent_get)
 * - thinking_level default branch (value 99 maps to "none")
 * - agent_row.c idle field parsing (row->idle bool from DB text)
 */

#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/agent.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"
#include "tests/test_constants.h"
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

// ========== Helper: Insert agent ==========

static void insert_test_agent(const char *uuid, int thinking_level)
{
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, uuid);
    agent.name = NULL;
    agent.parent_uuid = NULL;
    agent.provider = NULL;
    agent.model = NULL;
    agent.thinking_level = thinking_level;
    agent.created_at = time(NULL);
    agent.fork_message_id = 0;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert_msg(is_ok(&res), "Failed to insert test agent");
}

// ========== Tests ==========

// Test: set idle to true and verify
START_TEST(test_set_idle_true) {
    SKIP_IF_NO_DB();

    insert_test_agent("test-idle-uuid-1", 0);

    // Set idle to true
    res_t res = ik_db_agent_set_idle(db, "test-idle-uuid-1", true);
    ck_assert(is_ok(&res));

    // Verify via ik_db_agent_get
    ik_db_agent_row_t *row = NULL;
    res = ik_db_agent_get(db, test_ctx, "test-idle-uuid-1", &row);
    ck_assert(is_ok(&res));
    ck_assert(row != NULL);
    ck_assert(row->idle == true);
}
END_TEST

// Test: set idle to false and verify
START_TEST(test_set_idle_false) {
    SKIP_IF_NO_DB();

    insert_test_agent("test-idle-uuid-2", 0);

    // First set to true
    res_t res = ik_db_agent_set_idle(db, "test-idle-uuid-2", true);
    ck_assert(is_ok(&res));

    // Then set back to false
    res = ik_db_agent_set_idle(db, "test-idle-uuid-2", false);
    ck_assert(is_ok(&res));

    // Verify it flipped back
    ik_db_agent_row_t *row = NULL;
    res = ik_db_agent_get(db, test_ctx, "test-idle-uuid-2", &row);
    ck_assert(is_ok(&res));
    ck_assert(row != NULL);
    ck_assert(row->idle == false);
}
END_TEST

// Test: default idle value is false for newly inserted agent
START_TEST(test_idle_default_false) {
    SKIP_IF_NO_DB();

    insert_test_agent("test-idle-uuid-3", 0);

    // New agent's idle should default to false (but migration 006 sets running agents
    // to idle=true on migration, so within a transaction after insert, the column
    // default is false)
    ik_db_agent_row_t *row = NULL;
    res_t res = ik_db_agent_get(db, test_ctx, "test-idle-uuid-3", &row);
    ck_assert(is_ok(&res));
    ck_assert(row != NULL);
    ck_assert(row->idle == false);
}
END_TEST

// Test: thinking_level default branch (value 99 maps to "none")
START_TEST(test_thinking_level_default_branch) {
    SKIP_IF_NO_DB();

    // Insert agent with thinking_level = 99 to hit the default case
    insert_test_agent("test-thinking-default", 99);

    // Verify agent was inserted and thinking_level stored as "none"
    ik_db_agent_row_t *row = NULL;
    res_t res = ik_db_agent_get(db, test_ctx, "test-thinking-default", &row);
    ck_assert(is_ok(&res));
    ck_assert(row != NULL);
    ck_assert(row->thinking_level != NULL);
    ck_assert_str_eq(row->thinking_level, "none");
}
END_TEST

// Test: thinking_level known values (low, med, high)
START_TEST(test_thinking_level_low) {
    SKIP_IF_NO_DB();

    insert_test_agent("test-thinking-low", 1);

    ik_db_agent_row_t *row = NULL;
    res_t res = ik_db_agent_get(db, test_ctx, "test-thinking-low", &row);
    ck_assert(is_ok(&res));
    ck_assert(row != NULL);
    ck_assert(row->thinking_level != NULL);
    ck_assert_str_eq(row->thinking_level, "low");
}
END_TEST

START_TEST(test_thinking_level_med) {
    SKIP_IF_NO_DB();

    insert_test_agent("test-thinking-med", 2);

    ik_db_agent_row_t *row = NULL;
    res_t res = ik_db_agent_get(db, test_ctx, "test-thinking-med", &row);
    ck_assert(is_ok(&res));
    ck_assert(row != NULL);
    ck_assert(row->thinking_level != NULL);
    ck_assert_str_eq(row->thinking_level, "med");
}
END_TEST

START_TEST(test_thinking_level_high) {
    SKIP_IF_NO_DB();

    insert_test_agent("test-thinking-high", 3);

    ik_db_agent_row_t *row = NULL;
    res_t res = ik_db_agent_get(db, test_ctx, "test-thinking-high", &row);
    ck_assert(is_ok(&res));
    ck_assert(row != NULL);
    ck_assert(row->thinking_level != NULL);
    ck_assert_str_eq(row->thinking_level, "high");
}
END_TEST

// Test: thinking_level = 0 means NULL (no thinking level)
START_TEST(test_thinking_level_zero_is_null) {
    SKIP_IF_NO_DB();

    insert_test_agent("test-thinking-zero", 0);

    ik_db_agent_row_t *row = NULL;
    res_t res = ik_db_agent_get(db, test_ctx, "test-thinking-zero", &row);
    ck_assert(is_ok(&res));
    ck_assert(row != NULL);
    ck_assert(row->thinking_level == NULL);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *agent_idle_suite(void)
{
    Suite *s = suite_create("Agent Idle and Thinking Level");

    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_unchecked_fixture(tc, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc, test_setup, test_teardown);

    tcase_add_test(tc, test_set_idle_true);
    tcase_add_test(tc, test_set_idle_false);
    tcase_add_test(tc, test_idle_default_false);
    tcase_add_test(tc, test_thinking_level_default_branch);
    tcase_add_test(tc, test_thinking_level_low);
    tcase_add_test(tc, test_thinking_level_med);
    tcase_add_test(tc, test_thinking_level_high);
    tcase_add_test(tc, test_thinking_level_zero_is_null);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = agent_idle_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/agent_idle_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
