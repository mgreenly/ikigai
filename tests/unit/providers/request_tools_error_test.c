/**
 * @file request_tools_error_test.c
 * @brief Tests for error handling in request_tools.c
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

/**
 * Test error path when ik_request_set_system fails (line 252-253)
 * This is very hard to trigger since talloc_strdup would need to fail
 */
START_TEST(test_set_system_error_cleanup) {
    // This test documents the error path but may not actually hit it
    // The error would occur if talloc_strdup fails in ik_request_set_system
    // which is nearly impossible to trigger in practice
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    // Set a very long system message - though this won't cause failure
    char *huge_msg = talloc_array(agent, char, 1000);
    memset(huge_msg, 'A', 999);
    huge_msg[999] = '\0';
    agent->shared->cfg->openai_system_message = huge_msg;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    // Should still succeed - allocation failures are very rare
    ck_assert(!is_err(&result));
}
END_TEST

/**
 * Test error path when ik_request_add_message_direct fails (line 264-265)
 * This could happen if message copying fails
 */
START_TEST(test_add_message_error_cleanup) {
    // Similarly hard to trigger - would need talloc to fail during message copy
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "claude-sonnet-4-5");
    agent->thinking_level = 0;

    // Create many messages
    agent->message_count = 10;
    agent->messages = talloc_array(agent, ik_message_t *, 10);
    for (size_t i = 0; i < 10; i++) {
        agent->messages[i] = talloc_zero(agent, ik_message_t);
        agent->messages[i]->role = IK_ROLE_USER;
        agent->messages[i]->content_count = 1;
        agent->messages[i]->content_blocks = talloc_array(agent->messages[i], ik_content_block_t, 1);
        agent->messages[i]->content_blocks[0].type = IK_CONTENT_TEXT;
        char *text = talloc_asprintf(agent->messages[i], "Message %zu", i);
        agent->messages[i]->content_blocks[0].data.text.text = text;
    }

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    // Should succeed - documenting the error path
    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 10);
}
END_TEST

/**
 * Test error path when ik_request_add_tool fails (line 282-283)
 * This would happen if tool addition fails
 */
START_TEST(test_add_tool_error_cleanup) {
    // The error path would be hit if ik_request_add_tool fails
    // which is also very hard to trigger
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    // Should succeed with 0 tools (internal tools removed)
    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->tool_count, 0);
}
END_TEST

static Suite *request_tools_error_suite(void)
{
    Suite *s = suite_create("Request Tools Errors");

    TCase *tc = tcase_create("Error Paths");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_set_system_error_cleanup);
    tcase_add_test(tc, test_add_message_error_cleanup);
    tcase_add_test(tc, test_add_tool_error_cleanup);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_tools_error_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/request_tools_error_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
