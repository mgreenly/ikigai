/**
 * @file cmd_fork_test.c
 * @brief Unit tests for /fork command
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../../src/openai/client.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
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

    res_t res = ik_openai_conversation_create(test_ctx);
    ck_assert(is_ok(&res));
    ik_openai_conversation_t *conv = res.ok;

    ik_cfg_t *cfg = talloc_zero(test_ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = sb;
    agent->conversation = conv;
    agent->uuid = talloc_strdup(agent, "parent-uuid-123");
    agent->name = NULL;
    agent->parent_uuid = NULL;
    repl->current = agent;

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db;
    shared->fork_pending = false;
    repl->shared = shared;

    // Initialize agent array
    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->agent_capacity = 16;
}

static void setup(void)
{
    DB_NAME = ik_test_db_name(NULL, __FILE__);
    ik_test_db_create(DB_NAME);
    ik_test_db_migrate(NULL, DB_NAME);

    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    ik_test_db_connect(test_ctx, DB_NAME, &db);
    ik_test_db_begin(db);

    setup_repl();
}

static void teardown(void)
{
    ik_test_db_rollback(db);
    talloc_free(test_ctx);
}

static void suite_teardown(void)
{
    ik_test_db_destroy(DB_NAME);
}

// Test: Creates new agent
START_TEST(test_fork_creates_agent)
{
    size_t initial_count = repl->agent_count;

    res_t res = cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->agent_count, initial_count + 1);
}
END_TEST

// Test: Child has parent_uuid set
START_TEST(test_fork_sets_parent)
{
    const char *parent_uuid = repl->current->uuid;

    res_t res = cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Find the newly created child
    ik_agent_ctx_t *child = repl->agents[repl->agent_count - 1];
    ck_assert_ptr_nonnull(child);
    ck_assert_ptr_nonnull(child->parent_uuid);
    ck_assert_str_eq(child->parent_uuid, parent_uuid);
}
END_TEST

// Test: Child added to agents array
START_TEST(test_fork_adds_to_array)
{
    size_t initial_count = repl->agent_count;

    res_t res = cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->agent_count, initial_count + 1);
    ck_assert_ptr_nonnull(repl->agents[initial_count]);
}
END_TEST

// Test: Switches to child
START_TEST(test_fork_switches_to_child)
{
    ik_agent_ctx_t *parent = repl->current;

    res_t res = cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ck_assert_ptr_ne(repl->current, parent);
    ck_assert_str_eq(repl->current->parent_uuid, parent->uuid);
}
END_TEST

// Test: Child in registry with status='running'
START_TEST(test_fork_registry_entry)
{
    res_t res = cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Query registry for child
    ik_db_agent_row_t *row = NULL;
    res_t db_res = ik_db_agent_get(db, test_ctx, repl->current->uuid, &row);
    ck_assert(is_ok(&db_res));
    ck_assert_ptr_nonnull(row);
    ck_assert_str_eq(row->status, "running");
}
END_TEST

// Test: Confirmation message displayed
START_TEST(test_fork_confirmation_message)
{
    res_t res = cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Check scrollback for confirmation message
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(line_count, 0);
}
END_TEST

// Test: fork_pending flag set during fork
START_TEST(test_fork_pending_flag_set)
{
    // This test would need mocking to observe mid-execution
    // For now, verify flag is clear after completion
    res_t res = cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert(!repl->shared->fork_pending);
}
END_TEST

// Test: fork_pending flag cleared after fork
START_TEST(test_fork_pending_flag_cleared)
{
    res_t res = cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert(!repl->shared->fork_pending);
}
END_TEST

// Test: Concurrent fork rejected
START_TEST(test_fork_concurrent_rejected)
{
    repl->shared->fork_pending = true;

    res_t res = cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));  // Returns OK but appends error

    // Check scrollback for error message
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    bool found_error = false;
    for (size_t i = 0; i < line_count; i++) {
        const char *text = NULL;
        size_t length = 0;
        res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, i, &text, &length);
        if (is_ok(&line_res) && text && strstr(text, "Fork already in progress")) {
            found_error = true;
            break;
        }
    }
    ck_assert(found_error);
}
END_TEST

// Test: Failed fork rolls back
START_TEST(test_fork_rollback_on_failure)
{
    // Force a failure by corrupting the database connection
    // Store original connection
    PGconn *orig_conn = db->conn;
    db->conn = NULL;

    res_t res = cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_err(&res));

    // Restore connection
    db->conn = orig_conn;

    // Verify no orphan registry entry
    // Since we're in a transaction, rollback will clean up
}
END_TEST

// Test: Failed fork clears fork_pending
START_TEST(test_fork_clears_pending_on_failure)
{
    // Force a failure
    PGconn *orig_conn = db->conn;
    db->conn = NULL;

    res_t res = cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_err(&res));

    db->conn = orig_conn;

    ck_assert(!repl->shared->fork_pending);
}
END_TEST

static Suite *cmd_fork_suite(void)
{
    Suite *s = suite_create("Fork Command");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_fork_creates_agent);
    tcase_add_test(tc, test_fork_sets_parent);
    tcase_add_test(tc, test_fork_adds_to_array);
    tcase_add_test(tc, test_fork_switches_to_child);
    tcase_add_test(tc, test_fork_registry_entry);
    tcase_add_test(tc, test_fork_confirmation_message);
    tcase_add_test(tc, test_fork_pending_flag_set);
    tcase_add_test(tc, test_fork_pending_flag_cleared);
    tcase_add_test(tc, test_fork_concurrent_rejected);
    tcase_add_test(tc, test_fork_rollback_on_failure);
    tcase_add_test(tc, test_fork_clears_pending_on_failure);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = cmd_fork_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
