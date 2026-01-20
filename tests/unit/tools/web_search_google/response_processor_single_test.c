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

START_TEST(test_single_call_success_basic) {
    char *json_response = talloc_strdup(test_ctx,
                                        "{\"items\":[{\"title\":\"Test Title\",\"link\":\"https://example.com\",\"snippet\":\"Test snippet\"}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json_response;
    calls[0].response.size = strlen(json_response);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 1, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_int_eq(yyjson_get_bool(success), 1);

    yyjson_val *results = yyjson_obj_get(root, "results");
    ck_assert_uint_eq(yyjson_arr_size(results), 1);

    yyjson_val *first_result = yyjson_arr_get(results, 0);
    yyjson_val *title = yyjson_obj_get(first_result, "title");
    ck_assert_str_eq(yyjson_get_str(title), "Test Title");

}

END_TEST

START_TEST(test_single_call_no_snippet) {
    char *json_response = talloc_strdup(test_ctx,
                                        "{\"items\":[{\"title\":\"No Snippet\",\"link\":\"https://example.com\"}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json_response;
    calls[0].response.size = strlen(json_response);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 1, 0, NULL, 10);

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

START_TEST(test_single_call_failed) {
    struct api_call calls[1];
    calls[0].success = false;

    char *result = process_responses(test_ctx, calls, 1, 1, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_single_call_invalid_json) {
    char *json_response = talloc_strdup(test_ctx, "not valid json");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json_response;
    calls[0].response.size = strlen(json_response);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 1, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_single_call_missing_title) {
    char *json_response = talloc_strdup(test_ctx,
                                        "{\"items\":[{\"link\":\"https://example.com\",\"snippet\":\"Test\"}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json_response;
    calls[0].response.size = strlen(json_response);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 1, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_single_call_missing_link) {
    char *json_response = talloc_strdup(test_ctx, "{\"items\":[{\"title\":\"Test\",\"snippet\":\"Test\"}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json_response;
    calls[0].response.size = strlen(json_response);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 1, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_single_call_items_not_array) {
    char *json_response = talloc_strdup(test_ctx, "{\"items\":\"not an array\"}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json_response;
    calls[0].response.size = strlen(json_response);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 1, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_single_call_blocked_domains) {
    char *json_response = talloc_strdup(test_ctx,
                                        "{\"items\":[{\"title\":\"Blocked Site\",\"link\":\"https://blocked-example.com/page\",\"snippet\":\"Should be blocked\"},{\"title\":\"Allowed Site\",\"link\":\"https://allowed.com/page\",\"snippet\":\"Should pass\"}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json_response;
    calls[0].response.size = strlen(json_response);
    calls[0].response.ctx = test_ctx;

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    char *blocked_json = talloc_strdup(test_ctx, "[\"blocked-example.com\", \"spam.com\"]");
    yyjson_doc *blocked_doc = yyjson_read_opts(blocked_json, strlen(blocked_json), 0, &allocator, NULL);
    yyjson_val *blocked_val = yyjson_doc_get_root(blocked_doc);

    char *result = process_responses(test_ctx, calls, 1, 1, 2, blocked_val, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 1);

    yyjson_val *first_result = yyjson_arr_get(results, 0);
    yyjson_val *title = yyjson_obj_get(first_result, "title");
    ck_assert_str_eq(yyjson_get_str(title), "Allowed Site");
}

END_TEST

START_TEST(test_single_call_title_not_string) {
    char *json_response = talloc_strdup(test_ctx,
                                        "{\"items\":[{\"title\":123,\"link\":\"https://example.com\",\"snippet\":\"Test\"}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json_response;
    calls[0].response.size = strlen(json_response);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 1, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_single_call_link_not_string) {
    char *json_response = talloc_strdup(test_ctx,
                                        "{\"items\":[{\"title\":\"Test\",\"link\":456,\"snippet\":\"Test\"}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json_response;
    calls[0].response.size = strlen(json_response);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 1, 0, NULL, 10);

    ck_assert_ptr_nonnull(result);

    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(result, strlen(result), 0, &allocator, NULL);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *results = yyjson_obj_get(root, "results");

    ck_assert_uint_eq(yyjson_arr_size(results), 0);
}

END_TEST

START_TEST(test_single_call_snippet_not_string) {
    char *json_response = talloc_strdup(test_ctx,
                                        "{\"items\":[{\"title\":\"Test\",\"link\":\"https://example.com\",\"snippet\":789}]}");

    struct api_call calls[1];
    calls[0].success = true;
    calls[0].response.data = json_response;
    calls[0].response.size = strlen(json_response);
    calls[0].response.ctx = test_ctx;

    char *result = process_responses(test_ctx, calls, 1, 1, 0, NULL, 10);

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

static Suite *response_processor_single_suite(void)
{
    Suite *s = suite_create("WebSearchGoogleResponseProcessorSingle");

    TCase *tc = tcase_create("SingleCall");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_single_call_success_basic);
    tcase_add_test(tc, test_single_call_no_snippet);
    tcase_add_test(tc, test_single_call_failed);
    tcase_add_test(tc, test_single_call_invalid_json);
    tcase_add_test(tc, test_single_call_missing_title);
    tcase_add_test(tc, test_single_call_missing_link);
    tcase_add_test(tc, test_single_call_items_not_array);
    tcase_add_test(tc, test_single_call_blocked_domains);
    tcase_add_test(tc, test_single_call_title_not_string);
    tcase_add_test(tc, test_single_call_link_not_string);
    tcase_add_test(tc, test_single_call_snippet_not_string);

    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_processor_single_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
