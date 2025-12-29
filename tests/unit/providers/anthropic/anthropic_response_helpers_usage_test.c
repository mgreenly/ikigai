/**
 * @file anthropic_response_helpers_usage_test.c
 * @brief Unit tests for Anthropic usage parsing helper functions
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/response_helpers.h"
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
 * Usage Parsing Tests
 * ================================================================ */

START_TEST(test_parse_usage_null) {
    ik_usage_t usage;
    memset(&usage, 0xFF, sizeof(usage)); // Fill with garbage

    ik_anthropic_parse_usage(NULL, &usage);

    ck_assert_int_eq(usage.input_tokens, 0);
    ck_assert_int_eq(usage.output_tokens, 0);
    ck_assert_int_eq(usage.thinking_tokens, 0);
    ck_assert_int_eq(usage.cached_tokens, 0);
    ck_assert_int_eq(usage.total_tokens, 0);
}
END_TEST START_TEST(test_parse_usage_basic)
{
    const char *json = "{\"input_tokens\": 100, \"output_tokens\": 50}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_usage_t usage;

    ik_anthropic_parse_usage(root, &usage);

    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 50);
    ck_assert_int_eq(usage.thinking_tokens, 0);
    ck_assert_int_eq(usage.cached_tokens, 0);
    ck_assert_int_eq(usage.total_tokens, 150);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_parse_usage_with_thinking)
{
    const char *json = "{\"input_tokens\": 100, \"output_tokens\": 50, \"thinking_tokens\": 25}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_usage_t usage;

    ik_anthropic_parse_usage(root, &usage);

    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 50);
    ck_assert_int_eq(usage.thinking_tokens, 25);
    ck_assert_int_eq(usage.cached_tokens, 0);
    ck_assert_int_eq(usage.total_tokens, 175);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_parse_usage_with_cached)
{
    const char *json = "{\"input_tokens\": 100, \"output_tokens\": 50, \"cache_read_input_tokens\": 200}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_usage_t usage;

    ik_anthropic_parse_usage(root, &usage);

    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 50);
    ck_assert_int_eq(usage.thinking_tokens, 0);
    ck_assert_int_eq(usage.cached_tokens, 200);
    ck_assert_int_eq(usage.total_tokens, 150);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_parse_usage_all_fields)
{
    const char *json = "{"
                       "\"input_tokens\": 100,"
                       "\"output_tokens\": 50,"
                       "\"thinking_tokens\": 25,"
                       "\"cache_read_input_tokens\": 200"
                       "}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_usage_t usage;

    ik_anthropic_parse_usage(root, &usage);

    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 50);
    ck_assert_int_eq(usage.thinking_tokens, 25);
    ck_assert_int_eq(usage.cached_tokens, 200);
    ck_assert_int_eq(usage.total_tokens, 175);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_parse_usage_empty_object)
{
    const char *json = "{}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_usage_t usage;

    ik_anthropic_parse_usage(root, &usage);

    ck_assert_int_eq(usage.input_tokens, 0);
    ck_assert_int_eq(usage.output_tokens, 0);
    ck_assert_int_eq(usage.thinking_tokens, 0);
    ck_assert_int_eq(usage.cached_tokens, 0);
    ck_assert_int_eq(usage.total_tokens, 0);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_parse_usage_non_int_values)
{
    // Non-integer values should be ignored
    const char *json = "{"
                       "\"input_tokens\": \"not a number\","
                       "\"output_tokens\": 50"
                       "}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_usage_t usage;

    ik_anthropic_parse_usage(root, &usage);

    ck_assert_int_eq(usage.input_tokens, 0);  // Ignored non-int
    ck_assert_int_eq(usage.output_tokens, 50);
    ck_assert_int_eq(usage.total_tokens, 50);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_parse_usage_output_tokens_not_int)
{
    // output_tokens is a string, should be ignored (line 161 branch 3)
    const char *json = "{"
                       "\"input_tokens\": 100,"
                       "\"output_tokens\": \"not an int\""
                       "}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_usage_t usage;

    ik_anthropic_parse_usage(root, &usage);

    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 0);  // Ignored non-int
    ck_assert_int_eq(usage.thinking_tokens, 0);
    ck_assert_int_eq(usage.cached_tokens, 0);
    ck_assert_int_eq(usage.total_tokens, 100);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_parse_usage_thinking_tokens_not_int)
{
    // thinking_tokens is a string, should be ignored (line 167 branch 3)
    const char *json = "{"
                       "\"input_tokens\": 100,"
                       "\"output_tokens\": 50,"
                       "\"thinking_tokens\": \"not an int\""
                       "}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_usage_t usage;

    ik_anthropic_parse_usage(root, &usage);

    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 50);
    ck_assert_int_eq(usage.thinking_tokens, 0);  // Ignored non-int
    ck_assert_int_eq(usage.cached_tokens, 0);
    ck_assert_int_eq(usage.total_tokens, 150);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_parse_usage_cached_tokens_not_int)
{
    // cache_read_input_tokens is a string, should be ignored (line 173 branch 3)
    const char *json = "{"
                       "\"input_tokens\": 100,"
                       "\"output_tokens\": 50,"
                       "\"cache_read_input_tokens\": \"not an int\""
                       "}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_usage_t usage;

    ik_anthropic_parse_usage(root, &usage);

    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 50);
    ck_assert_int_eq(usage.thinking_tokens, 0);
    ck_assert_int_eq(usage.cached_tokens, 0);  // Ignored non-int
    ck_assert_int_eq(usage.total_tokens, 150);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_response_helpers_usage_suite(void)
{
    Suite *s = suite_create("Anthropic Response Helpers - Usage");

    TCase *tc_usage = tcase_create("Usage Parsing");
    tcase_set_timeout(tc_usage, 30);
    tcase_add_unchecked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_parse_usage_null);
    tcase_add_test(tc_usage, test_parse_usage_basic);
    tcase_add_test(tc_usage, test_parse_usage_with_thinking);
    tcase_add_test(tc_usage, test_parse_usage_with_cached);
    tcase_add_test(tc_usage, test_parse_usage_all_fields);
    tcase_add_test(tc_usage, test_parse_usage_empty_object);
    tcase_add_test(tc_usage, test_parse_usage_non_int_values);
    tcase_add_test(tc_usage, test_parse_usage_output_tokens_not_int);
    tcase_add_test(tc_usage, test_parse_usage_thinking_tokens_not_int);
    tcase_add_test(tc_usage, test_parse_usage_cached_tokens_not_int);
    suite_add_tcase(s, tc_usage);

    return s;
}

int main(void)
{
    Suite *s = anthropic_response_helpers_usage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
