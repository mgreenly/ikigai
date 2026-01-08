/**
 * @file response_utils_test.c
 * @brief Unit tests for Google response utility functions
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/response.h"
#include "providers/google/response_utils.h"
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
 * Thought Signature Extraction Tests
 * ================================================================ */

START_TEST(test_extract_thought_signature_top_level) {
    const char *json = "{\"thoughtSignature\":\"test-signature\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    char *result = ik_google_extract_thought_signature_from_response(test_ctx, root);

    ck_assert_ptr_nonnull(result);
    ck_assert_ptr_nonnull(strstr(result, "test-signature"));
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_extract_thought_signature_in_candidates) {
    const char *json = "{\"candidates\":[{\"thoughtSignature\":\"candidate-sig\"}]}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    char *result = ik_google_extract_thought_signature_from_response(test_ctx, root);

    ck_assert_ptr_nonnull(result);
    ck_assert_ptr_nonnull(strstr(result, "candidate-sig"));
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_extract_thought_signature_no_signature) {
    const char *json = "{\"other\":\"field\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    char *result = ik_google_extract_thought_signature_from_response(test_ctx, root);

    ck_assert_ptr_null(result);
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_extract_thought_signature_candidates_not_array) {
    const char *json = "{\"candidates\":\"not-an-array\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    char *result = ik_google_extract_thought_signature_from_response(test_ctx, root);

    ck_assert_ptr_null(result);
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_extract_thought_signature_candidates_empty_array) {
    const char *json = "{\"candidates\":[]}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    char *result = ik_google_extract_thought_signature_from_response(test_ctx, root);

    ck_assert_ptr_null(result);
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_extract_thought_signature_not_string) {
    const char *json = "{\"thoughtSignature\":123}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    char *result = ik_google_extract_thought_signature_from_response(test_ctx, root);

    ck_assert_ptr_null(result);
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_extract_thought_signature_empty_string) {
    const char *json = "{\"thoughtSignature\":\"\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    char *result = ik_google_extract_thought_signature_from_response(test_ctx, root);

    ck_assert_ptr_null(result);
    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_response_utils_suite(void)
{
    Suite *s = suite_create("Google Response Utils");

    TCase *tc_thought = tcase_create("Thought Signature Extraction");
    tcase_set_timeout(tc_thought, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_thought, setup, teardown);
    tcase_add_test(tc_thought, test_extract_thought_signature_top_level);
    tcase_add_test(tc_thought, test_extract_thought_signature_in_candidates);
    tcase_add_test(tc_thought, test_extract_thought_signature_no_signature);
    tcase_add_test(tc_thought, test_extract_thought_signature_candidates_not_array);
    tcase_add_test(tc_thought, test_extract_thought_signature_candidates_empty_array);
    tcase_add_test(tc_thought, test_extract_thought_signature_not_string);
    tcase_add_test(tc_thought, test_extract_thought_signature_empty_string);
    suite_add_tcase(s, tc_thought);

    return s;
}

int main(void)
{
    Suite *s = google_response_utils_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
