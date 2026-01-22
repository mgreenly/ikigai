/**
 * @file request_tools_copy_thinking_test.c
 * @brief Tests for message deep copy of thinking content in request_tools.c
 */

#include "../../../src/providers/request.h"
#include "../../../src/agent.h"
#include "../../../src/error.h"
#include "../../../src/shared.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx = NULL;

static void setup(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
    }
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
}

/**
 * Test copying message with THINKING content
 */
START_TEST(test_copy_thinking_message) {
    ik_shared_ctx_t *shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);

    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "o1-preview");
    agent->thinking_level = 1;

    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 1);
    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_ASSISTANT;
    agent->messages[0]->content_count = 1;
    agent->messages[0]->content_blocks = talloc_array(agent->messages[0], ik_content_block_t, 1);
    agent->messages[0]->content_blocks[0].type = IK_CONTENT_THINKING;
    agent->messages[0]->content_blocks[0].data.thinking.text =
        talloc_strdup(agent->messages[0], "Thinking...");
    agent->messages[0]->content_blocks[0].data.thinking.signature = NULL;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);
    ck_assert_int_eq(req->messages[0].content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.thinking.text, "Thinking...");
}
END_TEST

/**
 * Test copying message with THINKING content and signature
 */
START_TEST(test_copy_thinking_with_signature) {
    ik_shared_ctx_t *shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);

    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "claude-sonnet-4-5");
    agent->thinking_level = 1;

    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 1);
    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_ASSISTANT;
    agent->messages[0]->content_count = 1;
    agent->messages[0]->content_blocks = talloc_array(agent->messages[0], ik_content_block_t, 1);
    agent->messages[0]->content_blocks[0].type = IK_CONTENT_THINKING;
    agent->messages[0]->content_blocks[0].data.thinking.text =
        talloc_strdup(agent->messages[0], "Let me analyze...");
    agent->messages[0]->content_blocks[0].data.thinking.signature =
        talloc_strdup(agent->messages[0], "EqQBCgIYAhIM...");

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);
    ck_assert_int_eq(req->messages[0].content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.thinking.text, "Let me analyze...");
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.thinking.signature, "EqQBCgIYAhIM...");
}
END_TEST

/**
 * Test copying message with THINKING content and NULL signature
 */
START_TEST(test_copy_thinking_null_signature) {
    ik_shared_ctx_t *shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);

    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "claude-sonnet-4-5");
    agent->thinking_level = 1;

    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 1);
    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_ASSISTANT;
    agent->messages[0]->content_count = 1;
    agent->messages[0]->content_blocks = talloc_array(agent->messages[0], ik_content_block_t, 1);
    agent->messages[0]->content_blocks[0].type = IK_CONTENT_THINKING;
    agent->messages[0]->content_blocks[0].data.thinking.text =
        talloc_strdup(agent->messages[0], "Thinking without signature...");
    agent->messages[0]->content_blocks[0].data.thinking.signature = NULL;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);
    ck_assert_int_eq(req->messages[0].content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.thinking.text, "Thinking without signature...");
    ck_assert_ptr_null(req->messages[0].content_blocks[0].data.thinking.signature);
}
END_TEST

/**
 * Test copying message with REDACTED_THINKING content
 */
START_TEST(test_copy_redacted_thinking) {
    ik_shared_ctx_t *shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);

    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "claude-sonnet-4-5");
    agent->thinking_level = 1;

    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 1);
    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_ASSISTANT;
    agent->messages[0]->content_count = 1;
    agent->messages[0]->content_blocks = talloc_array(agent->messages[0], ik_content_block_t, 1);
    agent->messages[0]->content_blocks[0].type = IK_CONTENT_REDACTED_THINKING;
    agent->messages[0]->content_blocks[0].data.redacted_thinking.data =
        talloc_strdup(agent->messages[0], "EmwKAhgBEgy...");

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);
    ck_assert_int_eq(req->messages[0].content_blocks[0].type, IK_CONTENT_REDACTED_THINKING);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.redacted_thinking.data, "EmwKAhgBEgy...");
}
END_TEST

static Suite *request_tools_copy_thinking_suite(void)
{
    Suite *s = suite_create("Request Tools Copy Thinking");

    TCase *tc = tcase_create("Thinking Message Deep Copy");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_copy_thinking_message);
    tcase_add_test(tc, test_copy_thinking_with_signature);
    tcase_add_test(tc, test_copy_thinking_null_signature);
    tcase_add_test(tc, test_copy_redacted_thinking);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_tools_copy_thinking_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/request_tools_copy_thinking_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
