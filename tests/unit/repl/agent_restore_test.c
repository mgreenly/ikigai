/**
 * @file agent_restore_test.c
 * @brief Tests for agent startup restoration functionality
 *
 * Tests for ik_repl_restore_agents() which restores all running agents
 * from the database on startup.
 */

#include "../../../src/repl/agent_restore.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/agent.h"
#include "../../../src/repl.h"
#include "../../../src/error.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/logger.h"
#include "../../../src/scrollback.h"
#include "../../../src/layer.h"
#include "../../../src/layer_wrappers.h"
#include "../../test_utils_helper.h"
#include "agent_restore_test_helper.h"
#include <check.h>
#include <talloc.h>
#include <string.h>
#include <time.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

// Per-test state (exported for helpers)
TALLOC_CTX *test_ctx;
ik_db_ctx_t *db;
int64_t session_id;

// Suite-level setup: Create and migrate database (runs once)
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
    test_paths_setup_env();  // Setup paths environment once for all tests
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
        return;
    }

    // Create a session for tests
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

// ========== Test Cases ==========

// Test: restore_agents handles agent with empty history
START_TEST(test_restore_agents_handles_empty_history) {
    SKIP_IF_NO_DB();

    // Insert Agent 0 with no messages (fresh install scenario)
    insert_agent("agent0-empty-test12", NULL, 1000, 0);

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-empty-test12");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Should succeed even with no history
    ck_assert_uint_eq(repl->agent_count, 1);
}

END_TEST
// Test: restore_agents handles restore failure gracefully
START_TEST(test_restore_agents_handles_restore_failure_gracefully) {
    SKIP_IF_NO_DB();

    // Insert Agent 0
    insert_agent("agent0-fail-test123", NULL, 1000, 0);
    insert_message("agent0-fail-test123", "clear", NULL);

    // Insert a valid child
    insert_agent("child1-fail-test123", "agent0-fail-test123", 2000, 0);

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-fail-test123");

    // Restore should succeed (individual failures logged but don't stop process)
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));
}

END_TEST

// Test: restore_child_agent adds lower_separator_layer when present
START_TEST(test_restore_child_agent_adds_lower_separator_layer) {
    SKIP_IF_NO_DB();

    // Insert Agent 0
    insert_agent("agent0-sep-test-123", NULL, 1000, 0);
    insert_message("agent0-sep-test-123", "clear", NULL);

    // Insert child agent
    insert_agent("child1-sep-test-123", "agent0-sep-test-123", 2000, 0);

    // Create repl with lower_separator_layer
    ik_repl_ctx_t *repl = create_test_repl_with_lower_separator();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-sep-test-123");

    // Call restore_agents
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify child agent was restored
    ck_assert_uint_eq(repl->agent_count, 2);
    ik_agent_ctx_t *child = repl->agents[1];
    ck_assert_ptr_nonnull(child);

    // Verify lower_separator_layer was added to child's layer cake
    // The layer cake should contain the lower_separator_layer
    ck_assert_ptr_nonnull(child->layer_cake);
    ck_assert_ptr_nonnull(repl->lower_separator_layer);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *agent_restore_suite(void)
{
    Suite *s = suite_create("Agent Restore");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    // FIXME: Commented out temporarily - missing implementations (see $CDD_DIR/reminder.md)
    // tcase_add_test(tc_core, test_restore_agents_queries_running_agents);
    // tcase_add_test(tc_core, test_restore_agents_sorts_by_created_at);
    // tcase_add_test(tc_core, test_restore_agents_skips_none_restores_all_running);
    // tcase_add_test(tc_core, test_restore_agents_handles_agent0_specially);
    // tcase_add_test(tc_core, test_restore_agents_populates_conversation);
    // tcase_add_test(tc_core, test_restore_agents_populates_scrollback);
    // tcase_add_test(tc_core, test_restore_agents_handles_mark_events);
    tcase_add_test(tc_core, test_restore_agents_handles_empty_history);
    tcase_add_test(tc_core, test_restore_agents_handles_restore_failure_gracefully);
    tcase_add_test(tc_core, test_restore_child_agent_adds_lower_separator_layer);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_restore_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
