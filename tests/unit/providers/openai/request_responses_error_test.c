/**
 * @file request_responses_error_test.c
 * @brief Error handling and URL tests for OpenAI Responses API
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

START_TEST(test_serialize_null_model) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    // Set model to NULL to test validation
    req->model = NULL;
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    // Should return an error when model is NULL
    ck_assert(is_err(&result));
}

END_TEST

/* ================================================================
 * URL Building Tests
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

static Suite *request_responses_error_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Error Handling");

    TCase *tc_errors = tcase_create("Error Handling");
    tcase_set_timeout(tc_errors, 30);
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_serialize_null_model);
    suite_add_tcase(s, tc_errors);

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
    Suite *s = request_responses_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
