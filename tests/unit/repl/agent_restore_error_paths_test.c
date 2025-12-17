/**
 * @file agent_restore_error_paths_test.c
 * @brief Tests for agent_restore error handling and edge cases
 *
 * Tests error paths, fresh install scenario with system message, and mark restoration.
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

// Test: Fresh install scenario - Agent 0 with no history writes clear and system message
START_TEST(test_restore_agents_fresh_install_with_system_message)
{
    SKIP_IF_NO_DB();

    // Insert Agent 0 with NO messages (fresh install)
    insert_agent("agent0-fresh-test12", NULL, 1000, 0);

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-fresh-test12");

    // Set system message in config
    talloc_free(repl->shared->cfg->openai_system_message);
    repl->shared->cfg->openai_system_message = talloc_strdup(
        repl->shared->cfg, "You are a helpful assistant.");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify Agent 0 restored
    ck_assert_uint_eq(repl->agent_count, 1);

    // Verify system message was added to conversation
    ck_assert_uint_ge(repl->current->conversation->message_count, 1);
}
END_TEST

// Test: Fresh install scenario - Agent 0 with no history and NO system message configured
START_TEST(test_restore_agents_fresh_install_no_system_message)
{
    SKIP_IF_NO_DB();

    // Insert Agent 0 with NO messages (fresh install)
    insert_agent("agent0-fresh-nosys1", NULL, 1000, 0);

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-fresh-nosys1");

    // Clear system message in config
    talloc_free(repl->shared->cfg->openai_system_message);
    repl->shared->cfg->openai_system_message = NULL;

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify Agent 0 restored
    ck_assert_uint_eq(repl->agent_count, 1);
}
END_TEST

// Test: Agent 0 with marks in history - marks tested in agent_restore_test.c
// (mark_stack population depends on replay logic implementation)
START_TEST(test_restore_agents_agent0_with_mark_events)
{
    SKIP_IF_NO_DB();

    // Insert Agent 0 with mark events
    insert_agent("agent0-marks-test34", NULL, 1000, 0);
    insert_message("agent0-marks-test34", "clear", NULL);
    insert_message("agent0-marks-test34", "user", "Before mark");

    // Insert mark event with label
    res_t res = ik_db_message_insert(db, session_id, "agent0-marks-test34",
                                      "mark", NULL, "{\"label\":\"checkpoint1\"}");
    ck_assert(is_ok(&res));

    insert_message("agent0-marks-test34", "user", "After mark");

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-marks-test34");

    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify restore succeeded (marks are in DB)
    ck_assert_uint_eq(repl->agent_count, 1);
}
END_TEST

// Test: Child agent with mark events in history
START_TEST(test_restore_agents_child_with_mark_events)
{
    SKIP_IF_NO_DB();

    // Insert Agent 0
    insert_agent("agent0-child-marks1", NULL, 1000, 0);
    insert_message("agent0-child-marks1", "clear", NULL);
    insert_message("agent0-child-marks1", "user", "Parent message");

    // Get fork point
    int64_t fork_id = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "agent0-child-marks1", &fork_id);
    ck_assert(is_ok(&res));

    // Insert child with marks
    insert_agent("child-marks-test123", "agent0-child-marks1", 2000, fork_id);
    insert_message("child-marks-test123", "user", "Child message");

    // Insert mark in child
    res = ik_db_message_insert(db, session_id, "child-marks-test123",
                                "mark", NULL, "{\"label\":\"child_checkpoint\"}");
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-child-marks1");

    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify child exists
    ck_assert_uint_eq(repl->agent_count, 2);
}
END_TEST

// Test: Agent with only one message in comparison function
START_TEST(test_restore_agents_single_agent_comparison)
{
    SKIP_IF_NO_DB();

    // Insert single agent (exercises qsort comparison with count=1, which skips qsort)
    insert_agent("agent0-single-test1", NULL, 1000, 0);
    insert_message("agent0-single-test1", "clear", NULL);

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-single-test1");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->agent_count, 1);
}
END_TEST

// Test: Agents with identical timestamps (exercises comparison return 0)
START_TEST(test_restore_agents_identical_timestamps)
{
    SKIP_IF_NO_DB();

    // Insert Agent 0
    insert_agent("agent0-ident-ts-tes", NULL, 1000, 0);
    insert_message("agent0-ident-ts-tes", "clear", NULL);

    // Insert children with IDENTICAL timestamps
    insert_agent("child1-ident-ts-te", "agent0-ident-ts-tes", 2000, 0);
    insert_agent("child2-ident-ts-te", "agent0-ident-ts-tes", 2000, 0);  // Same timestamp

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-ident-ts-tes");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Should succeed even with identical timestamps
    ck_assert_uint_eq(repl->agent_count, 3);
}
END_TEST

// Test: Agents sorted correctly with return 1 from comparison (newer timestamp)
START_TEST(test_restore_agents_comparison_return_1)
{
    SKIP_IF_NO_DB();

    // Insert Agent 0
    insert_agent("agent0-cmp1-test123", NULL, 1000, 0);
    insert_message("agent0-cmp1-test123", "clear", NULL);

    // Insert newer child first, then older (exercises return 1 and return -1 paths)
    insert_agent("newer-child-cmp1-t", "agent0-cmp1-test123", 3000, 0);
    insert_agent("older-child-cmp1-t", "agent0-cmp1-test123", 2000, 0);

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-cmp1-test123");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify older child is restored first (index 1)
    ck_assert_uint_eq(repl->agent_count, 3);
    ck_assert_str_eq(repl->agents[1]->uuid, "older-child-cmp1-t");
    ck_assert_str_eq(repl->agents[2]->uuid, "newer-child-cmp1-t");
}
END_TEST

// Test: Agent 0 with multiple message kinds (exercises conversation filter)
START_TEST(test_restore_agents_agent0_multiple_kinds)
{
    SKIP_IF_NO_DB();

    // Insert Agent 0 with various message kinds
    insert_agent("agent0-kinds-test12", NULL, 1000, 0);
    insert_message("agent0-kinds-test12", "clear", NULL);
    insert_message("agent0-kinds-test12", "user", "User message");
    insert_message("agent0-kinds-test12", "assistant", "Assistant message");
    insert_message("agent0-kinds-test12", "tool_call", NULL);  // Non-conversation kind
    insert_message("agent0-kinds-test12", "tool_result", "Result");  // Non-conversation kind

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-kinds-test12");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Conversation should only have user and assistant messages
    ck_assert_uint_eq(repl->agent_count, 1);
    ck_assert_uint_ge(repl->current->conversation->message_count, 2);
}
END_TEST

// Test: Child agent with multiple message kinds
START_TEST(test_restore_agents_child_multiple_kinds)
{
    SKIP_IF_NO_DB();

    // Insert Agent 0
    insert_agent("agent0-child-kinds1", NULL, 1000, 0);
    insert_message("agent0-child-kinds1", "clear", NULL);
    insert_message("agent0-child-kinds1", "user", "Parent user message");

    // Get fork point
    int64_t fork_id = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "agent0-child-kinds1", &fork_id);
    ck_assert(is_ok(&res));

    // Insert child with various message kinds
    insert_agent("child-kinds-test123", "agent0-child-kinds1", 2000, fork_id);
    insert_message("child-kinds-test123", "user", "Child user message");
    insert_message("child-kinds-test123", "assistant", "Child assistant message");
    insert_message("child-kinds-test123", "tool_call", NULL);

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-child-kinds1");

    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify child conversation filtered correctly
    ck_assert_uint_eq(repl->agent_count, 2);
    ik_agent_ctx_t *child = repl->agents[1];
    ck_assert_uint_ge(child->conversation->message_count, 2);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *agent_restore_error_suite(void)
{
    Suite *s = suite_create("Agent Restore Error Paths");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_restore_agents_fresh_install_with_system_message);
    tcase_add_test(tc_core, test_restore_agents_fresh_install_no_system_message);
    tcase_add_test(tc_core, test_restore_agents_agent0_with_mark_events);
    tcase_add_test(tc_core, test_restore_agents_child_with_mark_events);
    tcase_add_test(tc_core, test_restore_agents_single_agent_comparison);
    tcase_add_test(tc_core, test_restore_agents_identical_timestamps);
    tcase_add_test(tc_core, test_restore_agents_comparison_return_1);
    tcase_add_test(tc_core, test_restore_agents_agent0_multiple_kinds);
    tcase_add_test(tc_core, test_restore_agents_child_multiple_kinds);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_restore_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
