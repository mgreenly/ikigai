/**
 * @file request_responses_advanced_test.c
 * @brief Advanced tests for OpenAI Responses API request serialization
 */

#include "../../../../src/providers/openai/request.h"
#include "../../../../src/providers/request.h"
#include "../../../../src/providers/provider.h"
#include "../../../../src/error.h"
#include "../../../../src/vendor/yyjson/yyjson.h"

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

END_TEST START_TEST(test_serialize_reasoning_medium)
{
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

END_TEST START_TEST(test_serialize_reasoning_high)
{
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

END_TEST START_TEST(test_serialize_reasoning_none)
{
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

    // No reasoning field should be present
    yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
    ck_assert_ptr_null(reasoning);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_non_reasoning_model_with_thinking)
{
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

START_TEST(test_serialize_single_tool)
{
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

    yyjson_val *func = yyjson_obj_get(tool, "function");
    ck_assert_ptr_nonnull(func);

    yyjson_val *name = yyjson_obj_get(func, "name");
    ck_assert_str_eq(yyjson_get_str(name), "calculator");

    yyjson_val *desc = yyjson_obj_get(func, "description");
    ck_assert_str_eq(yyjson_get_str(desc), "Performs calculations");

    yyjson_val *strict = yyjson_obj_get(func, "strict");
    ck_assert(yyjson_get_bool(strict));

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_multiple_tools)
{
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

END_TEST START_TEST(test_serialize_tool_choice_auto)
{
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

END_TEST START_TEST(test_serialize_tool_choice_none)
{
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

END_TEST START_TEST(test_serialize_tool_choice_required)
{
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

END_TEST START_TEST(test_serialize_tool_choice_unknown)
{
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
 * Input Format Tests
 * ================================================================ */

START_TEST(test_serialize_multi_turn_conversation)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    // Add multiple messages
    ik_request_add_message(req, IK_ROLE_USER, "First message");
    ik_request_add_message(req, IK_ROLE_ASSISTANT, "First response");
    ik_request_add_message(req, IK_ROLE_USER, "Second message");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Multi-turn should use array format
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);
    ck_assert(yyjson_is_arr(input));
    ck_assert_int_eq((int)yyjson_arr_size(input), 3);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_non_user_message)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    // Single assistant message should use array format (not string)
    ik_request_add_message(req, IK_ROLE_ASSISTANT, "Assistant message");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Non-user message should use array format
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);
    ck_assert(yyjson_is_arr(input));

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Instructions (System Prompt) Tests
 * ================================================================ */

START_TEST(test_serialize_with_system_prompt)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    res_t sys_result = ik_request_set_system(req, "You are a helpful assistant.");
    ck_assert(!is_err(&sys_result));
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_nonnull(instructions);
    ck_assert_str_eq(yyjson_get_str(instructions), "You are a helpful assistant.");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_without_system_prompt)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // No instructions field when system_prompt is NULL
    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_null(instructions);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_with_empty_system_prompt)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    res_t sys_result = ik_request_set_system(req, "");
    ck_assert(!is_err(&sys_result));
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // No instructions field when system_prompt is empty string
    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_null(instructions);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Streaming and Output Tests
 * ================================================================ */

START_TEST(test_serialize_streaming_enabled)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test streaming");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, true, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_nonnull(stream);
    ck_assert(yyjson_get_bool(stream));

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_streaming_disabled)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test no streaming");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // No stream field when streaming is disabled
    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_null(stream);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_max_output_tokens)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");
    req->max_output_tokens = 1024;

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *max_tokens = yyjson_obj_get(root, "max_output_tokens");
    ck_assert_ptr_nonnull(max_tokens);
    ck_assert_int_eq((int)yyjson_get_int(max_tokens), 1024);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_no_max_output_tokens)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");
    // max_output_tokens defaults to 0 (not set)

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // No max_output_tokens field when not set
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_output_tokens");
    ck_assert_ptr_null(max_tokens);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * URL Building Tests
 * ================================================================ */

START_TEST(test_build_responses_url)
{
    char *url = NULL;
    res_t result = ik_openai_build_responses_url(test_ctx, "https://api.openai.com", &url);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(url);
    ck_assert_str_eq(url, "https://api.openai.com/v1/responses");
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *request_responses_advanced_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Advanced Serialization");

    TCase *tc_reasoning = tcase_create("Reasoning Configuration");
    tcase_set_timeout(tc_reasoning, 30);
    tcase_add_checked_fixture(tc_reasoning, setup, teardown);
    tcase_add_test(tc_reasoning, test_serialize_reasoning_low);
    tcase_add_test(tc_reasoning, test_serialize_reasoning_medium);
    tcase_add_test(tc_reasoning, test_serialize_reasoning_high);
    tcase_add_test(tc_reasoning, test_serialize_reasoning_none);
    tcase_add_test(tc_reasoning, test_serialize_non_reasoning_model_with_thinking);
    suite_add_tcase(s, tc_reasoning);

    TCase *tc_tools = tcase_create("Tool Definitions");
    tcase_set_timeout(tc_tools, 30);
    tcase_add_checked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_serialize_single_tool);
    tcase_add_test(tc_tools, test_serialize_multiple_tools);
    tcase_add_test(tc_tools, test_serialize_tool_choice_auto);
    tcase_add_test(tc_tools, test_serialize_tool_choice_none);
    tcase_add_test(tc_tools, test_serialize_tool_choice_required);
    tcase_add_test(tc_tools, test_serialize_tool_choice_unknown);
    suite_add_tcase(s, tc_tools);

    TCase *tc_input = tcase_create("Input Format");
    tcase_set_timeout(tc_input, 30);
    tcase_add_checked_fixture(tc_input, setup, teardown);
    tcase_add_test(tc_input, test_serialize_multi_turn_conversation);
    tcase_add_test(tc_input, test_serialize_non_user_message);
    suite_add_tcase(s, tc_input);

    TCase *tc_instructions = tcase_create("Instructions");
    tcase_set_timeout(tc_instructions, 30);
    tcase_add_checked_fixture(tc_instructions, setup, teardown);
    tcase_add_test(tc_instructions, test_serialize_with_system_prompt);
    tcase_add_test(tc_instructions, test_serialize_without_system_prompt);
    tcase_add_test(tc_instructions, test_serialize_with_empty_system_prompt);
    suite_add_tcase(s, tc_instructions);

    TCase *tc_streaming = tcase_create("Streaming and Output");
    tcase_set_timeout(tc_streaming, 30);
    tcase_add_checked_fixture(tc_streaming, setup, teardown);
    tcase_add_test(tc_streaming, test_serialize_streaming_enabled);
    tcase_add_test(tc_streaming, test_serialize_streaming_disabled);
    tcase_add_test(tc_streaming, test_serialize_max_output_tokens);
    tcase_add_test(tc_streaming, test_serialize_no_max_output_tokens);
    suite_add_tcase(s, tc_streaming);

    TCase *tc_url = tcase_create("URL Building");
    tcase_set_timeout(tc_url, 30);
    tcase_add_checked_fixture(tc_url, setup, teardown);
    tcase_add_test(tc_url, test_build_responses_url);
    suite_add_tcase(s, tc_url);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = request_responses_advanced_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
