/**
 * @file agent_restore_replay_mocked_test.c
 * @brief Unit tests for agent_restore_replay.c with mocked yyjson functions
 *
 * Tests error paths by mocking yyjson wrapper functions to return NULL.
 */

#include "../../../src/repl/agent_restore_replay.h"

#include "../../../src/agent.h"
#include "../../../src/db/agent_replay.h"
#include "../../../src/error.h"
#include "../../../src/logger.h"
#include "../../../src/msg.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../../src/vendor/yyjson/yyjson.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;
static ik_agent_ctx_t *agent;
static ik_replay_context_t *replay_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    // Create minimal shared context
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared->logger = ik_logger_create(shared, "/tmp");

    // Create agent
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));

    // Create minimal replay context with a command message
    replay_ctx = talloc_zero(test_ctx, ik_replay_context_t);
    replay_ctx->capacity = 1;
    replay_ctx->count = 1;
    replay_ctx->messages = talloc_array(replay_ctx, ik_msg_t *, 1);

    ik_msg_t *msg = talloc_zero(replay_ctx->messages, ik_msg_t);
    msg->kind = talloc_strdup(msg, "command");
    msg->content = NULL;
    msg->data_json = talloc_strdup(msg, "{\"command\":\"model\",\"args\":\"gpt-4\"}");

    replay_ctx->messages[0] = msg;
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Mock yyjson_doc_get_root_ to return NULL
yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc)
{
    (void)doc;
    return NULL;
}

// Test: yyjson_doc_get_root_ returns NULL in replay_command_effects
START_TEST(test_replay_command_effects_null_root) {
    // Populate scrollback triggers replay_command_effects for command messages
    ik_agent_restore_populate_scrollback(agent, replay_ctx, agent->shared->logger);

    // Agent provider/model should not be set (early return on NULL root)
    ck_assert_ptr_null(agent->provider);
    ck_assert_ptr_null(agent->model);
}

END_TEST

static Suite *agent_restore_replay_mocked_suite(void)
{
    Suite *s = suite_create("Agent Restore Replay (Mocked)");
    TCase *tc_mocked = tcase_create("Mocked");
    tcase_set_timeout(tc_mocked, 30);
    tcase_set_timeout(tc_mocked, 30);
    tcase_set_timeout(tc_mocked, 30);
    tcase_set_timeout(tc_mocked, 30);
    tcase_set_timeout(tc_mocked, 30);

    tcase_add_checked_fixture(tc_mocked, setup, teardown);
    tcase_add_test(tc_mocked, test_replay_command_effects_null_root);

    suite_add_tcase(s, tc_mocked);
    return s;
}

int main(void)
{
    Suite *s = agent_restore_replay_mocked_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
