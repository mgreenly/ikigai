#include "tests/test_constants.h"
/**
 * @file agent_restore_replay_interrupted_test.c
 * @brief Coverage tests for interrupted message paths and rewind edge cases
 *
 * Tests branch coverage gaps in agent_restore_replay.c:
 * - Lines 28/169: interrupted conversation messages
 * - Lines 98/101/106/108/161: parse_rewind_target_id edge cases
 */

#include "apps/ikigai/repl/agent_restore_replay.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/agent_replay.h"
#include "shared/error.h"
#include "shared/logger.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;
static ik_agent_ctx_t *agent;

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared->logger = ik_logger_create(shared, "/tmp");

    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* Helper: build a single-message replay context */
static ik_replay_context_t *make_replay(const char *kind, const char *content,
                                        const char *data_json, bool interrupted)
{
    ik_replay_context_t *ctx = talloc_zero(test_ctx, ik_replay_context_t);
    ctx->capacity = 1;
    ctx->count    = 1;
    ctx->messages = talloc_array(ctx, ik_msg_t *, 1);

    ik_msg_t *msg = talloc_zero(ctx->messages, ik_msg_t);
    msg->kind       = kind ? talloc_strdup(msg, kind) : NULL;
    msg->content    = content ? talloc_strdup(msg, content) : NULL;
    msg->data_json  = data_json ? talloc_strdup(msg, data_json) : NULL;
    msg->interrupted = interrupted;

    ctx->messages[0] = msg;
    return ctx;
}

/* ----------------------------------------------------------------
 * populate_conversation: interrupted conversation message
 * Covers BRDA:28 branch 3 (A && !B when B is true = interrupted)
 * ---------------------------------------------------------------- */
START_TEST(test_populate_conversation_interrupted_message)
{
    /* User message that is interrupted — must NOT be added to agent */
    ik_replay_context_t *replay = make_replay("user", "Hello", NULL, true);

    size_t before = agent->message_count;
    ik_agent_restore_populate_conversation(agent, replay, agent->shared->logger);

    /* Interrupted message is skipped — count must not increase */
    ck_assert_uint_eq(agent->message_count, before);
}
END_TEST

/* ----------------------------------------------------------------
 * populate_scrollback: interrupted conversation message
 * Covers BRDA:169 branch 3 (running_conv_count not incremented)
 * ---------------------------------------------------------------- */
START_TEST(test_populate_scrollback_interrupted_message)
{
    /* User message that is interrupted — running_conv_count must NOT increase */
    ik_replay_context_t *replay = make_replay("user", "Hello", NULL, true);

    /* Just ensure it runs without crashing */
    ik_agent_restore_populate_scrollback(agent, replay, agent->shared->logger);
}
END_TEST

/* ----------------------------------------------------------------
 * populate_scrollback: rewind with NULL data_json
 * Covers BRDA:98 branch 0 (data_json == NULL → target_id = -1)
 * and BRDA:161 branch 1 (target_id < 0 → skip trimming)
 * ---------------------------------------------------------------- */
START_TEST(test_populate_scrollback_rewind_null_data_json)
{
    ik_replay_context_t *replay = make_replay("rewind", NULL, NULL, false);

    /* Should not crash — target_id < 0 skips trimming */
    ik_agent_restore_populate_scrollback(agent, replay, agent->shared->logger);
}
END_TEST

/* ----------------------------------------------------------------
 * populate_scrollback: rewind with invalid JSON (yyjson_read fails)
 * Covers BRDA:101 branch 0 (doc == NULL → target_id = -1)
 * ---------------------------------------------------------------- */
START_TEST(test_populate_scrollback_rewind_invalid_json)
{
    ik_replay_context_t *replay = make_replay("rewind", NULL, "NOT_VALID_JSON{{{", false);

    ik_agent_restore_populate_scrollback(agent, replay, agent->shared->logger);
}
END_TEST

/* ----------------------------------------------------------------
 * populate_scrollback: rewind with JSON that has no target_message_id
 * Covers BRDA:108 branch 1 (id_val == NULL → condition false)
 * and BRDA:161 branch 1 (target_id stays -1)
 * ---------------------------------------------------------------- */
START_TEST(test_populate_scrollback_rewind_missing_target_id)
{
    ik_replay_context_t *replay = make_replay("rewind", NULL, "{}", false);

    ik_agent_restore_populate_scrollback(agent, replay, agent->shared->logger);
}
END_TEST

/* ----------------------------------------------------------------
 * populate_scrollback: rewind where target_message_id is a string
 * Covers BRDA:108 branch 3 (yyjson_is_int is false → condition false)
 * ---------------------------------------------------------------- */
START_TEST(test_populate_scrollback_rewind_non_int_target_id)
{
    ik_replay_context_t *replay = make_replay("rewind", NULL,
        "{\"target_message_id\":\"not-an-integer\"}", false);

    ik_agent_restore_populate_scrollback(agent, replay, agent->shared->logger);
}
END_TEST

/* ----------------------------------------------------------------
 * Suite
 * ---------------------------------------------------------------- */

static Suite *interrupted_suite(void)
{
    Suite *s = suite_create("agent_restore_replay_interrupted");

    TCase *tc = tcase_create("interrupted_and_rewind");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_populate_conversation_interrupted_message);
    tcase_add_test(tc, test_populate_scrollback_interrupted_message);
    tcase_add_test(tc, test_populate_scrollback_rewind_null_data_json);
    tcase_add_test(tc, test_populate_scrollback_rewind_invalid_json);
    tcase_add_test(tc, test_populate_scrollback_rewind_missing_target_id);
    tcase_add_test(tc, test_populate_scrollback_rewind_non_int_target_id);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite   *s  = interrupted_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/repl/"
        "agent_restore_replay_interrupted_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int32_t nfail = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (nfail == 0) ? 0 : 1;
}
