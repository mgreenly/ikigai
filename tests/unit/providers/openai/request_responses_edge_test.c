/**
 * @file request_responses_edge_test.c
 * @brief Edge case tests for OpenAI Responses API request serialization
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
 * Error Handling Tests
 * ================================================================ */

START_TEST(test_serialize_null_model)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    req->model = NULL; // Invalid

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(is_err(&result));
}

END_TEST START_TEST(test_serialize_invalid_tool_params)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    // Invalid JSON in parameters
    const char *bad_params = "{invalid json}";
    ik_request_add_tool(req, "bad_tool", "Bad", bad_params, true);

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(is_err(&result));
}

END_TEST
/* ================================================================
 * Edge Cases
 * ================================================================ */

START_TEST(test_serialize_empty_system_prompt)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_set_system(req, "");
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Empty system prompt should not be included
    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_null(instructions);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_max_output_tokens_zero)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    req->max_output_tokens = 0; // Not set
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // max_output_tokens should not be present
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_output_tokens");
    ck_assert_ptr_null(max_tokens);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_no_streaming)
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

    // stream field should not be present when streaming is false
    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_null(stream);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *request_responses_edge_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Edge Cases");

    TCase *tc_errors = tcase_create("Error Handling");
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_serialize_null_model);
    tcase_add_test(tc_errors, test_serialize_invalid_tool_params);
    suite_add_tcase(s, tc_errors);

    TCase *tc_edge = tcase_create("Edge Cases");
    tcase_add_checked_fixture(tc_edge, setup, teardown);
    tcase_add_test(tc_edge, test_serialize_empty_system_prompt);
    tcase_add_test(tc_edge, test_serialize_max_output_tokens_zero);
    tcase_add_test(tc_edge, test_serialize_no_streaming);
    suite_add_tcase(s, tc_edge);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = request_responses_edge_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
