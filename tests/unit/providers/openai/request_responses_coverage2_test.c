#include "../../../test_constants.h"
/**
 * @file request_responses_coverage2_test.c
 * @brief Coverage tests for OpenAI Responses API request serialization (Part 2)
 *
 * Tests for optional fields, validation, and URL building edge cases.
 */

#include "request_responses_test_helper.h"

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

static Suite *request_responses_coverage2_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Coverage Tests (Part 2)");

    TCase *tc_fields = tcase_create("Optional Fields");
    tcase_set_timeout(tc_fields, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_fields, request_responses_setup, request_responses_teardown);
    tcase_add_test(tc_fields, test_system_prompt);
    tcase_add_test(tc_fields, test_empty_system_prompt);
    tcase_add_test(tc_fields, test_max_output_tokens);
    tcase_add_test(tc_fields, test_streaming);
    suite_add_tcase(s, tc_fields);

    TCase *tc_validation = tcase_create("Validation");
    tcase_set_timeout(tc_validation, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_validation, request_responses_setup, request_responses_teardown);
    tcase_add_test(tc_validation, test_null_model);
    tcase_add_test(tc_validation, test_empty_input);
    suite_add_tcase(s, tc_validation);

    TCase *tc_url = tcase_create("URL Building");
    tcase_set_timeout(tc_url, IK_TEST_TIMEOUT);
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
    Suite *s = request_responses_coverage2_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
