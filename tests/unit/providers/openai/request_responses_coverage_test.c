/**
 * @file request_responses_coverage_test.c
 * @brief Coverage tests for OpenAI Responses API request serialization
 *
 * Tests to achieve 100% coverage by triggering all error paths and branches.
 */

#include "request_responses_test_helpers.h"

/* ================================================================
 * Reasoning Invalid Level Test
 * ================================================================ */

START_TEST(test_reasoning_invalid_level) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");
    req->thinking.level = 999;     // Invalid level

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *reasoning = yyjson_obj_get(yyjson_doc_get_root(doc), "reasoning");
    ck_assert_ptr_null(reasoning);
    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Tool Choice Tests
 * ================================================================ */

START_TEST(test_tool_choice_auto) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    ik_request_add_message(req, IK_ROLE_USER, "Test");
    ik_request_add_tool(req, "test_tool", "Test description", "{\"type\":\"object\"}", true);
    req->tool_choice_mode = 0;

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tool_choice = yyjson_obj_get(yyjson_doc_get_root(doc), "tool_choice");
    ck_assert_str_eq(yyjson_get_str(tool_choice), "auto");
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_tool_choice_none) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    ik_request_add_message(req, IK_ROLE_USER, "Test");
    ik_request_add_tool(req, "test_tool", "Test description", "{\"type\":\"object\"}", true);
    req->tool_choice_mode = 1;

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tool_choice = yyjson_obj_get(yyjson_doc_get_root(doc), "tool_choice");
    ck_assert_str_eq(yyjson_get_str(tool_choice), "none");
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_tool_choice_required) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    ik_request_add_message(req, IK_ROLE_USER, "Test");
    ik_request_add_tool(req, "test_tool", "Test description", "{\"type\":\"object\"}", true);
    req->tool_choice_mode = 2;

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tool_choice = yyjson_obj_get(yyjson_doc_get_root(doc), "tool_choice");
    ck_assert_str_eq(yyjson_get_str(tool_choice), "required");
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_tool_choice_default_case) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    ik_request_add_message(req, IK_ROLE_USER, "Test");
    ik_request_add_tool(req, "test_tool", "Test description", "{\"type\":\"object\"}", true);
    req->tool_choice_mode = 999;

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *tool_choice = yyjson_obj_get(yyjson_doc_get_root(doc), "tool_choice");
    ck_assert_str_eq(yyjson_get_str(tool_choice), "auto");
    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Multi-turn Input Array Test
 * ================================================================ */

START_TEST(test_multi_turn_input_array) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    ik_request_add_message(req, IK_ROLE_USER, "First message");
    ik_request_add_message(req, IK_ROLE_ASSISTANT, "Response");
    ik_request_add_message(req, IK_ROLE_USER, "Second message");

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *input = yyjson_obj_get(yyjson_doc_get_root(doc), "input");
    ck_assert(yyjson_is_arr(input));
    ck_assert_uint_eq((unsigned int)yyjson_arr_size(input), 3U);
    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * System Prompt Test
 * ================================================================ */

START_TEST(test_system_prompt) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    req->system_prompt = talloc_strdup(req, "You are a helpful assistant");
    ik_request_add_message(req, IK_ROLE_USER, "Hello");

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *instructions = yyjson_obj_get(yyjson_doc_get_root(doc), "instructions");
    ck_assert_str_eq(yyjson_get_str(instructions), "You are a helpful assistant");
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_empty_system_prompt) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    req->system_prompt = talloc_strdup(req, "");
    ik_request_add_message(req, IK_ROLE_USER, "Hello");

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_null(yyjson_obj_get(yyjson_doc_get_root(doc), "instructions"));
    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Max Output Tokens Test
 * ================================================================ */

START_TEST(test_max_output_tokens) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");
    req->max_output_tokens = 1000;

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    // Verify max_output_tokens field
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_output_tokens");
    ck_assert_ptr_nonnull(max_tokens);
    ck_assert_int_eq(yyjson_get_int(max_tokens), 1000);
    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Streaming Test
 * ================================================================ */

START_TEST(test_streaming) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, true, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    // Verify stream field is true
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_nonnull(stream);
    ck_assert(yyjson_get_bool(stream));
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_single_assistant_message_array) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));

    // Single assistant message should use array format, not string
    ik_request_add_message(req, IK_ROLE_ASSISTANT, "Response");

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *input = yyjson_obj_get(yyjson_doc_get_root(doc), "input");
    ck_assert(yyjson_is_arr(input));
    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Multiple Text Blocks Test
 * ================================================================ */

START_TEST(test_multiple_text_blocks) {
    ik_request_t *req = NULL;
    res_t r = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&r));
    ik_request_add_message(req, IK_ROLE_USER, "First block");

    // Add second text block to same message
    ik_message_t *msg = &req->messages[0];
    size_t n = msg->content_count;
    msg->content_blocks = talloc_realloc(req, msg->content_blocks, ik_content_block_t, (unsigned int)(n + 1U));
    msg->content_count = n + 1U;
    msg->content_blocks[n].type = IK_CONTENT_TEXT;
    msg->content_blocks[n].data.text.text = talloc_strdup(req, "Second block");

    char *json = NULL;
    r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *input = yyjson_obj_get(yyjson_doc_get_root(doc), "input");
    ck_assert_str_eq(yyjson_get_str(input), "First block\n\nSecond block");
    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Valid Reasoning Effort Test
 * ================================================================ */

START_TEST(test_valid_reasoning_effort) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    // Set a valid thinking level (e.g., 1 = low)
    req->thinking.level = 1;

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    // Verify reasoning object with effort field
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

/* ================================================================
 * NULL Model Test
 * ================================================================ */

START_TEST(test_null_model) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    // Set model to NULL to trigger validation error
    req->model = NULL;

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    // Should fail with ERR_INVALID_ARG error
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}

END_TEST

/* ================================================================
 * Empty Input Test
 * ================================================================ */

START_TEST(test_empty_input) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    // Single user message with non-text content triggers empty input string
    req->message_count = 1;
    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_blocks[0].type = IK_CONTENT_TOOL_CALL;

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *input = yyjson_obj_get(yyjson_doc_get_root(doc), "input");
    ck_assert_str_eq(yyjson_get_str(input), "");
    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Build URL Test
 * ================================================================ */

START_TEST(test_build_responses_url) {
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

static Suite *request_responses_coverage_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Coverage Tests");

    TCase *tc_reasoning = tcase_create("Reasoning Edge Cases");
    tcase_set_timeout(tc_reasoning, 30);
    tcase_add_checked_fixture(tc_reasoning, request_responses_setup, request_responses_teardown);
    tcase_add_test(tc_reasoning, test_reasoning_invalid_level);
    tcase_add_test(tc_reasoning, test_valid_reasoning_effort);
    suite_add_tcase(s, tc_reasoning);

    TCase *tc_tool_choice = tcase_create("Tool Choice Edge Cases");
    tcase_set_timeout(tc_tool_choice, 30);
    tcase_add_checked_fixture(tc_tool_choice, request_responses_setup, request_responses_teardown);
    tcase_add_test(tc_tool_choice, test_tool_choice_auto);
    tcase_add_test(tc_tool_choice, test_tool_choice_none);
    tcase_add_test(tc_tool_choice, test_tool_choice_required);
    tcase_add_test(tc_tool_choice, test_tool_choice_default_case);
    suite_add_tcase(s, tc_tool_choice);

    TCase *tc_input = tcase_create("Input Formats");
    tcase_set_timeout(tc_input, 30);
    tcase_add_checked_fixture(tc_input, request_responses_setup, request_responses_teardown);
    tcase_add_test(tc_input, test_multi_turn_input_array);
    tcase_add_test(tc_input, test_single_assistant_message_array);
    tcase_add_test(tc_input, test_multiple_text_blocks);
    suite_add_tcase(s, tc_input);

    TCase *tc_fields = tcase_create("Optional Fields");
    tcase_set_timeout(tc_fields, 30);
    tcase_add_checked_fixture(tc_fields, request_responses_setup, request_responses_teardown);
    tcase_add_test(tc_fields, test_system_prompt);
    tcase_add_test(tc_fields, test_empty_system_prompt);
    tcase_add_test(tc_fields, test_max_output_tokens);
    tcase_add_test(tc_fields, test_streaming);
    suite_add_tcase(s, tc_fields);

    TCase *tc_validation = tcase_create("Validation");
    tcase_set_timeout(tc_validation, 30);
    tcase_add_checked_fixture(tc_validation, request_responses_setup, request_responses_teardown);
    tcase_add_test(tc_validation, test_null_model);
    tcase_add_test(tc_validation, test_empty_input);
    suite_add_tcase(s, tc_validation);

    TCase *tc_url = tcase_create("URL Building");
    tcase_set_timeout(tc_url, 30);
    tcase_add_checked_fixture(tc_url, request_responses_setup, request_responses_teardown);
    tcase_add_test(tc_url, test_build_responses_url);
    suite_add_tcase(s, tc_url);

    // Note: Additional edge case tests for lines 158 and 185 are complex due to
    // branch evaluation order and require more investigation. The tool_choice
    // default case test above provides the most straightforward coverage improvement.

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = request_responses_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
