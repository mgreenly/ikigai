/**
 * @file request_tools_validation_test.c
 * @brief Tests for model validation and request building (lines 239-289)
 */

#include "../../../src/providers/request.h"
#include "../../../src/agent.h"
#include "../../../src/error.h"
#include "../../../src/shared.h"
#include "../../test_utils.h"

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

/**
 * Test error when model is NULL (line 239 true branch - first condition)
 */
START_TEST(test_null_model_error)
{
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = NULL;
    agent->thinking_level = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, &req);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

/**
 * Test error when model is empty string (line 239 true branch - second condition)
 */
START_TEST(test_empty_model_error)
{
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "");
    agent->thinking_level = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, &req);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

/**
 * Test success when model is valid (line 239 false branch, line 245 false branch)
 */
START_TEST(test_valid_model_success)
{
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, &req);

    // Line 245: is_err(&res) is false - request created successfully
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);
    ck_assert_str_eq(req->model, "gpt-4");
}
END_TEST

/**
 * Test with system message (line 249 true branch)
 */
START_TEST(test_with_system_message)
{
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    agent->shared->cfg->openai_system_message = talloc_strdup(agent->shared->cfg, "Be helpful");

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req->system_prompt);
    ck_assert_str_eq(req->system_prompt, "Be helpful");
}
END_TEST

/**
 * Test with NULL shared context (line 249 - first condition false)
 */
START_TEST(test_null_shared_context)
{
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = NULL;  // NULL shared context
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_null(req->system_prompt);
}
END_TEST

/**
 * Test with NULL config (line 249 - second condition false)
 */
START_TEST(test_null_config)
{
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->shared->cfg = NULL;  // NULL config
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_null(req->system_prompt);
}
END_TEST

/**
 * Test without system message (line 249 - third condition false)
 */
START_TEST(test_without_system_message)
{
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    agent->shared->cfg->openai_system_message = NULL;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_null(req->system_prompt);
}
END_TEST

/**
 * Test with NULL message in array (line 260 continue)
 */
START_TEST(test_skip_null_message)
{
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;

    agent->message_count = 2;
    agent->messages = talloc_array(agent, ik_message_t *, 2);

    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_USER;
    agent->messages[0]->content_count = 1;
    agent->messages[0]->content_blocks = talloc_array(agent->messages[0], ik_content_block_t, 1);
    agent->messages[0]->content_blocks[0].type = IK_CONTENT_TEXT;
    agent->messages[0]->content_blocks[0].data.text.text = talloc_strdup(agent->messages[0], "Hi");

    agent->messages[1] = NULL;  // Should be skipped

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);  // Only 1 message copied
}
END_TEST

static Suite *request_tools_validation_suite(void)
{
    Suite *s = suite_create("Request Tools Validation");

    TCase *tc_validation = tcase_create("Model Validation");
    tcase_set_timeout(tc_validation, 30);
    tcase_add_checked_fixture(tc_validation, setup, teardown);
    tcase_add_test(tc_validation, test_null_model_error);
    tcase_add_test(tc_validation, test_empty_model_error);
    tcase_add_test(tc_validation, test_valid_model_success);
    suite_add_tcase(s, tc_validation);

    TCase *tc_system = tcase_create("System Message");
    tcase_set_timeout(tc_system, 30);
    tcase_add_checked_fixture(tc_system, setup, teardown);
    tcase_add_test(tc_system, test_with_system_message);
    tcase_add_test(tc_system, test_null_shared_context);
    tcase_add_test(tc_system, test_null_config);
    tcase_add_test(tc_system, test_without_system_message);
    suite_add_tcase(s, tc_system);

    TCase *tc_messages = tcase_create("Message Array");
    tcase_set_timeout(tc_messages, 30);
    tcase_add_checked_fixture(tc_messages, setup, teardown);
    tcase_add_test(tc_messages, test_skip_null_message);
    suite_add_tcase(s, tc_messages);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_tools_validation_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
