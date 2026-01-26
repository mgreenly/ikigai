/**
 * @file pin_fork_replay_test.c
 * @brief Integration test for pin replay during agent restoration
 */

#include "../../src/agent.h"
#include "../../src/commands.h"
#include "../../src/commands_pin.h"
#include "../../src/config.h"
#include "../../src/db/agent.h"
#include "../../src/db/agent_replay.h"
#include "../../src/db/connection.h"
#include "../../src/db/session.h"
#include "../../src/error.h"
#include "../../src/repl.h"
#include "../../src/repl/agent_restore.h"
#include "../../src/repl/agent_restore_replay.h"
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

// Test: Fork with pins, restore child, verify pins replayed
START_TEST(test_fork_with_pins_replay) {
    // Create a session
    int64_t session_id = 0;
    res_t session_res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&session_res));
    repl->shared->session_id = session_id;

    ik_agent_ctx_t *parent = repl->current;

    // Pin two documents to the parent
    parent->pinned_paths = talloc_array(parent, char *, 2);
    ck_assert_ptr_nonnull(parent->pinned_paths);
    parent->pinned_paths[0] = talloc_strdup(parent, "/path/to/doc1.md");
    parent->pinned_paths[1] = talloc_strdup(parent, "/path/to/doc2.md");
    parent->pinned_count = 2;

    // Fork to create a child
    res_t fork_res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&fork_res));

    ik_agent_ctx_t *child = repl->current;
    const char *child_uuid = talloc_strdup(test_ctx, child->uuid);

    // Verify child inherited pins in memory
    ck_assert(child->pinned_count == 2);
    ck_assert_str_eq(child->pinned_paths[0], "/path/to/doc1.md");
    ck_assert_str_eq(child->pinned_paths[1], "/path/to/doc2.md");

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

    // 3. Replay history to rebuild pins
    ik_replay_context_t *replay_ctx = NULL;
    res_t replay_res = ik_agent_replay_history(db, test_ctx, child_uuid, &replay_ctx);
    ck_assert(is_ok(&replay_res));
    ck_assert_ptr_nonnull(replay_ctx);

    // 4. Populate scrollback (this triggers pin replay)
    ik_agent_restore_populate_scrollback(restored_child, replay_ctx, repl->shared->logger);

    // 5. Verify pins were replayed correctly
    ck_assert(restored_child->pinned_count == 2);
    ck_assert_ptr_nonnull(restored_child->pinned_paths);
    ck_assert_str_eq(restored_child->pinned_paths[0], "/path/to/doc1.md");
    ck_assert_str_eq(restored_child->pinned_paths[1], "/path/to/doc2.md");
}
END_TEST

// Test: Pin and unpin operations, then restore and verify replay
START_TEST(test_pin_unpin_replay) {
    // Create a session
    int64_t session_id = 0;
    res_t session_res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&session_res));
    repl->shared->session_id = session_id;

    ik_agent_ctx_t *agent = repl->current;
    const char *agent_uuid = talloc_strdup(test_ctx, agent->uuid);

    // Pin three documents
    res_t pin_res1 = ik_cmd_pin(test_ctx, repl, "/path/to/doc1.md");
    ck_assert(is_ok(&pin_res1));
    res_t pin_res2 = ik_cmd_pin(test_ctx, repl, "/path/to/doc2.md");
    ck_assert(is_ok(&pin_res2));
    res_t pin_res3 = ik_cmd_pin(test_ctx, repl, "/path/to/doc3.md");
    ck_assert(is_ok(&pin_res3));

    // Verify agent has 3 pins
    ck_assert(agent->pinned_count == 3);

    // Unpin the middle document
    res_t unpin_res = ik_cmd_unpin(test_ctx, repl, "/path/to/doc2.md");
    ck_assert(is_ok(&unpin_res));

    // Verify agent now has 2 pins (doc1 and doc3)
    ck_assert(agent->pinned_count == 2);
    ck_assert_str_eq(agent->pinned_paths[0], "/path/to/doc1.md");
    ck_assert_str_eq(agent->pinned_paths[1], "/path/to/doc3.md");

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

    // Replay history to rebuild pins
    ik_replay_context_t *replay_ctx = NULL;
    res_t replay_res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&replay_res));
    ck_assert_ptr_nonnull(replay_ctx);

    // Populate scrollback
    ik_agent_restore_populate_scrollback(restored_agent, replay_ctx, repl->shared->logger);

    // Replay pins (independent of clear boundaries)
    res_t pin_replay_res = ik_agent_replay_pins(db, restored_agent);
    ck_assert(is_ok(&pin_replay_res));

    // Verify pins were replayed correctly (should have doc1 and doc3, not doc2)
    ck_assert(restored_agent->pinned_count == 2);
    ck_assert_ptr_nonnull(restored_agent->pinned_paths);
    ck_assert_str_eq(restored_agent->pinned_paths[0], "/path/to/doc1.md");
    ck_assert_str_eq(restored_agent->pinned_paths[1], "/path/to/doc3.md");
}
END_TEST

static Suite *pin_fork_replay_suite(void)
{
    Suite *s = suite_create("Pin Fork Replay");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_fork_with_pins_replay);
    tcase_add_test(tc, test_pin_unpin_replay);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = pin_fork_replay_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/integration/pin_fork_replay_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
