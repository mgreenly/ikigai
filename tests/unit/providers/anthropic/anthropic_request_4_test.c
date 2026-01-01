/**
 * @file anthropic_request_test_4.c
 * @brief Unit tests for Anthropic request serialization - Part 4: Tool, Header, and Error tests
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/request.h"
#include "providers/provider.h"
#include "vendor/yyjson/yyjson.h"

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
 * Helper Functions
 * ================================================================ */

static ik_request_t *create_basic_request(TALLOC_CTX *ctx)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(req, "claude-3-5-sonnet-20241022");
    req->max_output_tokens = 1024;
    req->thinking.level = IK_THINKING_NONE;

    // Add one simple message
    req->message_count = 1;
    req->messages = talloc_array(req, ik_message_t, 1);
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks = talloc_array(req, ik_content_block_t, 1);
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello");

    return req;
}

/* ================================================================
 * Tool Definition Tests
 * ================================================================ */

START_TEST(test_tools_none) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 0;
    req->tools = NULL;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tools = yyjson_obj_get(root, "tools");

    // Should not have tools field
    ck_assert_ptr_null(tools);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_tools_single) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather for a city");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\",\"properties\":{}}");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tools = yyjson_obj_get(root, "tools");

    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_is_arr(tools));
    ck_assert(yyjson_arr_size(tools) == 1);

    yyjson_val *tool = yyjson_arr_get(tools, 0);
    yyjson_val *name = yyjson_obj_get(tool, "name");
    yyjson_val *description = yyjson_obj_get(tool, "description");
    yyjson_val *input_schema = yyjson_obj_get(tool, "input_schema");

    ck_assert_str_eq(yyjson_get_str(name), "get_weather");
    ck_assert_str_eq(yyjson_get_str(description), "Get weather for a city");
    ck_assert(yyjson_is_obj(input_schema));

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_tools_multiple) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 2;
    req->tools = talloc_array(req, ik_tool_def_t, 2);
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\"}");
    req->tools[1].name = talloc_strdup(req, "get_time");
    req->tools[1].description = talloc_strdup(req, "Get time");
    req->tools[1].parameters = talloc_strdup(req, "{\"type\":\"object\"}");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tools = yyjson_obj_get(root, "tools");

    ck_assert(yyjson_arr_size(tools) == 2);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_tool_choice_auto) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test");
    req->tools[0].description = talloc_strdup(req, "test");
    req->tools[0].parameters = talloc_strdup(req, "{}");
    req->tool_choice_mode = 0; // IK_TOOL_AUTO

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");

    ck_assert_ptr_nonnull(tool_choice);
    yyjson_val *type = yyjson_obj_get(tool_choice, "type");
    ck_assert_str_eq(yyjson_get_str(type), "auto");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_tool_choice_none) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test");
    req->tools[0].description = talloc_strdup(req, "test");
    req->tools[0].parameters = talloc_strdup(req, "{}");
    req->tool_choice_mode = 1; // IK_TOOL_NONE

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");

    ck_assert_ptr_nonnull(tool_choice);
    yyjson_val *type = yyjson_obj_get(tool_choice, "type");
    ck_assert_str_eq(yyjson_get_str(type), "none");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_tool_choice_required) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test");
    req->tools[0].description = talloc_strdup(req, "test");
    req->tools[0].parameters = talloc_strdup(req, "{}");
    req->tool_choice_mode = 2; // IK_TOOL_REQUIRED

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");

    ck_assert_ptr_nonnull(tool_choice);
    yyjson_val *type = yyjson_obj_get(tool_choice, "type");
    ck_assert_str_eq(yyjson_get_str(type), "any");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_tool_choice_default) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test");
    req->tools[0].description = talloc_strdup(req, "test");
    req->tools[0].parameters = talloc_strdup(req, "{}");
    req->tool_choice_mode = 99; // Unknown mode

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");

    ck_assert_ptr_nonnull(tool_choice);
    yyjson_val *type = yyjson_obj_get(tool_choice, "type");
    // Should default to auto
    ck_assert_str_eq(yyjson_get_str(type), "auto");

    yyjson_doc_free(doc);
}

END_TEST
/* ================================================================
 * Header Building Tests
 * ================================================================ */

/* ================================================================
 * Error Case Tests
 * ================================================================ */

START_TEST(test_serialize_invalid_tool_call_json) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->messages[0].role = IK_ROLE_ASSISTANT;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    req->messages[0].content_blocks[0].data.tool_call.id = talloc_strdup(req, "call_123");
    req->messages[0].content_blocks[0].data.tool_call.name = talloc_strdup(req, "get_weather");
    // Invalid JSON - should cause serialization to fail
    req->messages[0].content_blocks[0].data.tool_call.arguments = talloc_strdup(req, "not valid json");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
}

END_TEST

START_TEST(test_serialize_invalid_tool_params_json) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather");
    // Invalid JSON - should cause serialization to fail
    req->tools[0].parameters = talloc_strdup(req, "invalid json");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_request_suite_4(void)
{
    Suite *s = suite_create("Anthropic Request - Part 4");

    TCase *tc_tools = tcase_create("Tool Definitions");
    tcase_set_timeout(tc_tools, 30);
    tcase_add_unchecked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_tools_none);
    tcase_add_test(tc_tools, test_tools_single);
    tcase_add_test(tc_tools, test_tools_multiple);
    tcase_add_test(tc_tools, test_tool_choice_auto);
    tcase_add_test(tc_tools, test_tool_choice_none);
    tcase_add_test(tc_tools, test_tool_choice_required);
    tcase_add_test(tc_tools, test_tool_choice_default);
    suite_add_tcase(s, tc_tools);

    TCase *tc_errors = tcase_create("Error Cases");
    tcase_set_timeout(tc_errors, 30);
    tcase_add_unchecked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_serialize_invalid_tool_call_json);
    tcase_add_test(tc_errors, test_serialize_invalid_tool_params_json);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    Suite *s = anthropic_request_suite_4();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
