#include "tests/test_constants.h"
/**
 * @file request_responses_reasoning_tools_test.c
 * @brief Reasoning and tool tests for OpenAI Responses API
 */

#include "apps/ikigai/providers/openai/request.h"
#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/error.h"
#include "vendor/yyjson/yyjson.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/* Test fixture */
static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Reasoning Configuration Tests
 * ================================================================ */

START_TEST(test_serialize_reasoning_low) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_set_thinking(req, IK_THINKING_LOW, false);
    ik_request_add_message(req, IK_ROLE_USER, "Solve this problem");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
    ck_assert_ptr_nonnull(reasoning);

    yyjson_val *effort = yyjson_obj_get(reasoning, "effort");
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(yyjson_get_str(effort), "low");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_reasoning_medium) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1-mini", &req);
    ck_assert(!is_err(&create_result));

    ik_request_set_thinking(req, IK_THINKING_MED, false);
    ik_request_add_message(req, IK_ROLE_USER, "Complex task");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
    ck_assert_ptr_nonnull(reasoning);

    yyjson_val *effort = yyjson_obj_get(reasoning, "effort");
    ck_assert_str_eq(yyjson_get_str(effort), "medium");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_reasoning_high) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o3-mini", &req);
    ck_assert(!is_err(&create_result));

    ik_request_set_thinking(req, IK_THINKING_HIGH, false);
    ik_request_add_message(req, IK_ROLE_USER, "Very hard problem");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
    ck_assert_ptr_nonnull(reasoning);

    yyjson_val *effort = yyjson_obj_get(reasoning, "effort");
    ck_assert_str_eq(yyjson_get_str(effort), "high");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_reasoning_none) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_set_thinking(req, IK_THINKING_NONE, false);
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // For o1 models, NONE maps to "low" effort
    yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
    ck_assert_ptr_nonnull(reasoning);
    yyjson_val *effort = yyjson_obj_get(reasoning, "effort");
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(yyjson_get_str(effort), "low");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_gpt5_reasoning_none) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "gpt-5", &req);
    ck_assert(!is_err(&create_result));

    // GPT-5 with NONE should omit reasoning field
    ik_request_set_thinking(req, IK_THINKING_NONE, false);
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // No reasoning field should be present for gpt-5 with NONE
    yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
    ck_assert_ptr_null(reasoning);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_non_reasoning_model_with_thinking) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "gpt-4o", &req);
    ck_assert(!is_err(&create_result));

    // Non-reasoning model, thinking level set (but should be ignored)
    ik_request_set_thinking(req, IK_THINKING_HIGH, false);
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // No reasoning field for non-reasoning models
    yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
    ck_assert_ptr_null(reasoning);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Tool Definition Tests
 * ================================================================ */

START_TEST(test_serialize_single_tool) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Use a tool");

    const char *params = "{\"type\":\"object\",\"properties\":{\"x\":{\"type\":\"number\"}}}";
    ik_request_add_tool(req, "calculator", "Performs calculations", params, true);

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_is_arr(tools));
    ck_assert_int_eq((int)yyjson_arr_size(tools), 1);

    yyjson_val *tool = yyjson_arr_get_first(tools);
    ck_assert_ptr_nonnull(tool);

    yyjson_val *type = yyjson_obj_get(tool, "type");
    ck_assert_str_eq(yyjson_get_str(type), "function");

    yyjson_val *name = yyjson_obj_get(tool, "name");
    ck_assert_str_eq(yyjson_get_str(name), "calculator");

    yyjson_val *desc = yyjson_obj_get(tool, "description");
    ck_assert_str_eq(yyjson_get_str(desc), "Performs calculations");

    yyjson_val *strict = yyjson_obj_get(tool, "strict");
    ck_assert(yyjson_get_bool(strict));

    // Verify additionalProperties: false is set (required for strict mode)
    yyjson_val *params_obj = yyjson_obj_get(tool, "parameters");
    ck_assert_ptr_nonnull(params_obj);
    yyjson_val *additional_props = yyjson_obj_get(params_obj, "additionalProperties");
    ck_assert_ptr_nonnull(additional_props);
    ck_assert(!yyjson_get_bool(additional_props));

    // Verify all properties are in required array (strict mode requirement)
    yyjson_val *required_arr = yyjson_obj_get(params_obj, "required");
    ck_assert_ptr_nonnull(required_arr);
    ck_assert(yyjson_is_arr(required_arr));
    // The schema has property "x", so required should contain "x"
    ck_assert_int_eq((int)yyjson_arr_size(required_arr), 1);

    yyjson_val *func = yyjson_obj_get(tool, "function");
    ck_assert_ptr_null(func);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_multiple_tools) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Use tools");

    const char *params = "{\"type\":\"object\"}";
    ik_request_add_tool(req, "tool1", "First tool", params, true);
    ik_request_add_tool(req, "tool2", "Second tool", params, false);

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert_int_eq((int)yyjson_arr_size(tools), 2);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_tool_choice_auto) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    const char *params = "{\"type\":\"object\"}";
    ik_request_add_tool(req, "test_tool", "Test", params, true);

    req->tool_choice_mode = 0; // IK_TOOL_AUTO

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(choice);
    ck_assert_str_eq(yyjson_get_str(choice), "auto");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_tool_choice_none) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    const char *params = "{\"type\":\"object\"}";
    ik_request_add_tool(req, "test_tool", "Test", params, true);

    req->tool_choice_mode = 1; // IK_TOOL_NONE

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(choice);
    ck_assert_str_eq(yyjson_get_str(choice), "none");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_tool_choice_required) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    const char *params = "{\"type\":\"object\"}";
    ik_request_add_tool(req, "test_tool", "Test", params, true);

    req->tool_choice_mode = 2; // IK_TOOL_REQUIRED

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(choice);
    ck_assert_str_eq(yyjson_get_str(choice), "required");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_tool_choice_unknown) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    const char *params = "{\"type\":\"object\"}";
    ik_request_add_tool(req, "test_tool", "Test", params, true);

    req->tool_choice_mode = 999; // Unknown mode

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Unknown mode defaults to "auto"
    yyjson_val *choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(choice);
    ck_assert_str_eq(yyjson_get_str(choice), "auto");

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *request_responses_reasoning_tools_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Reasoning and Tools");

    TCase *tc_reasoning = tcase_create("Reasoning Configuration");
    tcase_set_timeout(tc_reasoning, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_reasoning, setup, teardown);
    tcase_add_test(tc_reasoning, test_serialize_reasoning_low);
    tcase_add_test(tc_reasoning, test_serialize_reasoning_medium);
    tcase_add_test(tc_reasoning, test_serialize_reasoning_high);
    tcase_add_test(tc_reasoning, test_serialize_reasoning_none);
    tcase_add_test(tc_reasoning, test_serialize_gpt5_reasoning_none);
    tcase_add_test(tc_reasoning, test_serialize_non_reasoning_model_with_thinking);
    suite_add_tcase(s, tc_reasoning);

    TCase *tc_tools = tcase_create("Tool Definitions");
    tcase_set_timeout(tc_tools, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_serialize_single_tool);
    tcase_add_test(tc_tools, test_serialize_multiple_tools);
    tcase_add_test(tc_tools, test_serialize_tool_choice_auto);
    tcase_add_test(tc_tools, test_serialize_tool_choice_none);
    tcase_add_test(tc_tools, test_serialize_tool_choice_required);
    tcase_add_test(tc_tools, test_serialize_tool_choice_unknown);
    suite_add_tcase(s, tc_tools);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = request_responses_reasoning_tools_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/request_responses_reasoning_tools_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
