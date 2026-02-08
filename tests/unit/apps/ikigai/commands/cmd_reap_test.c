/**
 * @file cmd_reap_test.c
 * @brief Unit tests for /reap command
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/session.h"
#include "shared/error.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/wrapper.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

// Mock posix_rename_ to prevent PANIC during logger rotation
int posix_rename_(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return 0;
}

// Test fixtures
static const char *DB_NAME;
static ik_db_ctx_t *db;
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;
static int64_t session_id;

// Helper: Create minimal agent
static ik_agent_ctx_t *create_agent(const char *uuid, const char *parent_uuid)
{
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->uuid = talloc_strdup(agent, uuid);
    agent->name = NULL;
    agent->parent_uuid = parent_uuid ? talloc_strdup(agent, parent_uuid) : NULL;
    agent->created_at = 1234567890;
    agent->fork_message_id = 0;
    agent->scrollback = ik_scrollback_create(agent, 80);
    agent->dead = false;
    agent->shared = repl->shared;
    agent->repl = repl;

    // Insert into DB
    res_t res = ik_db_agent_insert(db, agent);
    ck_assert(is_ok(&res));

    return agent;
}

// Helper: Add agent to repl array
static void add_agent_to_repl(ik_agent_ctx_t *agent)
{
    if (repl->agent_count >= repl->agent_capacity) {
        ck_abort_msg("Agent array full");
    }
    repl->agents[repl->agent_count++] = agent;
}

// Helper: Setup minimal REPL
static void setup_repl(void)
{
    ik_config_t *cfg = talloc_zero(test_ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db;
    shared->session_id = session_id;
    repl->shared = shared;

    // Initialize agent array
    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agent_count = 0;
    repl->agent_capacity = 16;

    // Create initial agent
    ik_agent_ctx_t *agent = create_agent("root-uuid", NULL);
    add_agent_to_repl(agent);
    repl->current = agent;
}

static bool suite_setup(void)
{
    DB_NAME = ik_test_db_name(NULL, __FILE__);
    res_t res = ik_test_db_create(DB_NAME);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to create database: %s\n", error_message(res.err));
        talloc_free(res.err);
        return false;
    }
    res = ik_test_db_migrate(NULL, DB_NAME);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to migrate database: %s\n", error_message(res.err));
        talloc_free(res.err);
        ik_test_db_destroy(DB_NAME);
        return false;
    }
    return true;
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    res_t db_res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&db_res)) {
        fprintf(stderr, "Failed to connect to database: %s\n", error_message(db_res.err));
        ck_abort_msg("Database connection failed");
    }

    db_res = ik_test_db_begin(db);
    if (is_err(&db_res)) {
        fprintf(stderr, "Failed to begin transaction: %s\n", error_message(db_res.err));
        ck_abort_msg("Begin transaction failed");
    }

    db_res = ik_db_session_create(db, &session_id);
    if (is_err(&db_res)) {
        fprintf(stderr, "Failed to create session: %s\n", error_message(db_res.err));
        ck_abort_msg("Session creation failed");
    }

    setup_repl();
}

static void teardown(void)
{
    if (db != NULL && test_ctx != NULL) {
        ik_test_db_rollback(db);
    }

    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }

    db = NULL;
}

static void suite_teardown(void)
{
    ik_test_db_destroy(DB_NAME);
}

// Test: Bulk mode with no dead agents
START_TEST(test_reap_bulk_no_dead_agents) {
    size_t initial_count = repl->agent_count;
    size_t initial_lines = ik_scrollback_get_line_count(repl->current->scrollback);

    res_t res = ik_cmd_reap(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify count unchanged
    ck_assert_uint_eq(repl->agent_count, initial_count);

    // Verify message appeared
    size_t final_lines = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(final_lines, initial_lines);
}
END_TEST

// Test: Bulk mode removes all dead agents
START_TEST(test_reap_bulk_removes_dead) {
    // Create two dead agents
    ik_agent_ctx_t *dead1 = create_agent("dead-1", NULL);
    add_agent_to_repl(dead1);
    dead1->dead = true;
    res_t res = ik_db_agent_mark_dead(db, dead1->uuid);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *dead2 = create_agent("dead-2", NULL);
    add_agent_to_repl(dead2);
    dead2->dead = true;
    res = ik_db_agent_mark_dead(db, dead2->uuid);
    ck_assert(is_ok(&res));

    size_t initial_count = repl->agent_count;

    res = ik_cmd_reap(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify both dead agents removed
    ck_assert_uint_eq(repl->agent_count, initial_count - 2);

    // Verify neither dead agent is in array
    for (size_t i = 0; i < repl->agent_count; i++) {
        ck_assert(repl->agents[i] != dead1);
        ck_assert(repl->agents[i] != dead2);
    }
}
END_TEST

// Test: Targeted mode - agent not found
START_TEST(test_reap_targeted_not_found) {
    size_t initial_lines = ik_scrollback_get_line_count(repl->current->scrollback);

    res_t res = ik_cmd_reap(test_ctx, repl, "nonexistent-uuid");
    ck_assert(is_ok(&res));

    // Verify error message appeared
    size_t final_lines = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(final_lines, initial_lines);
}
END_TEST

// Test: Targeted mode - agent is not dead
START_TEST(test_reap_targeted_not_dead) {
    ik_agent_ctx_t *living = create_agent("living-agent", NULL);
    add_agent_to_repl(living);

    size_t initial_lines = ik_scrollback_get_line_count(repl->current->scrollback);

    res_t res = ik_cmd_reap(test_ctx, repl, "living-agent");
    ck_assert(is_ok(&res));

    // Verify error message appeared
    size_t final_lines = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(final_lines, initial_lines);

    // Verify agent still in array
    ck_assert_uint_eq(repl->agent_count, 2);
}
END_TEST

// Test: Targeted mode removes specified dead agent
START_TEST(test_reap_targeted_removes_dead) {
    ik_agent_ctx_t *dead = create_agent("dead-target", NULL);
    add_agent_to_repl(dead);
    dead->dead = true;
    res_t res = ik_db_agent_mark_dead(db, dead->uuid);
    ck_assert(is_ok(&res));

    size_t initial_count = repl->agent_count;

    res = ik_cmd_reap(test_ctx, repl, "dead-target");
    ck_assert(is_ok(&res));

    // Verify agent removed
    ck_assert_uint_eq(repl->agent_count, initial_count - 1);

    // Verify agent not in array
    for (size_t i = 0; i < repl->agent_count; i++) {
        ck_assert(repl->agents[i] != dead);
    }
}
END_TEST

// Test: Targeted mode removes descendants
START_TEST(test_reap_targeted_removes_descendants) {
    ik_agent_ctx_t *parent = create_agent("dead-parent", NULL);
    add_agent_to_repl(parent);
    parent->dead = true;
    res_t res = ik_db_agent_mark_dead(db, parent->uuid);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = create_agent("child-of-dead", "dead-parent");
    add_agent_to_repl(child);

    size_t initial_count = repl->agent_count;

    res = ik_cmd_reap(test_ctx, repl, "dead-parent");
    ck_assert(is_ok(&res));

    // Verify both parent and child removed
    ck_assert_uint_eq(repl->agent_count, initial_count - 2);
}
END_TEST

// Test: View switches when current agent is reaped
START_TEST(test_reap_switches_view) {
    ik_agent_ctx_t *dead = create_agent("dead-current", NULL);
    add_agent_to_repl(dead);
    dead->dead = true;
    res_t res = ik_db_agent_mark_dead(db, dead->uuid);
    ck_assert(is_ok(&res));

    // Switch to dead agent
    repl->current = dead;

    res = ik_cmd_reap(test_ctx, repl, "dead-current");
    ck_assert(is_ok(&res));

    // Verify view switched to root agent
    ck_assert_ptr_eq(repl->current, repl->agents[0]);
    ck_assert(!repl->current->dead);
}
END_TEST

// Test: Cannot reap when no living agents remain
START_TEST(test_reap_no_living_agents) {
    // Mark root agent as dead
    repl->current->dead = true;
    res_t res = ik_db_agent_mark_dead(db, repl->current->uuid);
    ck_assert(is_ok(&res));

    size_t initial_lines = ik_scrollback_get_line_count(repl->current->scrollback);

    res = ik_cmd_reap(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify error message appeared
    size_t final_lines = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(final_lines, initial_lines);

    // Verify agent not removed
    ck_assert_uint_eq(repl->agent_count, 1);
}
END_TEST

// Test: Reap reports correct count
START_TEST(test_reap_reports_count) {
    ik_agent_ctx_t *dead = create_agent("dead-for-count", NULL);
    add_agent_to_repl(dead);
    dead->dead = true;
    res_t res = ik_db_agent_mark_dead(db, dead->uuid);
    ck_assert(is_ok(&res));

    size_t initial_lines = ik_scrollback_get_line_count(repl->current->scrollback);

    res = ik_cmd_reap(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify count message appeared
    size_t final_lines = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(final_lines, initial_lines);
}
END_TEST

// Test: View switches when reaping ancestor
START_TEST(test_reap_switches_when_ancestor_reaped) {
    // Create dead parent
    ik_agent_ctx_t *dead_parent = create_agent("dead-ancestor", NULL);
    add_agent_to_repl(dead_parent);
    dead_parent->dead = true;
    res_t res = ik_db_agent_mark_dead(db, dead_parent->uuid);
    ck_assert(is_ok(&res));

    // Create living child
    ik_agent_ctx_t *child = create_agent("living-child", "dead-ancestor");
    add_agent_to_repl(child);

    // Switch to child
    repl->current = child;

    // Reap parent
    res = ik_cmd_reap(test_ctx, repl, "dead-ancestor");
    ck_assert(is_ok(&res));

    // Verify view switched to root (since child was also reaped as descendant)
    ck_assert_ptr_eq(repl->current, repl->agents[0]);
}
END_TEST

// Test: View switches when grandchild viewing and grandparent reaped
START_TEST(test_reap_switches_grandchild_view) {
    // Create dead grandparent
    ik_agent_ctx_t *grandparent = create_agent("dead-gp", NULL);
    add_agent_to_repl(grandparent);
    grandparent->dead = true;
    res_t res = ik_db_agent_mark_dead(db, grandparent->uuid);
    ck_assert(is_ok(&res));

    // Create living parent (child of dead grandparent)
    ik_agent_ctx_t *parent = create_agent("living-parent", "dead-gp");
    add_agent_to_repl(parent);

    // Create living grandchild
    ik_agent_ctx_t *grandchild = create_agent("living-gc", "living-parent");
    add_agent_to_repl(grandchild);

    // Switch to grandchild
    repl->current = grandchild;

    // Reap grandparent (should cascade to parent and grandchild)
    res = ik_cmd_reap(test_ctx, repl, "dead-gp");
    ck_assert(is_ok(&res));

    // Verify view switched to root
    ck_assert_ptr_eq(repl->current, repl->agents[0]);
}
END_TEST

// Test: Bulk mode with empty string (same as NULL)
START_TEST(test_reap_bulk_empty_string) {
    ik_agent_ctx_t *dead = create_agent("dead-bulk", NULL);
    add_agent_to_repl(dead);
    dead->dead = true;
    res_t res = ik_db_agent_mark_dead(db, dead->uuid);
    ck_assert(is_ok(&res));

    size_t initial_count = repl->agent_count;

    res = ik_cmd_reap(test_ctx, repl, "");
    ck_assert(is_ok(&res));

    // Verify dead agent removed
    ck_assert_uint_eq(repl->agent_count, initial_count - 1);
}
END_TEST

// Test: Bulk reap switches view when current is child of dead agent
START_TEST(test_reap_bulk_living_child_of_dead) {
    // Create dead parent
    ik_agent_ctx_t *dead_parent = create_agent("dead-parent-bulk", NULL);
    add_agent_to_repl(dead_parent);
    dead_parent->dead = true;
    res_t res = ik_db_agent_mark_dead(db, dead_parent->uuid);
    ck_assert(is_ok(&res));

    // Create living child of dead parent
    ik_agent_ctx_t *living_child = create_agent("living-child-bulk", "dead-parent-bulk");
    add_agent_to_repl(living_child);

    // Switch to living child
    repl->current = living_child;

    // Bulk reap - should detect that current's parent is dead and switch view
    res = ik_cmd_reap(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify view switched to root
    ck_assert_ptr_eq(repl->current, repl->agents[0]);
}
END_TEST

// Test: Deep parent chain walk (grandchild -> living parent -> dead grandparent)
START_TEST(test_reap_bulk_deep_parent_chain) {
    // Create dead grandparent
    ik_agent_ctx_t *dead_gp = create_agent("dead-gp-chain", NULL);
    add_agent_to_repl(dead_gp);
    dead_gp->dead = true;
    res_t res = ik_db_agent_mark_dead(db, dead_gp->uuid);
    ck_assert(is_ok(&res));

    // Create living parent (child of dead grandparent)
    ik_agent_ctx_t *living_parent = create_agent("living-parent-chain", "dead-gp-chain");
    add_agent_to_repl(living_parent);

    // Create living grandchild (child of living parent)
    ik_agent_ctx_t *grandchild = create_agent("living-gc-chain", "living-parent-chain");
    add_agent_to_repl(grandchild);

    // Switch to grandchild
    repl->current = grandchild;

    // Bulk reap - should walk parent chain to detect dead grandparent
    res = ik_cmd_reap(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify view switched to root
    ck_assert_ptr_eq(repl->current, repl->agents[0]);
}
END_TEST

static Suite *reap_suite(void)
{
    Suite *s = suite_create("Reap Command");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_reap_bulk_no_dead_agents);
    tcase_add_test(tc, test_reap_bulk_removes_dead);
    tcase_add_test(tc, test_reap_targeted_not_found);
    tcase_add_test(tc, test_reap_targeted_not_dead);
    tcase_add_test(tc, test_reap_targeted_removes_dead);
    tcase_add_test(tc, test_reap_targeted_removes_descendants);
    tcase_add_test(tc, test_reap_switches_view);
    tcase_add_test(tc, test_reap_no_living_agents);
    tcase_add_test(tc, test_reap_reports_count);
    tcase_add_test(tc, test_reap_switches_when_ancestor_reaped);
    tcase_add_test(tc, test_reap_switches_grandchild_view);
    tcase_add_test(tc, test_reap_bulk_empty_string);
    tcase_add_test(tc, test_reap_bulk_living_child_of_dead);
    tcase_add_test(tc, test_reap_bulk_deep_parent_chain);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = reap_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/cmd_reap_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (failed == 0) ? 0 : 1;
}
