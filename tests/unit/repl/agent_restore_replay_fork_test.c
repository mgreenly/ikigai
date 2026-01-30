/**
 * @file agent_restore_replay_fork_test.c
 * @brief Tests for agent restore fork event replay
 */

#include "../../../src/repl/agent_restore_replay.h"
#include "../../../src/agent.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/agent_replay.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/logger.h"
#include "../../../src/msg.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../test_utils_helper.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

static TALLOC_CTX *test_ctx;
static ik_shared_ctx_t *shared_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->logger = ik_logger_create(shared_ctx, "/tmp");
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Helper: Create minimal agent for testing
static ik_agent_ctx_t *create_test_agent(void)
{
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared_ctx, NULL, &agent);
    ck_assert(is_ok(&res));
    return agent;
}

// Helper: Create replay context with fork message
static ik_replay_context_t *create_fork_replay(const char *data_json)
{
    ik_replay_context_t *replay_ctx = talloc_zero(test_ctx, ik_replay_context_t);
    replay_ctx->capacity = 1;
    replay_ctx->count = 1;
    replay_ctx->messages = talloc_array(replay_ctx, ik_msg_t *, 1);

    ik_msg_t *msg = talloc_zero(replay_ctx->messages, ik_msg_t);
    msg->kind = talloc_strdup(msg, "fork");
    msg->content = NULL;
    msg->data_json = talloc_strdup(msg, data_json);

    replay_ctx->messages[0] = msg;
    return replay_ctx;
}

// Test: fork event with role=child and pinned_paths
START_TEST(test_fork_event_with_pinned_paths) {
    ik_agent_ctx_t *agent = create_test_agent();

    const char *fork_json =
        "{\"role\":\"child\","
        "\"pinned_paths\":[\"file1.txt\",\"file2.txt\"],"
        "\"toolset_filter\":[\"tool1\",\"tool2\"]}";

    ik_replay_context_t *replay_ctx = create_fork_replay(fork_json);

    // Verify initial state
    ck_assert_int_eq((int)agent->pinned_count, 0);
    ck_assert_int_eq((int)agent->toolset_count, 0);

    // Replay fork event
    ik_agent_restore_replay_command_effects(agent, replay_ctx->messages[0],
                                            shared_ctx->logger);

    // Verify pinned paths were restored
    ck_assert_int_eq((int)agent->pinned_count, 2);
    ck_assert_str_eq(agent->pinned_paths[0], "file1.txt");
    ck_assert_str_eq(agent->pinned_paths[1], "file2.txt");

    // Verify toolset filter was restored
    ck_assert_int_eq((int)agent->toolset_count, 2);
    ck_assert_str_eq(agent->toolset_filter[0], "tool1");
    ck_assert_str_eq(agent->toolset_filter[1], "tool2");
}
END_TEST

// Test: fork event with role=child but pinned_paths is not an array
START_TEST(test_fork_event_pinned_paths_not_array) {
    ik_agent_ctx_t *agent = create_test_agent();

    const char *fork_json =
        "{\"role\":\"child\","
        "\"pinned_paths\":\"not_an_array\"}";

    ik_replay_context_t *replay_ctx = create_fork_replay(fork_json);

    // Replay fork event - should not crash
    ik_agent_restore_replay_command_effects(agent, replay_ctx->messages[0],
                                            shared_ctx->logger);

    // Pinned paths should remain empty (invalid data ignored)
    ck_assert_int_eq((int)agent->pinned_count, 0);
}
END_TEST

// Test: fork event with role=child but toolset_filter is not an array
START_TEST(test_fork_event_toolset_filter_not_array) {
    ik_agent_ctx_t *agent = create_test_agent();

    const char *fork_json =
        "{\"role\":\"child\","
        "\"toolset_filter\":\"not_an_array\"}";

    ik_replay_context_t *replay_ctx = create_fork_replay(fork_json);

    // Replay fork event - should not crash
    ik_agent_restore_replay_command_effects(agent, replay_ctx->messages[0],
                                            shared_ctx->logger);

    // Toolset filter should remain empty (invalid data ignored)
    ck_assert_int_eq((int)agent->toolset_count, 0);
}
END_TEST

// Test: fork event with role=parent (should be ignored)
START_TEST(test_fork_event_parent_role) {
    ik_agent_ctx_t *agent = create_test_agent();

    const char *fork_json =
        "{\"role\":\"parent\","
        "\"pinned_paths\":[\"file1.txt\"]}";

    ik_replay_context_t *replay_ctx = create_fork_replay(fork_json);

    // Replay fork event
    ik_agent_restore_replay_command_effects(agent, replay_ctx->messages[0],
                                            shared_ctx->logger);

    // Agent state should be unchanged (parent role ignored)
    ck_assert_int_eq((int)agent->pinned_count, 0);
}
END_TEST

// Test: fork event with empty pinned_paths array
START_TEST(test_fork_event_empty_pinned_paths) {
    ik_agent_ctx_t *agent = create_test_agent();

    const char *fork_json =
        "{\"role\":\"child\","
        "\"pinned_paths\":[]}";

    ik_replay_context_t *replay_ctx = create_fork_replay(fork_json);

    // Replay fork event
    ik_agent_restore_replay_command_effects(agent, replay_ctx->messages[0],
                                            shared_ctx->logger);

    // Pinned count should remain 0
    ck_assert_int_eq((int)agent->pinned_count, 0);
}
END_TEST

// Test: fork event with empty toolset_filter array
START_TEST(test_fork_event_empty_toolset_filter) {
    ik_agent_ctx_t *agent = create_test_agent();

    const char *fork_json =
        "{\"role\":\"child\","
        "\"toolset_filter\":[]}";

    ik_replay_context_t *replay_ctx = create_fork_replay(fork_json);

    // Replay fork event
    ik_agent_restore_replay_command_effects(agent, replay_ctx->messages[0],
                                            shared_ctx->logger);

    // Toolset count should remain 0
    ck_assert_int_eq((int)agent->toolset_count, 0);
}
END_TEST

static Suite *agent_restore_replay_fork_suite(void)
{
    Suite *s = suite_create("Agent Restore Replay - Fork Events");

    TCase *tc = tcase_create("Fork Events");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_fork_event_with_pinned_paths);
    tcase_add_test(tc, test_fork_event_pinned_paths_not_array);
    tcase_add_test(tc, test_fork_event_toolset_filter_not_array);
    tcase_add_test(tc, test_fork_event_parent_role);
    tcase_add_test(tc, test_fork_event_empty_pinned_paths);
    tcase_add_test(tc, test_fork_event_empty_toolset_filter);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = agent_restore_replay_fork_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/agent_restore_replay_fork_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
