/**
 * @file request_tools_system_prompt_test.c
 * @brief Coverage tests for request_tools.c system prompt paths
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

// Test empty system_prompt branch (lines 134-136)
START_TEST(test_empty_system_prompt) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    // Set config to empty string to trigger the empty system_prompt branch
    agent->shared->cfg->openai_system_message = talloc_strdup(agent->shared->cfg, "");

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    // Should succeed - empty system message is valid
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(req);

    talloc_free(req);
}
END_TEST

// Test NULL system_prompt (should use hardcoded default)
START_TEST(test_null_system_prompt) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;
    agent->pinned_count = 0;
    agent->shared->paths = NULL;

    // Set config to NULL to trigger NULL system_prompt path
    agent->shared->cfg->openai_system_message = NULL;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    // Should succeed - will use hardcoded default
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(req);

    talloc_free(req);
}
END_TEST

static Suite *request_tools_system_prompt_suite(void)
{
    Suite *s = suite_create("Request Tools System Prompt");

    TCase *tc = tcase_create("System Prompt Paths");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_empty_system_prompt);
    tcase_add_test(tc, test_null_system_prompt);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_tools_system_prompt_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/request_tools_system_prompt_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
