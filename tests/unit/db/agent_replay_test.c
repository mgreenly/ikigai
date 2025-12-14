/**
 * @file agent_replay_test.c
 * @brief Tests for agent startup replay functionality
 *
 * Tests the "walk backwards, play forwards" algorithm for reconstructing
 * agent history from the database.
 */

#include "../../../src/db/agent.h"
#include "../../../src/db/agent_replay.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/db/replay.h"
#include "../../../src/agent.h"
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
} while(0)

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

// ========== find_clear Tests ==========

// Test: find_clear returns 0 when no clear exists
START_TEST(test_find_clear_no_clear)
{
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("agent-no-clear", NULL, time(NULL), 0);

    // Insert some messages but no clear
    insert_message("agent-no-clear", "user", "Hello");
    insert_message("agent-no-clear", "assistant", "Hi");

    // Find clear - should return 0
    int64_t clear_id = -1;
    res_t res = ik_agent_find_clear(db, "agent-no-clear", 0, &clear_id);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(clear_id, 0);
}
END_TEST

// Test: find_clear returns correct message ID
START_TEST(test_find_clear_returns_id)
{
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("agent-with-clear", NULL, time(NULL), 0);

    // Insert messages with a clear
    insert_message("agent-with-clear", "user", "Before clear");
    insert_message("agent-with-clear", "clear", NULL);
    insert_message("agent-with-clear", "user", "After clear");

    // Find clear - should return the clear's ID
    int64_t clear_id = 0;
    res_t res = ik_agent_find_clear(db, "agent-with-clear", 0, &clear_id);
    ck_assert(is_ok(&res));
    ck_assert(clear_id > 0);
}
END_TEST

// Test: find_clear respects max_id limit
START_TEST(test_find_clear_respects_max_id)
{
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("agent-clear-limit", NULL, time(NULL), 0);

    // Insert messages: user, clear, user, clear, user
    insert_message("agent-clear-limit", "user", "First");
    insert_message("agent-clear-limit", "clear", NULL);  // This is the earlier clear
    insert_message("agent-clear-limit", "user", "Second");
    insert_message("agent-clear-limit", "clear", NULL);  // This is the later clear
    insert_message("agent-clear-limit", "user", "Third");

    // Find clear with no limit - should return later clear
    int64_t clear_id_all = 0;
    res_t res1 = ik_agent_find_clear(db, "agent-clear-limit", 0, &clear_id_all);
    ck_assert(is_ok(&res1));
    ck_assert(clear_id_all > 0);

    // Find clear with max_id = earlier clear id + 1
    // First get the earlier clear id by querying with a limit between them
    int64_t earlier_clear_id = 0;
    res_t res2 = ik_agent_find_clear(db, "agent-clear-limit", clear_id_all - 1, &earlier_clear_id);
    ck_assert(is_ok(&res2));

    // If earlier clear exists, it should be less than the later clear
    if (earlier_clear_id > 0) {
        ck_assert(earlier_clear_id < clear_id_all);
    }
}
END_TEST

// ========== build_replay_ranges Tests ==========

// Test: range building for root agent (single range with end_id=0)
START_TEST(test_build_ranges_root_agent)
{
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

// ========== query_range Tests ==========

// Test: query_range returns correct message subset
START_TEST(test_query_range_subset)
{
    SKIP_IF_NO_DB();

    // Insert agent with multiple messages
    insert_agent("query-test-agent", NULL, time(NULL), 0);
    insert_message("query-test-agent", "user", "Msg 1");
    insert_message("query-test-agent", "assistant", "Msg 2");
    insert_message("query-test-agent", "user", "Msg 3");
    insert_message("query-test-agent", "assistant", "Msg 4");

    // Get message IDs
    int64_t last_id = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "query-test-agent", &last_id);
    ck_assert(is_ok(&res));

    // Query all messages (start_id=0, end_id=0)
    ik_replay_range_t range = {
        .agent_uuid = talloc_strdup(test_ctx, "query-test-agent"),
        .start_id = 0,
        .end_id = 0
    };

    ik_message_t **messages = NULL;
    size_t msg_count = 0;
    res = ik_agent_query_range(db, test_ctx, &range, &messages, &msg_count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)msg_count, 4);
}
END_TEST

// Test: query_range with start_id=0 returns from beginning
START_TEST(test_query_range_from_beginning)
{
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("query-begin-agent", NULL, time(NULL), 0);
    insert_message("query-begin-agent", "user", "First");
    insert_message("query-begin-agent", "assistant", "Second");

    // Query from beginning (start_id=0)
    ik_replay_range_t range = {
        .agent_uuid = talloc_strdup(test_ctx, "query-begin-agent"),
        .start_id = 0,
        .end_id = 0
    };

    ik_message_t **messages = NULL;
    size_t msg_count = 0;
    res_t res = ik_agent_query_range(db, test_ctx, &range, &messages, &msg_count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)msg_count, 2);

    // Verify first message
    ck_assert_str_eq(messages[0]->content, "First");
}
END_TEST

// Test: query_range with end_id=0 returns to end
START_TEST(test_query_range_to_end)
{
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("query-end-agent", NULL, time(NULL), 0);
    insert_message("query-end-agent", "user", "One");
    insert_message("query-end-agent", "assistant", "Two");
    insert_message("query-end-agent", "user", "Three");

    // Get first message ID
    ik_replay_range_t range_all = {
        .agent_uuid = talloc_strdup(test_ctx, "query-end-agent"),
        .start_id = 0,
        .end_id = 0
    };

    ik_message_t **all_msgs = NULL;
    size_t all_count = 0;
    res_t res = ik_agent_query_range(db, test_ctx, &range_all, &all_msgs, &all_count);
    ck_assert(is_ok(&res));
    ck_assert(all_count >= 3);

    // Query starting after first message with end_id=0 (to end)
    int64_t first_id = all_msgs[0]->id;
    ik_replay_range_t range = {
        .agent_uuid = talloc_strdup(test_ctx, "query-end-agent"),
        .start_id = first_id,
        .end_id = 0
    };

    ik_message_t **messages = NULL;
    size_t msg_count = 0;
    res = ik_agent_query_range(db, test_ctx, &range, &messages, &msg_count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)msg_count, 2);  // Two and Three

    // Verify messages
    ck_assert_str_eq(messages[0]->content, "Two");
    ck_assert_str_eq(messages[1]->content, "Three");
}
END_TEST

// ========== Full Replay Tests ==========

// Test: full replay produces correct chronological order
START_TEST(test_replay_chronological_order)
{
    SKIP_IF_NO_DB();

    // Insert root agent with messages
    insert_agent("replay-root", NULL, 1000, 0);
    insert_message("replay-root", "user", "Root-1");
    insert_message("replay-root", "assistant", "Root-2");

    int64_t fork_id = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "replay-root", &fork_id);
    ck_assert(is_ok(&res));

    // Insert child
    insert_agent("replay-child", "replay-root", 2000, fork_id);
    insert_message("replay-child", "user", "Child-1");
    insert_message("replay-child", "assistant", "Child-2");

    // Replay child's history
    ik_replay_context_t *ctx = NULL;
    res = ik_agent_replay_history(db, test_ctx, "replay-child", &ctx);
    ck_assert(is_ok(&res));
    ck_assert(ctx != NULL);

    // Should have 4 messages in chronological order
    ck_assert_int_eq((int)ctx->count, 4);
    ck_assert_str_eq(ctx->messages[0]->content, "Root-1");
    ck_assert_str_eq(ctx->messages[1]->content, "Root-2");
    ck_assert_str_eq(ctx->messages[2]->content, "Child-1");
    ck_assert_str_eq(ctx->messages[3]->content, "Child-2");
}
END_TEST

// Test: replay handles agent with no history
START_TEST(test_replay_empty_history)
{
    SKIP_IF_NO_DB();

    // Insert agent with no messages
    insert_agent("empty-agent", NULL, time(NULL), 0);

    // Replay should succeed with empty context
    ik_replay_context_t *ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, "empty-agent", &ctx);
    ck_assert(is_ok(&res));
    ck_assert(ctx != NULL);
    ck_assert_int_eq((int)ctx->count, 0);
}
END_TEST

// Test: replay handles deep ancestry (4+ levels)
START_TEST(test_replay_deep_ancestry)
{
    SKIP_IF_NO_DB();

    // Build 4-level hierarchy: great-grandparent -> grandparent -> parent -> child
    insert_agent("ggp", NULL, 1000, 0);
    insert_message("ggp", "user", "GGP");

    int64_t fork1 = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "ggp", &fork1);
    ck_assert(is_ok(&res));

    insert_agent("gp-deep", "ggp", 2000, fork1);
    insert_message("gp-deep", "user", "GP");

    int64_t fork2 = 0;
    res = ik_db_agent_get_last_message_id(db, "gp-deep", &fork2);
    ck_assert(is_ok(&res));

    insert_agent("p-deep", "gp-deep", 3000, fork2);
    insert_message("p-deep", "user", "P");

    int64_t fork3 = 0;
    res = ik_db_agent_get_last_message_id(db, "p-deep", &fork3);
    ck_assert(is_ok(&res));

    insert_agent("c-deep", "p-deep", 4000, fork3);
    insert_message("c-deep", "user", "C");

    // Replay child's history
    ik_replay_context_t *ctx = NULL;
    res = ik_agent_replay_history(db, test_ctx, "c-deep", &ctx);
    ck_assert(is_ok(&res));
    ck_assert(ctx != NULL);

    // Should have 4 messages from all 4 levels
    ck_assert_int_eq((int)ctx->count, 4);
    ck_assert_str_eq(ctx->messages[0]->content, "GGP");
    ck_assert_str_eq(ctx->messages[1]->content, "GP");
    ck_assert_str_eq(ctx->messages[2]->content, "P");
    ck_assert_str_eq(ctx->messages[3]->content, "C");
}
END_TEST

// ========== Suite Configuration ==========

static Suite *agent_replay_suite(void)
{
    Suite *s = suite_create("Agent Replay");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    // find_clear tests
    tcase_add_test(tc_core, test_find_clear_no_clear);
    tcase_add_test(tc_core, test_find_clear_returns_id);
    tcase_add_test(tc_core, test_find_clear_respects_max_id);

    // build_replay_ranges tests
    tcase_add_test(tc_core, test_build_ranges_root_agent);
    tcase_add_test(tc_core, test_build_ranges_child);
    tcase_add_test(tc_core, test_build_ranges_grandchild);
    tcase_add_test(tc_core, test_build_ranges_stops_at_clear);

    // query_range tests
    tcase_add_test(tc_core, test_query_range_subset);
    tcase_add_test(tc_core, test_query_range_from_beginning);
    tcase_add_test(tc_core, test_query_range_to_end);

    // Full replay tests
    tcase_add_test(tc_core, test_replay_chronological_order);
    tcase_add_test(tc_core, test_replay_empty_history);
    tcase_add_test(tc_core, test_replay_deep_ancestry);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_replay_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
