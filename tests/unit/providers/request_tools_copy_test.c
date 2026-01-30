/**
 * @file request_tools_copy_test.c
 * @brief Tests for message deep copy in request_tools.c - basic content types
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
 * Test copying message with TEXT content block (line 182 true branch)
 */
START_TEST(test_copy_text_message) {
    ik_shared_ctx_t *shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);

    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "claude-sonnet-4-5");
    agent->thinking_level = 0;

    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 1);
    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_USER;
    agent->messages[0]->content_count = 1;
    agent->messages[0]->content_blocks = talloc_array(agent->messages[0], ik_content_block_t, 1);
    agent->messages[0]->content_blocks[0].type = IK_CONTENT_TEXT;
    agent->messages[0]->content_blocks[0].data.text.text = talloc_strdup(agent->messages[0], "Hello");

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);
    ck_assert_int_eq(req->messages[0].content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.text.text, "Hello");
}
END_TEST

/**
 * Test copying message with TOOL_CALL content (lines 192-193 branches)
 */
START_TEST(test_copy_tool_call_message) {
    ik_shared_ctx_t *shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);

    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4o");
    agent->thinking_level = 0;

    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 1);
    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_ASSISTANT;
    agent->messages[0]->content_count = 1;
    agent->messages[0]->content_blocks = talloc_array(agent->messages[0], ik_content_block_t, 1);
    agent->messages[0]->content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    agent->messages[0]->content_blocks[0].data.tool_call.id = talloc_strdup(agent->messages[0], "c1");
    agent->messages[0]->content_blocks[0].data.tool_call.name = talloc_strdup(agent->messages[0], "bash");
    agent->messages[0]->content_blocks[0].data.tool_call.arguments = talloc_strdup(agent->messages[0], "{}");
    agent->messages[0]->content_blocks[0].data.tool_call.thought_signature = NULL;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);
    ck_assert_int_eq(req->messages[0].content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.tool_call.id, "c1");
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.tool_call.name, "bash");
}
END_TEST

/**
 * Test copying message with TOOL_RESULT content (line 202 branches)
 */
START_TEST(test_copy_tool_result_message) {
    ik_shared_ctx_t *shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);

    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gemini-2.0-flash");
    agent->thinking_level = 0;

    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 1);
    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_USER;
    agent->messages[0]->content_count = 1;
    agent->messages[0]->content_blocks = talloc_array(agent->messages[0], ik_content_block_t, 1);
    agent->messages[0]->content_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    agent->messages[0]->content_blocks[0].data.tool_result.tool_call_id =
        talloc_strdup(agent->messages[0], "c2");
    agent->messages[0]->content_blocks[0].data.tool_result.content =
        talloc_strdup(agent->messages[0], "output");
    agent->messages[0]->content_blocks[0].data.tool_result.is_error = false;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);
    ck_assert_int_eq(req->messages[0].content_blocks[0].type, IK_CONTENT_TOOL_RESULT);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.tool_result.tool_call_id, "c2");
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.tool_result.content, "output");
    ck_assert(!req->messages[0].content_blocks[0].data.tool_result.is_error);
}
END_TEST

/**
 * Test copying message with TOOL_RESULT with is_error=true (line 203)
 */
START_TEST(test_copy_tool_result_error_message) {
    ik_shared_ctx_t *shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);

    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gemini-2.0-flash");
    agent->thinking_level = 0;

    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 1);
    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_USER;
    agent->messages[0]->content_count = 1;
    agent->messages[0]->content_blocks = talloc_array(agent->messages[0], ik_content_block_t, 1);
    agent->messages[0]->content_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    agent->messages[0]->content_blocks[0].data.tool_result.tool_call_id =
        talloc_strdup(agent->messages[0], "c3");
    agent->messages[0]->content_blocks[0].data.tool_result.content =
        talloc_strdup(agent->messages[0], "Error: file not found");
    agent->messages[0]->content_blocks[0].data.tool_result.is_error = true;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);
    ck_assert_int_eq(req->messages[0].content_blocks[0].type, IK_CONTENT_TOOL_RESULT);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.tool_result.tool_call_id, "c3");
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.tool_result.content, "Error: file not found");
    ck_assert(req->messages[0].content_blocks[0].data.tool_result.is_error);
}
END_TEST

/**
 * Test copying message with TOOL_CALL with thought_signature (line 59 true branch)
 */
START_TEST(test_copy_tool_call_with_thought_signature) {
    ik_shared_ctx_t *shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);

    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gemini-3-pro-preview");
    agent->thinking_level = 0;

    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 1);
    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_ASSISTANT;
    agent->messages[0]->content_count = 1;
    agent->messages[0]->content_blocks = talloc_array(agent->messages[0], ik_content_block_t, 1);
    agent->messages[0]->content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    agent->messages[0]->content_blocks[0].data.tool_call.id = talloc_strdup(agent->messages[0], "c1");
    agent->messages[0]->content_blocks[0].data.tool_call.name = talloc_strdup(agent->messages[0], "bash");
    agent->messages[0]->content_blocks[0].data.tool_call.arguments = talloc_strdup(agent->messages[0], "{}");
    agent->messages[0]->content_blocks[0].data.tool_call.thought_signature = talloc_strdup(agent->messages[0], "sig123");

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);
    ck_assert_int_eq(req->messages[0].content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.tool_call.thought_signature, "sig123");
}
END_TEST

/**
 * Test copying message with multiple content blocks
 */
START_TEST(test_copy_multiple_content_blocks) {
    ik_shared_ctx_t *shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);

    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "claude-sonnet-4-5");
    agent->thinking_level = 0;

    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 1);
    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_ASSISTANT;
    agent->messages[0]->content_count = 3;
    agent->messages[0]->content_blocks = talloc_zero_array(agent->messages[0], ik_content_block_t, 3);

    // Block 0: text
    agent->messages[0]->content_blocks[0].type = IK_CONTENT_TEXT;
    agent->messages[0]->content_blocks[0].data.text.text = talloc_strdup(agent->messages[0], "Let me run a command");

    // Block 1: tool call
    agent->messages[0]->content_blocks[1].type = IK_CONTENT_TOOL_CALL;
    agent->messages[0]->content_blocks[1].data.tool_call.id = talloc_strdup(agent->messages[0], "tc1");
    agent->messages[0]->content_blocks[1].data.tool_call.name = talloc_strdup(agent->messages[0], "bash");
    agent->messages[0]->content_blocks[1].data.tool_call.arguments = talloc_strdup(agent->messages[0],
                                                                                   "{\"command\":\"ls\"}");
    agent->messages[0]->content_blocks[1].data.tool_call.thought_signature = NULL;

    // Block 2: thinking
    agent->messages[0]->content_blocks[2].type = IK_CONTENT_THINKING;
    agent->messages[0]->content_blocks[2].data.thinking.text = talloc_strdup(agent->messages[0], "Analyzing...");

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);
    ck_assert_int_eq((int)req->messages[0].content_count, 3);

    // Verify all blocks were copied
    ck_assert_int_eq(req->messages[0].content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.text.text, "Let me run a command");

    ck_assert_int_eq(req->messages[0].content_blocks[1].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(req->messages[0].content_blocks[1].data.tool_call.id, "tc1");

    ck_assert_int_eq(req->messages[0].content_blocks[2].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(req->messages[0].content_blocks[2].data.thinking.text, "Analyzing...");
}
END_TEST

static Suite *request_tools_copy_suite(void)
{
    Suite *s = suite_create("Request Tools Copy");

    TCase *tc = tcase_create("Message Deep Copy");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_copy_text_message);
    tcase_add_test(tc, test_copy_tool_call_message);
    tcase_add_test(tc, test_copy_tool_call_with_thought_signature);
    tcase_add_test(tc, test_copy_tool_result_message);
    tcase_add_test(tc, test_copy_tool_result_error_message);
    tcase_add_test(tc, test_copy_multiple_content_blocks);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_tools_copy_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/request_tools_copy_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
