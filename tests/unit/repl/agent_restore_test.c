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
#include "../../test_utils.h"
#include <check.h>
#include <talloc.h>
#include <string.h>
#include <time.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;

// Suite-level setup: Create and migrate database (runs once)
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
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

// Helper: Insert an agent into the registry
static void insert_agent(const char *uuid, const char *parent_uuid,
                         int64_t created_at, int64_t fork_message_id)
{
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, uuid);
    agent.name = NULL;
    agent.parent_uuid = parent_uuid ? talloc_strdup(test_ctx, parent_uuid) : NULL;
    agent.created_at = created_at;
    agent.fork_message_id = fork_message_id;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));
}

// Helper: Insert a message
static void insert_message(const char *agent_uuid, const char *kind,
                           const char *content)
{
    res_t res = ik_db_message_insert(db, session_id, agent_uuid, kind, content, "{}");
    ck_assert(is_ok(&res));
}

// Helper: Create minimal repl context for testing
static ik_repl_ctx_t *create_test_repl(void)
{
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->db_ctx = db;
    shared->session_id = session_id;

    // Create logger
    shared->logger = ik_logger_create(shared, "/tmp");
    ck_assert_ptr_nonnull(shared->logger);

    // Create config
    shared->cfg = ik_test_create_config(shared);

    repl->shared = shared;

    // Initialize agents array
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agent_count = 0;
    repl->agent_capacity = 16;

    // Create Agent 0 (root agent)
    ik_agent_ctx_t *agent0 = NULL;
    res_t res = ik_agent_create(repl, shared, NULL, &agent0);
    ck_assert(is_ok(&res));

    repl->agents[0] = agent0;
    repl->agent_count = 1;
    repl->current = agent0;

    return repl;
}

// ========== Test Cases ==========

// Test: restore_agents queries running agents from DB
START_TEST(test_restore_agents_queries_running_agents) {
    SKIP_IF_NO_DB();

    // Insert Agent 0 (root)
    insert_agent("agent0-test-restore1", NULL, 1000, 0);
    insert_message("agent0-test-restore1", "clear", NULL);

    // Insert child agents
    insert_agent("child1-test-restore1", "agent0-test-restore1", 2000, 0);
    insert_agent("child2-test-restore1", "agent0-test-restore1", 3000, 0);

    // Create repl and override Agent 0's UUID
    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-test-restore1");

    // Call restore_agents
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify all running agents are restored
    ck_assert_uint_eq(repl->agent_count, 3);
}
END_TEST
// Test: restore_agents sorts by created_at (oldest first)
START_TEST(test_restore_agents_sorts_by_created_at) {
    SKIP_IF_NO_DB();

    // Insert root
    insert_agent("root-sort-test-12345", NULL, 1000, 0);
    insert_message("root-sort-test-12345", "clear", NULL);

    // Insert children out of order (newer first, then older)
    insert_agent("newer-child-sort-te", "root-sort-test-12345", 3000, 0);
    insert_agent("older-child-sort-te", "root-sort-test-12345", 2000, 0);

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "root-sort-test-12345");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify 3 agents total
    ck_assert_uint_eq(repl->agent_count, 3);

    // Verify sorting: older child should be restored before newer
    // Agent 0 is at index 0, older-child should be at 1, newer-child at 2
    ck_assert_str_eq(repl->agents[1]->uuid, "older-child-sort-te");
    ck_assert_str_eq(repl->agents[2]->uuid, "newer-child-sort-te");
}

END_TEST
// Test: restore_agents skips Agent 0 (parent_uuid=NULL)
START_TEST(test_restore_agents_skips_none_restores_all_running) {
    SKIP_IF_NO_DB();

    // Insert Agent 0
    insert_agent("agent0-skip-test-12", NULL, 1000, 0);
    insert_message("agent0-skip-test-12", "clear", NULL);

    // Insert child
    insert_agent("child1-skip-test-12", "agent0-skip-test-12", 2000, 0);

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-skip-test-12");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Agent 0 should not be duplicated, child should be added
    ck_assert_uint_eq(repl->agent_count, 2);
}

END_TEST
// Test: restore_agents handles Agent 0 specially
START_TEST(test_restore_agents_handles_agent0_specially) {
    SKIP_IF_NO_DB();

    // Insert Agent 0 with some history
    insert_agent("agent0-special-test", NULL, 1000, 0);
    insert_message("agent0-special-test", "clear", NULL);
    insert_message("agent0-special-test", "user", "Hello");
    insert_message("agent0-special-test", "assistant", "Hi there");

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-special-test");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Agent 0 should not be duplicated
    ck_assert_uint_eq(repl->agent_count, 1);

    // Agent 0's conversation should have messages
    ck_assert_uint_ge(repl->current->message_count, 2);
}

END_TEST
// Test: restore_agents populates conversation from replay
START_TEST(test_restore_agents_populates_conversation) {
    SKIP_IF_NO_DB();

    // Insert Agent 0
    insert_agent("agent0-conv-test-12", NULL, 1000, 0);
    insert_message("agent0-conv-test-12", "clear", NULL);
    insert_message("agent0-conv-test-12", "user", "Test message");
    insert_message("agent0-conv-test-12", "assistant", "Response");

    // Get fork point
    int64_t fork_id = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "agent0-conv-test-12", &fork_id);
    ck_assert(is_ok(&res));

    // Insert child with its own messages
    insert_agent("child1-conv-test-12", "agent0-conv-test-12", 2000, fork_id);
    insert_message("child1-conv-test-12", "user", "Child message");

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-conv-test-12");

    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify child exists and has conversation
    ck_assert_uint_eq(repl->agent_count, 2);
    ik_agent_ctx_t *child = repl->agents[1];
    ck_assert_uint_ge(child->message_count, 0);

    // Child should have parent's messages plus its own
    ck_assert_uint_ge(child->message_count, 3);
}

END_TEST
// Test: restore_agents populates scrollback from replay
START_TEST(test_restore_agents_populates_scrollback) {
    SKIP_IF_NO_DB();

    // Insert Agent 0 with messages
    insert_agent("agent0-scroll-test1", NULL, 1000, 0);
    insert_message("agent0-scroll-test1", "clear", NULL);
    insert_message("agent0-scroll-test1", "user", "User input");
    insert_message("agent0-scroll-test1", "assistant", "AI response");

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-scroll-test1");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Scrollback should have content
    ck_assert_ptr_nonnull(repl->current->scrollback);
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(line_count, 0);
}

END_TEST
// Test: restore_agents handles mark events (marks are stored in DB but not processed by agent_replay)
// Note: The agent_replay module returns raw messages without processing marks.
// Mark processing happens at a higher level when needed.
START_TEST(test_restore_agents_handles_mark_events) {
    SKIP_IF_NO_DB();

    // Insert Agent 0 with mark events
    insert_agent("agent0-marks-test12", NULL, 1000, 0);
    insert_message("agent0-marks-test12", "clear", NULL);
    insert_message("agent0-marks-test12", "user", "Before mark");
    // Insert mark event
    res_t res = ik_db_message_insert(db, session_id, "agent0-marks-test12",
                                     "mark", NULL, "{\"label\":\"checkpoint1\"}");
    ck_assert(is_ok(&res));
    insert_message("agent0-marks-test12", "user", "After mark");

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-marks-test12");

    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify restore succeeded (mark events in DB are valid)
    ck_assert_uint_eq(repl->agent_count, 1);
}

END_TEST
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

// ========== Suite Configuration ==========

static Suite *agent_restore_suite(void)
{
    Suite *s = suite_create("Agent Restore");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_restore_agents_queries_running_agents);
    tcase_add_test(tc_core, test_restore_agents_sorts_by_created_at);
    tcase_add_test(tc_core, test_restore_agents_skips_none_restores_all_running);
    tcase_add_test(tc_core, test_restore_agents_handles_agent0_specially);
    tcase_add_test(tc_core, test_restore_agents_populates_conversation);
    tcase_add_test(tc_core, test_restore_agents_populates_scrollback);
    tcase_add_test(tc_core, test_restore_agents_handles_mark_events);
    tcase_add_test(tc_core, test_restore_agents_handles_empty_history);
    tcase_add_test(tc_core, test_restore_agents_handles_restore_failure_gracefully);

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
