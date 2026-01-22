#include "../../../test_constants.h"

#include "json_allocator.h"
#include "tools/web_search_google/result_utils.h"

#include <check.h>
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

START_TEST(test_url_already_seen_empty_array) {
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    bool result = url_already_seen(arr, "https://example.com");
    ck_assert_int_eq(result, false);
}

END_TEST

START_TEST(test_url_already_seen_not_found) {
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    yyjson_mut_val *item = yyjson_mut_obj(doc);
    yyjson_mut_val *url = yyjson_mut_str(doc, "https://example.com");
    yyjson_mut_obj_add_val(doc, item, "url", url);
    yyjson_mut_arr_append(arr, item);

    bool result = url_already_seen(arr, "https://different.com");
    ck_assert_int_eq(result, false);
}

END_TEST

START_TEST(test_url_already_seen_found) {
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    yyjson_mut_val *item = yyjson_mut_obj(doc);
    yyjson_mut_val *url = yyjson_mut_str(doc, "https://example.com");
    yyjson_mut_obj_add_val(doc, item, "url", url);
    yyjson_mut_arr_append(arr, item);

    bool result = url_already_seen(arr, "https://example.com");
    ck_assert_int_eq(result, true);
}

END_TEST

START_TEST(test_url_already_seen_multiple_urls) {
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    const char *urls[] = {
        "https://first.com",
        "https://second.com",
        "https://third.com",
    };

    for (int i = 0; i < 3; i++) {
        yyjson_mut_val *item = yyjson_mut_obj(doc);
        yyjson_mut_val *url = yyjson_mut_str(doc, urls[i]);
        yyjson_mut_obj_add_val(doc, item, "url", url);
        yyjson_mut_arr_append(arr, item);
    }

    ck_assert_int_eq(url_already_seen(arr, "https://second.com"), true);
    ck_assert_int_eq(url_already_seen(arr, "https://fourth.com"), false);
}

END_TEST

START_TEST(test_url_already_seen_item_without_url) {
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    yyjson_mut_val *item = yyjson_mut_obj(doc);
    yyjson_mut_val *title = yyjson_mut_str(doc, "Some Title");
    yyjson_mut_obj_add_val(doc, item, "title", title);
    yyjson_mut_arr_append(arr, item);

    bool result = url_already_seen(arr, "https://example.com");
    ck_assert_int_eq(result, false);
}

END_TEST

START_TEST(test_url_already_seen_url_not_string) {
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    yyjson_mut_val *item = yyjson_mut_obj(doc);
    yyjson_mut_val *url = yyjson_mut_int(doc, 42);
    yyjson_mut_obj_add_val(doc, item, "url", url);
    yyjson_mut_arr_append(arr, item);

    bool result = url_already_seen(arr, "https://example.com");
    ck_assert_int_eq(result, false);
}

END_TEST

START_TEST(test_url_already_seen_mixed_items) {
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    yyjson_mut_val *item1 = yyjson_mut_obj(doc);
    yyjson_mut_val *title = yyjson_mut_str(doc, "No URL Item");
    yyjson_mut_obj_add_val(doc, item1, "title", title);
    yyjson_mut_arr_append(arr, item1);

    yyjson_mut_val *item2 = yyjson_mut_obj(doc);
    yyjson_mut_val *url = yyjson_mut_str(doc, "https://example.com");
    yyjson_mut_obj_add_val(doc, item2, "url", url);
    yyjson_mut_arr_append(arr, item2);

    ck_assert_int_eq(url_already_seen(arr, "https://example.com"), true);
    ck_assert_int_eq(url_already_seen(arr, "https://other.com"), false);
}

END_TEST

static Suite *result_utils_suite(void)
{
    Suite *s = suite_create("WebSearchGoogleResultUtils");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_url_already_seen_empty_array);
    tcase_add_test(tc_core, test_url_already_seen_not_found);
    tcase_add_test(tc_core, test_url_already_seen_found);
    tcase_add_test(tc_core, test_url_already_seen_multiple_urls);
    tcase_add_test(tc_core, test_url_already_seen_item_without_url);
    tcase_add_test(tc_core, test_url_already_seen_url_not_string);
    tcase_add_test(tc_core, test_url_already_seen_mixed_items);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = result_utils_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/web_search_google/result_utils_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
