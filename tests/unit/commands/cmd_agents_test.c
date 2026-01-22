/**
 * @file cmd_agents_test.c
 * @brief Unit tests for /agents command
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

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

// Helper: Create minimal REPL for testing
static void setup_repl(void)
{
    ik_scrollback_t *sb = ik_scrollback_create(test_ctx, 80);
    ck_assert_ptr_nonnull(sb);

    ik_config_t *cfg = talloc_zero(test_ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = sb;

    agent->uuid = talloc_strdup(agent, "root-uuid-123");
    agent->name = NULL;
    agent->parent_uuid = NULL;
    agent->created_at = 1234567890;
    agent->fork_message_id = 0;
    repl->current = agent;

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db;
    shared->session_id = 1;
    repl->shared = shared;
    agent->shared = shared;

    // Initialize agent array
    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->agent_capacity = 16;

    // Insert root agent into registry
    res_t res = ik_db_agent_insert(db, agent);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to insert root agent: %s\n", error_message(res.err));
        ck_abort_msg("Failed to setup root agent in registry");
    }
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
    ck_assert_ptr_nonnull(db);
    ck_assert_ptr_nonnull(db->conn);

    // Begin transaction for test isolation
    db_res = ik_test_db_begin(db);
    if (is_err(&db_res)) {
        fprintf(stderr, "Failed to begin transaction: %s\n", error_message(db_res.err));
        ck_abort_msg("Begin transaction failed");
    }

    // Create session
    int64_t session_id = 0;
    db_res = ik_db_session_create(db, &session_id);
    if (is_err(&db_res)) {
        fprintf(stderr, "Failed to create session: %s\n", error_message(db_res.err));
        ck_abort_msg("Session creation failed");
    }

    setup_repl();

    // Update shared context with actual session_id
    repl->shared->session_id = session_id;
}

static void teardown(void)
{
    // Rollback transaction to discard test changes
    if (db != NULL && test_ctx != NULL) {
        ik_test_db_rollback(db);
    }

    // Free everything
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

// Test: displays tree structure with single root agent
START_TEST(test_agents_single_root) {
    res_t res = ik_cmd_agents(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify output in scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST
// Test: current agent marked with *
START_TEST(test_agents_current_marked) {
    res_t res = ik_cmd_agents(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify output exists
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}

END_TEST
// Test: shows status (running/dead)
START_TEST(test_agents_shows_status) {
    res_t res = ik_cmd_agents(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify output exists
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}

END_TEST
// Test: root labeled
START_TEST(test_agents_root_labeled) {
    res_t res = ik_cmd_agents(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify output exists
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}

END_TEST
// Test: indentation reflects depth
START_TEST(test_agents_indentation_depth) {
    // Create child agent
    ik_agent_ctx_t *child = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);
    child->uuid = talloc_strdup(child, "child-uuid-abc");
    child->name = NULL;
    child->parent_uuid = talloc_strdup(child, repl->current->uuid);
    child->created_at = 1234567891;
    child->fork_message_id = 1;

    // Add to agent array
    repl->agents[repl->agent_count++] = child;

    // Insert into registry
    res_t res = ik_db_agent_insert(db, child);
    ck_assert(is_ok(&res));

    // Run command
    res = ik_cmd_agents(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify output exists
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);

    // Line 2 should be the root agent (lines 0,1 are header/blank)
    const char *text;
    size_t length;
    res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &text, &length);
    ck_assert(is_ok(&line_res));

    // Root should start with "* " for current marker
    ck_assert_msg(text[0] == '*', "Root should have * marker");

    // Line 3 should be the child agent with tree prefix
    line_res = ik_scrollback_get_line_text(repl->current->scrollback, 3, &text, &length);
    ck_assert(is_ok(&line_res));

    // Child should have tree prefix "+-- "
    ck_assert_msg(strstr(text, "+--") != NULL, "Child should have +-- tree prefix");

    // Child UUID should start after "  +-- " (6 chars)
    ck_assert_msg(strncmp(text, "  +-- ", 6) == 0, "Child should have '  +-- ' prefix");
}

END_TEST
// Test: non-current root alignment
START_TEST(test_agents_root_alignment) {
    // Create child agent
    ik_agent_ctx_t *child = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);
    child->uuid = talloc_strdup(child, "child-uuid-xyz");
    child->name = NULL;
    child->parent_uuid = talloc_strdup(child, repl->current->uuid);
    child->created_at = 1234567891;
    child->fork_message_id = 1;
    child->scrollback = ik_scrollback_create(child, 80);
    child->shared = repl->shared;

    // Add to agent array
    repl->agents[repl->agent_count++] = child;

    // Insert into registry
    res_t res = ik_db_agent_insert(db, child);
    ck_assert(is_ok(&res));

    // Make child the current agent
    repl->current = child;

    // Run command
    res = ik_cmd_agents(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Line 2 should be the root agent (not current)
    const char *text;
    size_t length;
    res_t line_res = ik_scrollback_get_line_text(child->scrollback, 2, &text, &length);
    ck_assert(is_ok(&line_res));

    // Root should start with "  " (not "*") since child is current
    ck_assert_msg(text[0] == ' ' && text[1] == ' ', "Non-current root should have '  ' prefix");
}

END_TEST
// Test: depth > 1 (grandchild indentation)
START_TEST(test_agents_grandchild_indentation) {
    // Create child agent
    ik_agent_ctx_t *child = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);
    child->uuid = talloc_strdup(child, "child-uuid-abc");
    child->name = NULL;
    child->parent_uuid = talloc_strdup(child, repl->current->uuid);
    child->created_at = 1234567891;
    child->fork_message_id = 1;
    repl->agents[repl->agent_count++] = child;

    res_t res = ik_db_agent_insert(db, child);
    ck_assert(is_ok(&res));

    // Create grandchild agent (depth=2)
    ik_agent_ctx_t *grandchild = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(grandchild);
    grandchild->uuid = talloc_strdup(grandchild, "grandchild-uuid-xyz");
    grandchild->name = NULL;
    grandchild->parent_uuid = talloc_strdup(grandchild, child->uuid);
    grandchild->created_at = 1234567892;
    grandchild->fork_message_id = 2;
    repl->agents[repl->agent_count++] = grandchild;

    res = ik_db_agent_insert(db, grandchild);
    ck_assert(is_ok(&res));

    // Run command
    res = ik_cmd_agents(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Line 4 should be the grandchild (lines 0,1=header, 2=root, 3=child)
    const char *text;
    size_t length;
    res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, 4, &text, &length);
    ck_assert(is_ok(&line_res));

    // Grandchild should have 4 spaces of extra indentation plus "+-- "
    // Expected: "      +-- grandchild-uuid-xyz"
    ck_assert_msg(strncmp(text, "      +-- ", 10) == 0,
                  "Grandchild should have '      +-- ' prefix (4 spaces + tree)");
}

END_TEST
// Test: summary count correct
START_TEST(test_agents_summary_count) {
    // Create child agents - one running, one dead
    ik_agent_ctx_t *child1 = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child1);
    child1->uuid = talloc_strdup(child1, "child1-uuid-def");
    child1->name = NULL;
    child1->parent_uuid = talloc_strdup(child1, repl->current->uuid);
    child1->created_at = 1234567892;
    child1->fork_message_id = 2;
    repl->agents[repl->agent_count++] = child1;

    res_t res = ik_db_agent_insert(db, child1);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child2 = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child2);
    child2->uuid = talloc_strdup(child2, "child2-uuid-ghi");
    child2->name = NULL;
    child2->parent_uuid = talloc_strdup(child2, repl->current->uuid);
    child2->created_at = 1234567893;
    child2->fork_message_id = 3;
    repl->agents[repl->agent_count++] = child2;

    res = ik_db_agent_insert(db, child2);
    ck_assert(is_ok(&res));

    // Mark child2 as dead
    res = ik_db_agent_mark_dead(db, child2->uuid);
    ck_assert(is_ok(&res));

    // Run command
    res = ik_cmd_agents(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify output exists (should show 2 running, 1 dead)
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}

END_TEST

static Suite *agents_suite(void)
{
    Suite *s = suite_create("Agents Command");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_agents_single_root);
    tcase_add_test(tc, test_agents_current_marked);
    tcase_add_test(tc, test_agents_shows_status);
    tcase_add_test(tc, test_agents_root_labeled);
    tcase_add_test(tc, test_agents_indentation_depth);
    tcase_add_test(tc, test_agents_root_alignment);
    tcase_add_test(tc, test_agents_grandchild_indentation);
    tcase_add_test(tc, test_agents_summary_count);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = agents_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands/cmd_agents_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (failed == 0) ? 0 : 1;
}
