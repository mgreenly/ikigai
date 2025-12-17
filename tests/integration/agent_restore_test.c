/**
 * @file agent_restore_test.c
 * @brief Integration tests for multi-agent restoration
 *
 * Tests the complete agent restoration flow including:
 * - Multi-agent hierarchy preservation
 * - Fork point boundary enforcement
 * - Clear event handling
 */

#include "../../src/repl/agent_restore.h"
#include "../../src/db/agent.h"
#include "../../src/db/agent_replay.h"
#include "../../src/db/connection.h"
#include "../../src/db/message.h"
#include "../../src/db/session.h"
#include "../../src/agent.h"
#include "../../src/repl.h"
#include "../../src/error.h"
#include "../../src/config.h"
#include "../../src/shared.h"
#include "../../src/logger.h"
#include "../test_utils.h"
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

// Helper: Insert a message and return its ID
static int64_t insert_message_get_id(const char *agent_uuid, const char *kind,
                                      const char *content)
{
    res_t res = ik_db_message_insert(db, session_id, agent_uuid, kind, content, "{}");
    ck_assert(is_ok(&res));

    int64_t msg_id = 0;
    res = ik_db_agent_get_last_message_id(db, agent_uuid, &msg_id);
    ck_assert(is_ok(&res));
    return msg_id;
}

// Helper: Insert a message
static void insert_message(const char *agent_uuid, const char *kind,
                           const char *content)
{
    res_t res = ik_db_message_insert(db, session_id, agent_uuid, kind, content, "{}");
    ck_assert(is_ok(&res));
}

// Helper: Create minimal repl context for testing
static ik_repl_ctx_t *create_test_repl(const char *agent0_uuid)
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

    // Set Agent 0's UUID to match DB
    talloc_free(agent0->uuid);
    agent0->uuid = talloc_strdup(agent0, agent0_uuid);

    repl->agents[0] = agent0;
    repl->agent_count = 1;
    repl->current = agent0;

    return repl;
}

// ========== Integration Test Cases ==========

// Test: Multiple agents survive restart with hierarchy preserved
START_TEST(test_multi_agent_restart_preserves_hierarchy)
{
    SKIP_IF_NO_DB();

    // Setup:
    // 1. Insert parent agent (Agent 0)
    insert_agent("parent-hierarchy-te", NULL, 1000, 0);
    insert_message("parent-hierarchy-te", "clear", NULL);
    insert_message("parent-hierarchy-te", "user", "Parent msg 1");
    insert_message("parent-hierarchy-te", "assistant", "Parent msg 2");

    // 2. Get fork point
    int64_t fork_id = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "parent-hierarchy-te", &fork_id);
    ck_assert(is_ok(&res));

    // 3. Insert child agent forked from parent
    insert_agent("child-hierarchy-tes", "parent-hierarchy-te", 2000, fork_id);
    insert_message("child-hierarchy-tes", "user", "Child msg 1");

    // Simulate restart:
    // 4. Create fresh repl context with Agent 0
    ik_repl_ctx_t *repl = create_test_repl("parent-hierarchy-te");

    // 5. Call ik_repl_restore_agents()
    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify:
    // - Child agent exists in repl->agents[]
    ck_assert_uint_eq(repl->agent_count, 2);

    // - Child's parent_uuid points to Agent 0
    ik_agent_ctx_t *child = repl->agents[1];
    ck_assert_str_eq(child->parent_uuid, "parent-hierarchy-te");

    // - Child's conversation contains messages
    ck_assert_uint_ge(child->conversation->message_count, 3);
}
END_TEST

// Test: Forked agent survives restart with correct history
START_TEST(test_forked_agent_survives_restart)
{
    SKIP_IF_NO_DB();

    // Setup:
    // 1. Create parent with messages: [A, B]
    insert_agent("parent-fork-surv-te", NULL, 1000, 0);
    insert_message("parent-fork-surv-te", "clear", NULL);
    insert_message("parent-fork-surv-te", "user", "A");
    int64_t fork_point = insert_message_get_id("parent-fork-surv-te", "assistant", "B");

    // 2. Fork child after message B
    insert_agent("child-fork-surv-tes", "parent-fork-surv-te", 2000, fork_point);

    // 3. Add parent messages after fork: [C, D]
    insert_message("parent-fork-surv-te", "user", "C");
    insert_message("parent-fork-surv-te", "assistant", "D");

    // 4. Add child messages: [X, Y]
    insert_message("child-fork-surv-tes", "user", "X");
    insert_message("child-fork-surv-tes", "assistant", "Y");

    // Simulate restart and restore
    ik_repl_ctx_t *repl = create_test_repl("parent-fork-surv-te");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify:
    ck_assert_uint_eq(repl->agent_count, 2);

    // Parent sees: [A, B, C, D]
    ik_agent_ctx_t *parent = repl->current;
    ck_assert_uint_ge(parent->conversation->message_count, 4);

    // Child sees: [A, B, X, Y] (not C, D)
    ik_agent_ctx_t *child = repl->agents[1];
    ck_assert_uint_eq(child->conversation->message_count, 4);

    // Verify child has A, B, X, Y
    ck_assert_str_eq(child->conversation->messages[0]->content, "A");
    ck_assert_str_eq(child->conversation->messages[1]->content, "B");
    ck_assert_str_eq(child->conversation->messages[2]->content, "X");
    ck_assert_str_eq(child->conversation->messages[3]->content, "Y");
}
END_TEST

// Test: Killed agents not restored (status != 'running')
START_TEST(test_killed_agent_not_restored)
{
    SKIP_IF_NO_DB();

    // Setup:
    // 1. Insert running agent
    insert_agent("running-kill-test12", NULL, 1000, 0);
    insert_message("running-kill-test12", "clear", NULL);

    // 2. Insert child, then kill it
    insert_agent("dead-child-kill-te", "running-kill-test12", 2000, 0);
    res_t res = ik_db_agent_mark_dead(db, "dead-child-kill-te");
    ck_assert(is_ok(&res));

    // 3. Insert another running child
    insert_agent("live-child-kill-te", "running-kill-test12", 3000, 0);

    // Call ik_repl_restore_agents()
    ik_repl_ctx_t *repl = create_test_repl("running-kill-test12");
    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify:
    // - Only running agent restored (root + live child)
    ck_assert_uint_eq(repl->agent_count, 2);

    // - Dead agent not in repl->agents[]
    bool found_dead = false;
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strcmp(repl->agents[i]->uuid, "dead-child-kill-te") == 0) {
            found_dead = true;
            break;
        }
    }
    ck_assert(!found_dead);
}
END_TEST

// Test: Fork points respected on restore - child doesn't see parent's post-fork messages
START_TEST(test_fork_points_respected_on_restore)
{
    SKIP_IF_NO_DB();

    // Setup scenario:
    // - Parent: msg1, msg2, msg3 (fork here), msg4, msg5
    insert_agent("parent-forkpt-test", NULL, 1000, 0);
    insert_message("parent-forkpt-test", "clear", NULL);
    insert_message("parent-forkpt-test", "user", "msg1");
    insert_message("parent-forkpt-test", "assistant", "msg2");
    int64_t fork_point = insert_message_get_id("parent-forkpt-test", "user", "msg3");

    // Child forked at msg3
    insert_agent("child-forkpt-test1", "parent-forkpt-test", 2000, fork_point);

    // Parent continues after fork
    insert_message("parent-forkpt-test", "assistant", "msg4");
    insert_message("parent-forkpt-test", "user", "msg5");

    // Child adds its own messages
    insert_message("child-forkpt-test1", "user", "child_msg1");
    insert_message("child-forkpt-test1", "assistant", "child_msg2");

    // Restore
    ik_repl_ctx_t *repl = create_test_repl("parent-forkpt-test");
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // After restore:
    // - Child's conversation should have: msg1, msg2, msg3, child_msg1, child_msg2
    // - Child should NOT have: msg4, msg5
    ik_agent_ctx_t *child = repl->agents[1];
    ck_assert_uint_eq(child->conversation->message_count, 5);

    // Verify child messages
    ck_assert_str_eq(child->conversation->messages[0]->content, "msg1");
    ck_assert_str_eq(child->conversation->messages[1]->content, "msg2");
    ck_assert_str_eq(child->conversation->messages[2]->content, "msg3");
    ck_assert_str_eq(child->conversation->messages[3]->content, "child_msg1");
    ck_assert_str_eq(child->conversation->messages[4]->content, "child_msg2");

    // Verify child does NOT have msg4 or msg5
    for (size_t i = 0; i < child->conversation->message_count; i++) {
        ck_assert_str_ne(child->conversation->messages[i]->content, "msg4");
        ck_assert_str_ne(child->conversation->messages[i]->content, "msg5");
    }
}
END_TEST

// Test: Clear events respected - don't walk past clear
START_TEST(test_clear_events_respected_on_restore)
{
    SKIP_IF_NO_DB();

    // Setup:
    // - Agent with: msg1, msg2, clear, msg3, msg4
    insert_agent("agent-clear-test123", NULL, 1000, 0);
    insert_message("agent-clear-test123", "user", "msg1");
    insert_message("agent-clear-test123", "assistant", "msg2");
    insert_message("agent-clear-test123", "clear", NULL);
    insert_message("agent-clear-test123", "user", "msg3");
    insert_message("agent-clear-test123", "assistant", "msg4");

    // Restore
    ik_repl_ctx_t *repl = create_test_repl("agent-clear-test123");
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // After restore:
    // - Agent's conversation should have: msg3, msg4
    // - Agent should NOT have: msg1, msg2
    ck_assert_uint_eq(repl->current->conversation->message_count, 2);
    ck_assert_str_eq(repl->current->conversation->messages[0]->content, "msg3");
    ck_assert_str_eq(repl->current->conversation->messages[1]->content, "msg4");
}
END_TEST

// Test: Deep ancestry - grandchild accessing grandparent context
START_TEST(test_deep_ancestry_on_restore)
{
    SKIP_IF_NO_DB();

    // Setup 3-level hierarchy:
    // - Grandparent: gp_msg1, gp_msg2
    insert_agent("grandparent-deep-te", NULL, 1000, 0);
    insert_message("grandparent-deep-te", "clear", NULL);
    insert_message("grandparent-deep-te", "user", "gp_msg1");
    int64_t gp_fork = insert_message_get_id("grandparent-deep-te", "assistant", "gp_msg2");

    // - Parent (forked from grandparent): p_msg1
    insert_agent("parent-deep-test-12", "grandparent-deep-te", 2000, gp_fork);
    int64_t p_fork = insert_message_get_id("parent-deep-test-12", "user", "p_msg1");

    // - Child (forked from parent): c_msg1
    insert_agent("child-deep-test-123", "parent-deep-test-12", 3000, p_fork);
    insert_message("child-deep-test-123", "user", "c_msg1");

    // Restore
    ik_repl_ctx_t *repl = create_test_repl("grandparent-deep-te");
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // After restore, child should see:
    // - gp_msg1, gp_msg2 (from grandparent, up to fork point)
    // - p_msg1 (from parent, up to fork point)
    // - c_msg1 (own messages)
    ck_assert_uint_eq(repl->agent_count, 3);

    // Find child agent
    ik_agent_ctx_t *child = NULL;
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strcmp(repl->agents[i]->uuid, "child-deep-test-123") == 0) {
            child = repl->agents[i];
            break;
        }
    }
    ck_assert_ptr_nonnull(child);

    // Child should have 4 messages: gp_msg1, gp_msg2, p_msg1, c_msg1
    ck_assert_uint_eq(child->conversation->message_count, 4);
    ck_assert_str_eq(child->conversation->messages[0]->content, "gp_msg1");
    ck_assert_str_eq(child->conversation->messages[1]->content, "gp_msg2");
    ck_assert_str_eq(child->conversation->messages[2]->content, "p_msg1");
    ck_assert_str_eq(child->conversation->messages[3]->content, "c_msg1");
}
END_TEST

// Test: Dependency ordering - parents created before children
START_TEST(test_dependency_ordering_on_restore)
{
    SKIP_IF_NO_DB();

    // Setup:
    // - Insert agents with timestamps ensuring child is created before parent
    // (edge case that shouldn't happen in practice but tests robustness)
    insert_agent("parent-order-test12", NULL, 2000, 0);  // Later timestamp
    insert_message("parent-order-test12", "clear", NULL);

    // Insert child with earlier created_at (abnormal case)
    insert_agent("child-order-test123", "parent-order-test12", 1000, 0);  // Earlier timestamp

    // After restore:
    ik_repl_ctx_t *repl = create_test_repl("parent-order-test12");
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify both agents exist (sorting by created_at should still work)
    ck_assert_uint_eq(repl->agent_count, 2);

    // The child with earlier created_at should be processed first during sorting
    // but since parent_uuid reference exists, it should be handled correctly
}
END_TEST

// ========== Suite Configuration ==========

static Suite *agent_restore_integration_suite(void)
{
    Suite *s = suite_create("Agent Restore Integration");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_multi_agent_restart_preserves_hierarchy);
    tcase_add_test(tc_core, test_forked_agent_survives_restart);
    tcase_add_test(tc_core, test_killed_agent_not_restored);
    tcase_add_test(tc_core, test_fork_points_respected_on_restore);
    tcase_add_test(tc_core, test_clear_events_respected_on_restore);
    tcase_add_test(tc_core, test_deep_ancestry_on_restore);
    tcase_add_test(tc_core, test_dependency_ordering_on_restore);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_restore_integration_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
