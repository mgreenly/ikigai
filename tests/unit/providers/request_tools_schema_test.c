/**
 * @file request_tools_schema_test.c
 * @brief Tests for tool schema building in request_tools.c (lines 92-107)
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
 * Test build_from_conversation which internally calls build_tool_parameters_json
 * This covers lines 92-107 (the function that builds JSON tool schemas)
 */
START_TEST(test_build_tool_parameters_json_via_conversation)
{
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);

    // Should have built 5 standard tools (glob, file_read, grep, file_write, bash)
    ck_assert_int_eq((int)req->tool_count, 5);

    // Verify the tools have parameter schemas (JSON strings)
    for (size_t i = 0; i < 5; i++) {
        ck_assert_ptr_nonnull(req->tools[i].parameters);
        // Verify it's valid JSON by checking for basic structure
        ck_assert_ptr_nonnull(strstr(req->tools[i].parameters, "\"type\""));
        ck_assert_ptr_nonnull(strstr(req->tools[i].parameters, "\"properties\""));
    }
}
END_TEST

static Suite *request_tools_schema_suite(void)
{
    Suite *s = suite_create("Request Tools Schema");

    TCase *tc = tcase_create("Tool Schema Building");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_build_tool_parameters_json_via_conversation);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_tools_schema_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
