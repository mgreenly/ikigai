/**
 * @file request_chat_coverage_test.c
 * @brief Coverage tests for request_chat.c
 *
 * Tests coverage gaps in ik_openai_serialize_chat_request and helpers.
 */

#include "providers/openai/request.h"
#include "providers/request.h"
#include "message.h"
#include "tool.h"
#include "error.h"
#include "wrapper.h"
#include "vendor/yyjson/yyjson.h"

#include <check.h>
#include <talloc.h>
#include <string.h>

static void *ctx;

static void setup(void)
{
    ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(ctx);
}

/**
 * Test: Serialize request with tools to cover tool serialization branches
 */
START_TEST(test_serialize_with_tools)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(ctx, "gpt-4");
    req->system_prompt = NULL;
    req->messages = NULL;
    req->message_count = 0;
    req->max_output_tokens = 0;
    req->tool_count = 1;
    req->tools = talloc_array(ctx, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(ctx, "test_tool");
    req->tools[0].description = talloc_strdup(ctx, "A test tool");
    req->tools[0].parameters = talloc_strdup(ctx, "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}");
    req->tool_choice_mode = 0; // IK_TOOL_AUTO

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(json);

    /* Verify tools are present */
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_is_arr(tools));
    ck_assert_uint_eq(yyjson_arr_size(tools), 1);

    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Serialize with tool_choice_mode = 1 (IK_TOOL_NONE) to cover line 100
 */
START_TEST(test_tool_choice_none)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(ctx, "gpt-4");
    req->system_prompt = NULL;
    req->messages = NULL;
    req->message_count = 0;
    req->max_output_tokens = 0;
    req->tool_count = 1;
    req->tools = talloc_array(ctx, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(ctx, "test_tool");
    req->tools[0].description = talloc_strdup(ctx, "A test tool");
    req->tools[0].parameters = talloc_strdup(ctx, "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}");
    req->tool_choice_mode = 1; // IK_TOOL_NONE

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(json);

    /* Verify tool_choice is "none" */
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    const char *choice_str = yyjson_get_str(tool_choice);
    ck_assert_str_eq(choice_str, "none");

    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Serialize with tool_choice_mode = 2 (IK_TOOL_REQUIRED) to cover line 106
 */
START_TEST(test_tool_choice_required)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(ctx, "gpt-4");
    req->system_prompt = NULL;
    req->messages = NULL;
    req->message_count = 0;
    req->max_output_tokens = 0;
    req->tool_count = 1;
    req->tools = talloc_array(ctx, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(ctx, "test_tool");
    req->tools[0].description = talloc_strdup(ctx, "A test tool");
    req->tools[0].parameters = talloc_strdup(ctx, "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}");
    req->tool_choice_mode = 2; // IK_TOOL_REQUIRED

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(json);

    /* Verify tool_choice is "required" */
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    const char *choice_str = yyjson_get_str(tool_choice);
    ck_assert_str_eq(choice_str, "required");

    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: Serialize with invalid tool_choice_mode to cover default case (line 110)
 */
START_TEST(test_tool_choice_invalid)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(ctx, "gpt-4");
    req->system_prompt = NULL;
    req->messages = NULL;
    req->message_count = 0;
    req->max_output_tokens = 0;
    req->tool_count = 1;
    req->tools = talloc_array(ctx, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(ctx, "test_tool");
    req->tools[0].description = talloc_strdup(ctx, "A test tool");
    req->tools[0].parameters = talloc_strdup(ctx, "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}");
    req->tool_choice_mode = 999; /* Invalid value to trigger default case */

    char *json = NULL;
    res_t result = ik_openai_serialize_chat_request(ctx, req, false, &json);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(json);

    /* Verify tool_choice defaults to "auto" */
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    const char *choice_str = yyjson_get_str(tool_choice);
    ck_assert_str_eq(choice_str, "auto");

    yyjson_doc_free(doc);
}

END_TEST

static Suite *request_chat_coverage_suite(void)
{
    Suite *s = suite_create("request_chat_coverage");

    TCase *tc_tools = tcase_create("tool_serialization");
    tcase_add_checked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_serialize_with_tools);
    tcase_add_test(tc_tools, test_tool_choice_none);
    tcase_add_test(tc_tools, test_tool_choice_required);
    tcase_add_test(tc_tools, test_tool_choice_invalid);
    suite_add_tcase(s, tc_tools);

    return s;
}

int32_t main(void)
{
    Suite *s = request_chat_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
