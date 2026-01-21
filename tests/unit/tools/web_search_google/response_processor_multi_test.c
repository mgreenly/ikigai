#include "../../../test_constants.h"

#include "json_allocator.h"
#include "tools/web_search_google/response_processor.h"

#include "vendor/yyjson/yyjson.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

START_TEST(test_multi_call_round_robin) {
    char *json1 = talloc_strdup(test_ctx,
                                "{\"items\":[{\"title\":\"Result 1A\",\"link\":\"https://a1.com\",\"snippet\":\"Snippet 1A\"},{\"title\":\"Result 1B\",\"link\":\"https://a2.com\",\"snippet\":\"Snippet 1B\"}]}");
    char *json2 = talloc_strdup(test_ctx,
                                "{\"items\":[{\"title\":\"Result 2A\",\"link\":\"https://b1.com\",\"snippet\":\"Snippet 2A\"},{\"title\":\"Result 2B\",\"link\":\"https://b2.com\",\"snippet\":\"Snippet 2B\"}]}");

    struct api_call calls[2];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    calls[1].success = true;
    calls[1].response.data = json2;
    calls[1].response.size = strlen(json2);
    calls[1].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 2, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 4);

    yyjson_val *r0 = yyjson_arr_get(results, 0);
    yyjson_val *t0 = yyjson_obj_get(r0, "title");
    ck_assert_str_eq(yyjson_get_str(t0), "Result 1A");

    yyjson_val *r1 = yyjson_arr_get(results, 1);
    yyjson_val *t1 = yyjson_obj_get(r1, "title");
    ck_assert_str_eq(yyjson_get_str(t1), "Result 2A");
}

END_TEST

START_TEST(test_multi_call_with_limit) {
    char *json1 = talloc_strdup(test_ctx,
                                "{\"items\":[{\"title\":\"Result 1A\",\"link\":\"https://a1.com\",\"snippet\":\"Snippet 1A\"},{\"title\":\"Result 1B\",\"link\":\"https://a2.com\",\"snippet\":\"Snippet 1B\"}]}");
    char *json2 = talloc_strdup(test_ctx,
                                "{\"items\":[{\"title\":\"Result 2A\",\"link\":\"https://b1.com\",\"snippet\":\"Snippet 2A\"},{\"title\":\"Result 2B\",\"link\":\"https://b2.com\",\"snippet\":\"Snippet 2B\"}]}");

    struct api_call calls[2];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    calls[1].success = true;
    calls[1].response.data = json2;
    calls[1].response.size = strlen(json2);
    calls[1].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 2, 2, 0, NULL, 3);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");
    yyjson_val *count = yyjson_obj_get(root, "count");

    ck_assert_int_eq(yyjson_get_int(count), 3);
    ck_assert_uint_eq(yyjson_arr_size(results), 3);
}

END_TEST

START_TEST(test_multi_call_duplicate_url) {
    char *json1 = talloc_strdup(test_ctx,
                                "{\"items\":[{\"title\":\"Result 1A\",\"link\":\"https://same.com\",\"snippet\":\"First\"},{\"title\":\"Result 1B\",\"link\":\"https://unique.com\",\"snippet\":\"Second\"}]}");
    char *json2 = talloc_strdup(test_ctx,
                                "{\"items\":[{\"title\":\"Result 2A\",\"link\":\"https://same.com\",\"snippet\":\"Duplicate\"},{\"title\":\"Result 2B\",\"link\":\"https://other.com\",\"snippet\":\"Third\"}]}");

    struct api_call calls[2];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    calls[1].success = true;
    calls[1].response.data = json2;
    calls[1].response.size = strlen(json2);
    calls[1].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 2, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 3);
}

END_TEST

START_TEST(test_multi_call_one_failed) {
    char *json1 = talloc_strdup(test_ctx,
                                "{\"items\":[{\"title\":\"Result 1A\",\"link\":\"https://a1.com\",\"snippet\":\"Snippet 1A\"}]}");

    struct api_call calls[2];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    calls[1].success = false;

    char *result = process_responses(test_ctx, calls, 2, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 1);
}

END_TEST

START_TEST(test_multi_call_no_snippet) {
    char *json1 = talloc_strdup(test_ctx, "{\"items\":[{\"title\":\"No Snippet\",\"link\":\"https://a1.com\"}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");
    yyjson_val *first_result = yyjson_arr_get(results, 0);
    yyjson_val *snippet = yyjson_obj_get(first_result, "snippet");

    ck_assert_str_eq(yyjson_get_str(snippet), "");
}

END_TEST

START_TEST(test_multi_call_invalid_json) {
    char *json1 = talloc_strdup(test_ctx, "not valid json");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_multi_call_missing_title) {
    char *json1 = talloc_strdup(test_ctx, "{\"items\":[{\"link\":\"https://a1.com\",\"snippet\":\"No title\"}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_multi_call_missing_link) {
    char *json1 = talloc_strdup(test_ctx, "{\"items\":[{\"title\":\"No Link\",\"snippet\":\"Test\"}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_multi_call_title_not_string) {
    char *json1 = talloc_strdup(test_ctx,
                                "{\"items\":[{\"title\":123,\"link\":\"https://a1.com\",\"snippet\":\"Test\"}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_multi_call_link_not_string) {
    char *json1 = talloc_strdup(test_ctx, "{\"items\":[{\"title\":\"Test\",\"link\":456,\"snippet\":\"Test\"}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_multi_call_snippet_not_string) {
    char *json1 = talloc_strdup(test_ctx,
                                "{\"items\":[{\"title\":\"Test\",\"link\":\"https://a1.com\",\"snippet\":789}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 1);

    yyjson_val *first_result = yyjson_arr_get(results, 0);
    yyjson_val *snippet = yyjson_obj_get(first_result, "snippet");
    ck_assert_str_eq(yyjson_get_str(snippet), "");
}

END_TEST

START_TEST(test_multi_call_both_calls_invalid_json) {
    char *json1 = talloc_strdup(test_ctx, "invalid json");
    char *json2 = talloc_strdup(test_ctx, "also invalid");

    struct api_call calls[2];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    calls[1].success = true;
    calls[1].response.data = json2;
    calls[1].response.size = strlen(json2);
    calls[1].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 2, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_multi_call_missing_items_field) {
    char *json1 = talloc_strdup(test_ctx, "{\"no_items\":true}");
    char *json2 = talloc_strdup(test_ctx, "{\"items\":[{\"title\":\"Result\",\"link\":\"https://example.com\"}]}");

    struct api_call calls[2];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    calls[1].success = true;
    calls[1].response.data = json2;
    calls[1].response.size = strlen(json2);
    calls[1].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 2, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 1);
}

END_TEST

START_TEST(test_multi_call_items_not_array_field) {
    char *json1 = talloc_strdup(test_ctx, "{\"items\":\"not array\"}");
    char *json2 = talloc_strdup(test_ctx, "{\"items\":[{\"title\":\"Result\",\"link\":\"https://example.com\"}]}");

    struct api_call calls[2];
    calls[0].success = true;
    calls[0].response.data = json1;
    calls[0].response.size = strlen(json1);
    calls[0].response.ctx = test_ctx;

    calls[1].success = true;
    calls[1].response.data = json2;
    calls[1].response.size = strlen(json2);
    calls[1].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 2, 2, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 1);
}

END_TEST

static Suite *response_processor_multi_suite(void)
{
    Suite *s = suite_create("WebSearchGoogleResponseProcessorMulti");

    TCase *tc = tcase_create("MultiCall");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_multi_call_round_robin);
    tcase_add_test(tc, test_multi_call_with_limit);
    tcase_add_test(tc, test_multi_call_duplicate_url);
    tcase_add_test(tc, test_multi_call_one_failed);
    tcase_add_test(tc, test_multi_call_no_snippet);
    tcase_add_test(tc, test_multi_call_invalid_json);
    tcase_add_test(tc, test_multi_call_missing_title);
    tcase_add_test(tc, test_multi_call_missing_link);
    tcase_add_test(tc, test_multi_call_title_not_string);
    tcase_add_test(tc, test_multi_call_link_not_string);
    tcase_add_test(tc, test_multi_call_snippet_not_string);
    tcase_add_test(tc, test_multi_call_both_calls_invalid_json);
    tcase_add_test(tc, test_multi_call_missing_items_field);
    tcase_add_test(tc, test_multi_call_items_not_array_field);

    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_processor_multi_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/web_search_google/response_processor_multi_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
