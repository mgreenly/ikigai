/**
 * @file agent_replay_ranges_test.c
 * @brief Tests for agent replay range building
 *
 * Tests the build_replay_ranges function that constructs the replay plan
 * by walking backwards through agent ancestry.
 */

#include "../../../src/db/agent.h"
#include "../../../src/db/agent_replay.h"
#include "../../../src/db/message.h"
#include "../../../src/db/replay.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../test_utils.h"
#include <check.h>
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

// Per-test teardown: Rollback and disconnect
static void test_teardown(void)
{
    if (db != NULL) {
        ik_test_db_rollback(db);
    }
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
    db = NULL;
}

// Skip macro for tests when DB not available
#define SKIP_IF_NO_DB() do { \
            if (!db_available) { \
                return; \
            } \
} while (0)

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

// ========== build_replay_ranges Tests ==========

// Test: range building for root agent (single range with end_id=0)
START_TEST(test_build_ranges_root_agent) {
    SKIP_IF_NO_DB();

    // Insert root agent with no parent
    insert_agent("root-agent", NULL, time(NULL), 0);

    // Insert some messages
    insert_message("root-agent", "user", "Hello");
    insert_message("root-agent", "assistant", "Hi");

    // Build ranges
    ik_replay_range_t *ranges = NULL;
    size_t count = 0;
    res_t res = ik_agent_build_replay_ranges(db, test_ctx, "root-agent", &ranges, &count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)count, 1);
    ck_assert(ranges != NULL);

    // Single range should be: {root-agent, 0, 0}
    ck_assert_str_eq(ranges[0].agent_uuid, "root-agent");
    ck_assert_int_eq(ranges[0].start_id, 0);
    ck_assert_int_eq(ranges[0].end_id, 0);
}
END_TEST
// Test: range building for child (two ranges)
START_TEST(test_build_ranges_child)
{
    SKIP_IF_NO_DB();

    // Insert root agent
    insert_agent("parent-for-child", NULL, 1000, 0);

    // Insert parent messages
    insert_message("parent-for-child", "user", "Parent msg 1");
    insert_message("parent-for-child", "assistant", "Parent msg 2");

    // Get last message ID for fork point
    int64_t fork_msg_id = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "parent-for-child", &fork_msg_id);
    ck_assert(is_ok(&res));
    ck_assert(fork_msg_id > 0);

    // Insert child agent forked at parent's last message
    insert_agent("child-agent", "parent-for-child", 2000, fork_msg_id);

    // Insert child messages
    insert_message("child-agent", "user", "Child msg 1");
    insert_message("child-agent", "assistant", "Child msg 2");

    // Build ranges for child
    ik_replay_range_t *ranges = NULL;
    size_t count = 0;
    res = ik_agent_build_replay_ranges(db, test_ctx, "child-agent", &ranges, &count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)count, 2);

    // First range: parent (chronological order after reverse)
    ck_assert_str_eq(ranges[0].agent_uuid, "parent-for-child");
    ck_assert_int_eq(ranges[0].start_id, 0);
    ck_assert_int_eq(ranges[0].end_id, fork_msg_id);

    // Second range: child
    ck_assert_str_eq(ranges[1].agent_uuid, "child-agent");
    ck_assert_int_eq(ranges[1].start_id, 0);
    ck_assert_int_eq(ranges[1].end_id, 0);
}

END_TEST
// Test: range building for grandchild (three ranges)
START_TEST(test_build_ranges_grandchild)
{
    SKIP_IF_NO_DB();

    // Insert grandparent
    insert_agent("grandparent", NULL, 1000, 0);
    insert_message("grandparent", "user", "GP msg");

    int64_t gp_fork = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "grandparent", &gp_fork);
    ck_assert(is_ok(&res));

    // Insert parent
    insert_agent("parent-mid", "grandparent", 2000, gp_fork);
    insert_message("parent-mid", "user", "Parent msg");

    int64_t p_fork = 0;
    res = ik_db_agent_get_last_message_id(db, "parent-mid", &p_fork);
    ck_assert(is_ok(&res));

    // Insert grandchild
    insert_agent("grandchild", "parent-mid", 3000, p_fork);
    insert_message("grandchild", "user", "GC msg");

    // Build ranges for grandchild
    ik_replay_range_t *ranges = NULL;
    size_t count = 0;
    res = ik_agent_build_replay_ranges(db, test_ctx, "grandchild", &ranges, &count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)count, 3);

    // Check chronological order: grandparent, parent, grandchild
    ck_assert_str_eq(ranges[0].agent_uuid, "grandparent");
    ck_assert_str_eq(ranges[1].agent_uuid, "parent-mid");
    ck_assert_str_eq(ranges[2].agent_uuid, "grandchild");
}

END_TEST
// Test: range building stops at clear event
START_TEST(test_build_ranges_stops_at_clear)
{
    SKIP_IF_NO_DB();

    // Insert root agent with a clear in history
    insert_agent("agent-with-clear-range", NULL, time(NULL), 0);
    insert_message("agent-with-clear-range", "user", "Before clear");
    insert_message("agent-with-clear-range", "clear", NULL);
    insert_message("agent-with-clear-range", "user", "After clear");

    // Build ranges
    ik_replay_range_t *ranges = NULL;
    size_t count = 0;
    res_t res = ik_agent_build_replay_ranges(db, test_ctx, "agent-with-clear-range", &ranges, &count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)count, 1);

    // Range should start after the clear
    ck_assert(ranges[0].start_id > 0);
    ck_assert_int_eq(ranges[0].end_id, 0);
}

END_TEST

// ========== Suite Configuration ==========

static Suite *agent_replay_ranges_suite(void)
{
    Suite *s = suite_create("Agent Replay Ranges");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    // build_replay_ranges tests
    tcase_add_test(tc_core, test_build_ranges_root_agent);
    tcase_add_test(tc_core, test_build_ranges_child);
    tcase_add_test(tc_core, test_build_ranges_grandchild);
    tcase_add_test(tc_core, test_build_ranges_stops_at_clear);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_replay_ranges_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
