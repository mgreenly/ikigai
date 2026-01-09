/**
 * @file anthropic_request_test_1.c
 * @brief Unit tests for Anthropic request serialization - Part 1: Basic tests
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/request.h"
#include "providers/provider.h"
#include "providers/provider_types.h"
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
 * Basic Request Serialization Tests
 * ================================================================ */

START_TEST(test_serialize_request_stream) {
    ik_request_t *req = create_basic_request(test_ctx);
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Check stream is true
    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_nonnull(stream);
    ck_assert(yyjson_get_bool(stream) == true);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_null_model) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = NULL;  // Explicitly NULL
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

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Model cannot be NULL");
}

END_TEST

START_TEST(test_serialize_request_default_max_tokens) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->max_output_tokens = 0;  // Should default to 4096

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_tokens");
    ck_assert_ptr_nonnull(max_tokens);
    ck_assert_int_eq(yyjson_get_int(max_tokens), 4096);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_with_system_prompt) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = talloc_strdup(req, "You are a helpful assistant");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");
    ck_assert_ptr_nonnull(system);
    ck_assert_str_eq(yyjson_get_str(system), "You are a helpful assistant");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_with_thinking_low) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->thinking.level = IK_THINKING_LOW;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_nonnull(thinking);

    yyjson_val *type = yyjson_obj_get(thinking, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "enabled");

    yyjson_val *budget = yyjson_obj_get(thinking, "budget_tokens");
    ck_assert_ptr_nonnull(budget);
    ck_assert(yyjson_get_int(budget) > 0);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_with_thinking_high) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->thinking.level = IK_THINKING_HIGH;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_nonnull(thinking);

    yyjson_val *type = yyjson_obj_get(thinking, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "enabled");

    yyjson_val *budget = yyjson_obj_get(thinking, "budget_tokens");
    ck_assert_ptr_nonnull(budget);
    ck_assert(yyjson_get_int(budget) > 0);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_with_tools_auto) {
    ik_request_t *req = create_basic_request(test_ctx);

    // Add a tool
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test_tool");
    req->tools[0].description = talloc_strdup(req, "A test tool");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\",\"properties\":{}}");
    req->tools[0].strict = false;
    req->tool_choice_mode = 0;  // IK_TOOL_AUTO

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // Check tools array
    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_arr_size(tools) == 1);

    yyjson_val *tool = yyjson_arr_get_first(tools);
    yyjson_val *name = yyjson_obj_get(tool, "name");
    ck_assert_str_eq(yyjson_get_str(name), "test_tool");

    yyjson_val *description = yyjson_obj_get(tool, "description");
    ck_assert_str_eq(yyjson_get_str(description), "A test tool");

    yyjson_val *input_schema = yyjson_obj_get(tool, "input_schema");
    ck_assert_ptr_nonnull(input_schema);

    // Check tool_choice
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    yyjson_val *choice_type = yyjson_obj_get(tool_choice, "type");
    ck_assert_str_eq(yyjson_get_str(choice_type), "auto");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_with_tools_none) {
    ik_request_t *req = create_basic_request(test_ctx);

    // Add a tool
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test_tool");
    req->tools[0].description = talloc_strdup(req, "A test tool");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\"}");
    req->tools[0].strict = false;
    req->tool_choice_mode = 1;  // IK_TOOL_NONE

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // Check tool_choice
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    yyjson_val *choice_type = yyjson_obj_get(tool_choice, "type");
    ck_assert_str_eq(yyjson_get_str(choice_type), "none");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_with_tools_required) {
    ik_request_t *req = create_basic_request(test_ctx);

    // Add a tool
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test_tool");
    req->tools[0].description = talloc_strdup(req, "A test tool");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\"}");
    req->tools[0].strict = false;
    req->tool_choice_mode = 2;  // IK_TOOL_REQUIRED

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // Check tool_choice
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    yyjson_val *choice_type = yyjson_obj_get(tool_choice, "type");
    ck_assert_str_eq(yyjson_get_str(choice_type), "any");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_with_invalid_tool_json) {
    ik_request_t *req = create_basic_request(test_ctx);

    // Add a tool with invalid JSON
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test_tool");
    req->tools[0].description = talloc_strdup(req, "A test tool");
    req->tools[0].parameters = talloc_strdup(req, "{invalid json}");  // Invalid JSON
    req->tools[0].strict = false;
    req->tool_choice_mode = 0;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Invalid tool parameters JSON");
}

END_TEST

START_TEST(test_serialize_request_with_tools_unknown_mode) {
    ik_request_t *req = create_basic_request(test_ctx);

    // Add a tool with unknown tool choice mode
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test_tool");
    req->tools[0].description = talloc_strdup(req, "A test tool");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\"}");
    req->tools[0].strict = false;
    req->tool_choice_mode = 99;  // Unknown mode, should default to "auto"

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // Check tool_choice defaults to auto
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    yyjson_val *choice_type = yyjson_obj_get(tool_choice, "type");
    ck_assert_str_eq(yyjson_get_str(choice_type), "auto");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_thinking_budget_exceeds_max_tokens) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->thinking.level = IK_THINKING_HIGH;
    req->max_output_tokens = 100;  // Very small, will be less than thinking budget

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // max_tokens should be adjusted to budget + 4096
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_tokens");
    ck_assert_ptr_nonnull(max_tokens);
    ck_assert(yyjson_get_int(max_tokens) > 4096);  // Should be budget + 4096

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_request_suite_1(void)
{
    Suite *s = suite_create("Anthropic Request - Part 1");

    TCase *tc_basic = tcase_create("Basic Serialization");
    tcase_set_timeout(tc_basic, 30);
    tcase_add_unchecked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_serialize_request_stream);
    tcase_add_test(tc_basic, test_serialize_request_null_model);
    tcase_add_test(tc_basic, test_serialize_request_default_max_tokens);
    tcase_add_test(tc_basic, test_serialize_request_with_system_prompt);
    tcase_add_test(tc_basic, test_serialize_request_with_thinking_low);
    tcase_add_test(tc_basic, test_serialize_request_with_thinking_high);
    tcase_add_test(tc_basic, test_serialize_request_with_tools_auto);
    tcase_add_test(tc_basic, test_serialize_request_with_tools_none);
    tcase_add_test(tc_basic, test_serialize_request_with_tools_required);
    tcase_add_test(tc_basic, test_serialize_request_with_invalid_tool_json);
    tcase_add_test(tc_basic, test_serialize_request_with_tools_unknown_mode);
    tcase_add_test(tc_basic, test_serialize_request_thinking_budget_exceeds_max_tokens);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void)
{
    Suite *s = anthropic_request_suite_1();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
