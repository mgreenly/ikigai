/**
 * @file request_tools_context_window_test.c
 * @brief Tests that ik_request_build_from_conversation() respects context_start_index
 */

#include "tests/test_constants.h"

#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/token_cache.h"
#include "shared/error.h"
#include "apps/ikigai/shared.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;
static ik_shared_ctx_t *shared_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

static void add_msg(ik_agent_ctx_t *agent, ik_role_t role, const char *text)
{
    size_t idx = agent->message_count++;
    agent->messages = talloc_realloc(agent, agent->messages, ik_message_t *,
                                     (unsigned int)agent->message_count);
    ik_message_t *m = talloc_zero(agent, ik_message_t);
    m->role = role;
    m->content_count = 1;
    m->content_blocks = talloc_array(m, ik_content_block_t, 1);
    m->content_blocks[0].type = IK_CONTENT_TEXT;
    m->content_blocks[0].data.text.text = talloc_strdup(m, text);
    agent->messages[idx] = m;
}

/* With NULL token_cache, all messages are included (backwards compatible) */
START_TEST(test_null_token_cache_includes_all_messages)
{
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->token_cache = NULL;

    add_msg(agent, IK_ROLE_USER, "user message 1");
    add_msg(agent, IK_ROLE_ASSISTANT, "assistant message 1");
    add_msg(agent, IK_ROLE_USER, "user message 2");
    add_msg(agent, IK_ROLE_ASSISTANT, "assistant message 2");

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(req);
    ck_assert_int_eq((int)req->message_count, 4);

    talloc_free(req);
}
END_TEST

/* With context_start_index > 0, only messages from that index onward are included */
START_TEST(test_context_start_index_excludes_pruned_messages)
{
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");

    /* 4 messages: [user1(0), assistant1(1), user2(2), assistant2(3)] */
    add_msg(agent, IK_ROLE_USER, "user message 1");
    add_msg(agent, IK_ROLE_ASSISTANT, "assistant message 1");
    add_msg(agent, IK_ROLE_USER, "user message 2");
    add_msg(agent, IK_ROLE_ASSISTANT, "assistant message 2");

    /* Create cache with 2 turns and prune the oldest */
    agent->token_cache = ik_token_cache_create(test_ctx, agent);
    ik_token_cache_add_turn(agent->token_cache);
    ik_token_cache_add_turn(agent->token_cache);
    ik_token_cache_prune_oldest_turn(agent->token_cache);

    /* Pruning advances context_start_index to index of user2 = 2 */
    ck_assert_int_eq((int)ik_token_cache_get_context_start_index(agent->token_cache), 2);

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(req);
    /* Only messages at index 2 and 3: user2, assistant2 */
    ck_assert_int_eq((int)req->message_count, 2);
    ck_assert_int_eq((int)req->messages[0].role, (int)IK_ROLE_USER);
    ck_assert_int_eq((int)req->messages[1].role, (int)IK_ROLE_ASSISTANT);

    talloc_free(req);
}
END_TEST

/* With context_start_index == 0, all messages are included */
START_TEST(test_zero_context_start_index_includes_all_messages)
{
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");

    add_msg(agent, IK_ROLE_USER, "user message 1");
    add_msg(agent, IK_ROLE_ASSISTANT, "assistant message 1");

    /* Cache with no pruning: context_start_index remains 0 */
    agent->token_cache = ik_token_cache_create(test_ctx, agent);
    ik_token_cache_add_turn(agent->token_cache);

    ck_assert_int_eq((int)ik_token_cache_get_context_start_index(agent->token_cache), 0);

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(req);
    ck_assert_int_eq((int)req->message_count, 2);

    talloc_free(req);
}
END_TEST

static Suite *request_tools_context_window_suite(void)
{
    Suite *s = suite_create("Request Tools Context Window");

    TCase *tc = tcase_create("Context Start Index");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_null_token_cache_includes_all_messages);
    tcase_add_test(tc, test_context_start_index_excludes_pruned_messages);
    tcase_add_test(tc, test_zero_context_start_index_includes_all_messages);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_tools_context_window_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/request_tools_context_window_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
