/**
 * @file request_tools_required_test.c
 * @brief Tests for optional vs required parameters in tool schemas
 */

#include "../../../src/providers/request.h"
#include "../../../src/agent.h"
#include "../../../src/error.h"
#include "../../../src/shared.h"
#include "../../test_utils.h"
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
    res_t result = ik_request_build_from_conversation(test_ctx, agent, &req);

    ck_assert(!is_err(&result));

    // Find glob tool (should be first)
    ik_tool_def_t *glob_tool = NULL;
    for (size_t i = 0; i < req->tool_count; i++) {
        if (strcmp(req->tools[i].name, "glob") == 0) {
            glob_tool = &req->tools[i];
            break;
        }
    }

    ck_assert_ptr_nonnull(glob_tool);
    ck_assert_ptr_nonnull(glob_tool->parameters);

    // Parse the JSON
    yyjson_doc *doc = yyjson_read(glob_tool->parameters, strlen(glob_tool->parameters), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *required = yyjson_obj_get(root, "required");
    ck_assert_ptr_nonnull(required);
    ck_assert(yyjson_is_arr(required));

    // Should have only 1 required param: "pattern"
    size_t required_count = yyjson_arr_size(required);
    ck_assert_int_eq((int)required_count, 1);

    yyjson_val *first_required = yyjson_arr_get(required, 0);
    ck_assert_str_eq(yyjson_get_str(first_required), "pattern");

    yyjson_doc_free(doc);
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
    res_t result = ik_request_build_from_conversation(test_ctx, agent, &req);

    ck_assert(!is_err(&result));

    // Find grep tool
    ik_tool_def_t *grep_tool = NULL;
    for (size_t i = 0; i < req->tool_count; i++) {
        if (strcmp(req->tools[i].name, "grep") == 0) {
            grep_tool = &req->tools[i];
            break;
        }
    }

    ck_assert_ptr_nonnull(grep_tool);
    ck_assert_ptr_nonnull(grep_tool->parameters);

    // Parse the JSON
    yyjson_doc *doc = yyjson_read(grep_tool->parameters, strlen(grep_tool->parameters), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *required = yyjson_obj_get(root, "required");
    ck_assert_ptr_nonnull(required);

    // Should have only 1 required param: "pattern"
    size_t required_count = yyjson_arr_size(required);
    ck_assert_int_eq((int)required_count, 1);

    yyjson_val *first_required = yyjson_arr_get(required, 0);
    ck_assert_str_eq(yyjson_get_str(first_required), "pattern");

    yyjson_doc_free(doc);
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
    res_t result = ik_request_build_from_conversation(test_ctx, agent, &req);

    ck_assert(!is_err(&result));

    // Find file_write tool
    ik_tool_def_t *file_write_tool = NULL;
    for (size_t i = 0; i < req->tool_count; i++) {
        if (strcmp(req->tools[i].name, "file_write") == 0) {
            file_write_tool = &req->tools[i];
            break;
        }
    }

    ck_assert_ptr_nonnull(file_write_tool);
    ck_assert_ptr_nonnull(file_write_tool->parameters);

    // Parse the JSON
    yyjson_doc *doc = yyjson_read(file_write_tool->parameters, strlen(file_write_tool->parameters), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *required = yyjson_obj_get(root, "required");
    ck_assert_ptr_nonnull(required);

    // Should have 2 required params: "path" and "content"
    size_t required_count = yyjson_arr_size(required);
    ck_assert_int_eq((int)required_count, 2);

    yyjson_doc_free(doc);
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
