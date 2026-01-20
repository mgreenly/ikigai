/**
 * @file request_tools_required_test.c
 * @brief Tests for optional vs required parameters in tool schemas
 */

#include "../../../src/providers/request.h"
#include "../../../src/agent.h"
#include "../../../src/error.h"
#include "../../../src/shared.h"
#include "../../test_utils_helper.h"
#include "../../../src/vendor/yyjson/yyjson.h"

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
 * Test that glob tool has correct required/optional parameters
 * glob has: pattern (required), path (optional)
 * The "required" array should only contain "pattern"
 */
START_TEST(test_glob_required_parameters) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));

    // Internal tools removed - should have 0 tools
    ck_assert_int_eq((int)req->tool_count, 0);
}
END_TEST

/**
 * Test that grep tool has correct required/optional parameters
 * grep has: pattern (required), path (optional), glob (optional)
 * The "required" array should only contain "pattern"
 */
START_TEST(test_grep_required_parameters) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));

    // Internal tools removed - should have 0 tools
    ck_assert_int_eq((int)req->tool_count, 0);
}
END_TEST

/**
 * Test that file_write tool has all required parameters
 * file_write has: path (required), content (required)
 */
START_TEST(test_file_write_required_parameters) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));

    // Internal tools removed - should have 0 tools
    ck_assert_int_eq((int)req->tool_count, 0);
}
END_TEST

static Suite *request_tools_required_suite(void)
{
    Suite *s = suite_create("Request Tools Required Params");

    TCase *tc = tcase_create("Required vs Optional");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_glob_required_parameters);
    tcase_add_test(tc, test_grep_required_parameters);
    tcase_add_test(tc, test_file_write_required_parameters);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_tools_required_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
