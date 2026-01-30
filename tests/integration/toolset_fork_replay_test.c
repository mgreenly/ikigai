/**
 * @file toolset_fork_replay_test.c
 * @brief Integration test for toolset replay during agent restoration
 */

#include "../../src/agent.h"
#include "../../src/commands.h"
#include "../../src/commands_toolset.h"
#include "../../src/config.h"
#include "../../src/db/agent.h"
#include "../../src/db/agent_replay.h"
#include "../../src/db/connection.h"
#include "../../src/db/session.h"
#include "../../src/error.h"
#include "../../src/repl.h"
#include "../../src/repl/agent_restore.h"
#include "../../src/repl/agent_restore_replay.h"
#include "../../src/repl/agent_restore_replay_toolset.h"
#include "../../src/scrollback.h"
#include "../../src/shared.h"
#include "../test_utils_helper.h"

#include <check.h>
#include <inttypes.h>
#include <talloc.h>

// Test fixtures
static const char *DB_NAME;
static ik_db_ctx_t *db;
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;

static bool suite_setup(void)
{
    DB_NAME = ik_test_db_name(NULL, __FILE__);
    res_t res = ik_test_db_create(DB_NAME);
    if (is_err(&res)) {
        talloc_free(res.err);
        return false;
    }
    res = ik_test_db_migrate(NULL, DB_NAME);
    if (is_err(&res)) {
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
    ck_assert(is_ok(&db_res));

    ik_scrollback_t *sb = ik_scrollback_create(test_ctx, 80);
    ck_assert_ptr_nonnull(sb);

    ik_config_t *cfg = talloc_zero(test_ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db;
    atomic_init(&shared->fork_pending, false);
    repl->shared = shared;

    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agent_count = 0;
    repl->agent_capacity = 16;

    // Create and register parent agent
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = sb;
    agent->uuid = talloc_strdup(agent, "parent-uuid");
    agent->name = NULL;
    agent->parent_uuid = NULL;
    agent->created_at = 1234567890;
    agent->fork_message_id = 0;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->shared = shared;

    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->current = agent;

    res_t res = ik_db_agent_insert(db, agent);
    ck_assert(is_ok(&res));
}

static void teardown(void)
{
    if (db != NULL && test_ctx != NULL) {
        ik_test_db_truncate_all(db);
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

// Test: Fork with toolset filter, restore child, verify filter replayed
START_TEST(test_fork_with_toolset_replay) {
    // Create a session
    int64_t session_id = 0;
    res_t session_res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&session_res));
    repl->shared->session_id = session_id;

    ik_agent_ctx_t *parent = repl->current;

    // Set toolset filter on parent
    parent->toolset_filter = talloc_array(parent, char *, 2);
    ck_assert_ptr_nonnull(parent->toolset_filter);
    parent->toolset_filter[0] = talloc_strdup(parent, "bash");
    parent->toolset_filter[1] = talloc_strdup(parent, "file_read");
    parent->toolset_count = 2;

    // Fork to create a child
    res_t fork_res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&fork_res));

    ik_agent_ctx_t *child = repl->current;
    const char *child_uuid = talloc_strdup(test_ctx, child->uuid);

    // Verify child inherited toolset filter in memory
    ck_assert(child->toolset_count == 2);
    ck_assert_str_eq(child->toolset_filter[0], "bash");
    ck_assert_str_eq(child->toolset_filter[1], "file_read");

    // Now simulate agent restoration by:
    // 1. Getting child agent row from database
    ik_db_agent_row_t *child_row = NULL;
    res_t get_res = ik_db_agent_get(db, test_ctx, child_uuid, &child_row);
    ck_assert(is_ok(&get_res));
    ck_assert_ptr_nonnull(child_row);

    // 2. Restore child agent from row
    ik_agent_ctx_t *restored_child = NULL;
    res_t restore_res = ik_agent_restore(test_ctx, repl->shared, child_row, &restored_child);
    ck_assert(is_ok(&restore_res));
    ck_assert_ptr_nonnull(restored_child);

    // 3. Replay history to rebuild toolset filter
    ik_replay_context_t *replay_ctx = NULL;
    res_t replay_res = ik_agent_replay_history(db, test_ctx, child_uuid, &replay_ctx);
    ck_assert(is_ok(&replay_res));
    ck_assert_ptr_nonnull(replay_ctx);

    // 4. Populate scrollback (this triggers toolset replay)
    ik_agent_restore_populate_scrollback(restored_child, replay_ctx, repl->shared->logger);

    // 5. Verify toolset filter was replayed correctly
    ck_assert(restored_child->toolset_count == 2);
    ck_assert_ptr_nonnull(restored_child->toolset_filter);
    ck_assert_str_eq(restored_child->toolset_filter[0], "bash");
    ck_assert_str_eq(restored_child->toolset_filter[1], "file_read");
}
END_TEST

// Test: Set toolset operations, then restore and verify replay
START_TEST(test_toolset_replay) {
    // Create a session
    int64_t session_id = 0;
    res_t session_res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&session_res));
    repl->shared->session_id = session_id;

    ik_agent_ctx_t *agent = repl->current;
    const char *agent_uuid = talloc_strdup(test_ctx, agent->uuid);

    // Set toolset to three tools
    res_t set_res1 = ik_cmd_toolset(test_ctx, repl, "bash, file_read, glob");
    ck_assert(is_ok(&set_res1));

    // Verify agent has 3 tools
    ck_assert(agent->toolset_count == 3);
    ck_assert_str_eq(agent->toolset_filter[0], "bash");
    ck_assert_str_eq(agent->toolset_filter[1], "file_read");
    ck_assert_str_eq(agent->toolset_filter[2], "glob");

    // Replace with different toolset (replacement, not additive)
    res_t set_res2 = ik_cmd_toolset(test_ctx, repl, "file_write, grep");
    ck_assert(is_ok(&set_res2));

    // Verify agent now has 2 tools (replaced, not added)
    ck_assert(agent->toolset_count == 2);
    ck_assert_str_eq(agent->toolset_filter[0], "file_write");
    ck_assert_str_eq(agent->toolset_filter[1], "grep");

    // Get agent row from database
    ik_db_agent_row_t *agent_row = NULL;
    res_t get_res = ik_db_agent_get(db, test_ctx, agent_uuid, &agent_row);
    ck_assert(is_ok(&get_res));
    ck_assert_ptr_nonnull(agent_row);

    // Restore agent from row
    ik_agent_ctx_t *restored_agent = NULL;
    res_t restore_res = ik_agent_restore(test_ctx, repl->shared, agent_row, &restored_agent);
    ck_assert(is_ok(&restore_res));
    ck_assert_ptr_nonnull(restored_agent);

    // Replay history to rebuild toolset filter
    ik_replay_context_t *replay_ctx = NULL;
    res_t replay_res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&replay_res));
    ck_assert_ptr_nonnull(replay_ctx);

    // Populate scrollback
    ik_agent_restore_populate_scrollback(restored_agent, replay_ctx, repl->shared->logger);

    // Replay toolset (independent of clear boundaries)
    res_t toolset_replay_res = ik_agent_replay_toolset(db, restored_agent);
    ck_assert(is_ok(&toolset_replay_res));

    // Verify toolset filter was replayed correctly (should have latest: file_write, grep)
    ck_assert(restored_agent->toolset_count == 2);
    ck_assert_ptr_nonnull(restored_agent->toolset_filter);
    ck_assert_str_eq(restored_agent->toolset_filter[0], "file_write");
    ck_assert_str_eq(restored_agent->toolset_filter[1], "grep");
}
END_TEST

static Suite *toolset_fork_replay_suite(void)
{
    Suite *s = suite_create("Toolset Fork Replay");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_fork_with_toolset_replay);
    tcase_add_test(tc, test_toolset_replay);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = toolset_fork_replay_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/integration/toolset_fork_replay_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
