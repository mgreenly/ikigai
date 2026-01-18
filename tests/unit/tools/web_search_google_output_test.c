#include "../../test_constants.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

#include "../../../src/tools/web_search_google/output.h"
#include "../../../src/vendor/yyjson/yyjson.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

START_TEST(test_output_error_with_event_auth_missing) {
    FILE *fp = tmpfile();
    ck_assert_ptr_nonnull(fp);

    int32_t saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(fp), STDOUT_FILENO);

    output_error_with_event(test_ctx, "Test error", "AUTH_MISSING");

    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    rewind(fp);
    char buf[4096] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);

    ck_assert(n > 0);

    yyjson_doc *doc = yyjson_read(buf, n, 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(!yyjson_get_bool(success));

    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    ck_assert_str_eq(yyjson_get_str(error), "Test error");

    yyjson_val *error_code = yyjson_obj_get(root, "error_code");
    ck_assert_ptr_nonnull(error_code);
    ck_assert_str_eq(yyjson_get_str(error_code), "AUTH_MISSING");

    yyjson_val *event = yyjson_obj_get(root, "_event");
    ck_assert_ptr_nonnull(event);

    yyjson_val *kind = yyjson_obj_get(event, "kind");
    ck_assert_ptr_nonnull(kind);
    ck_assert_str_eq(yyjson_get_str(kind), "config_required");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_output_error_with_event_other_code) {
    FILE *fp = tmpfile();
    ck_assert_ptr_nonnull(fp);

    int32_t saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(fp), STDOUT_FILENO);

    output_error_with_event(test_ctx, "Other error", "OTHER_CODE");

    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    rewind(fp);
    char buf[4096] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);

    ck_assert(n > 0);

    yyjson_doc *doc = yyjson_read(buf, n, 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(!yyjson_get_bool(success));

    yyjson_val *event = yyjson_obj_get(root, "_event");
    ck_assert_ptr_null(event);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_output_error_network_error) {
    FILE *fp = tmpfile();
    ck_assert_ptr_nonnull(fp);

    int32_t saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(fp), STDOUT_FILENO);

    output_error(test_ctx, "Network failed", "NETWORK_ERROR");

    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    rewind(fp);
    char buf[4096] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);

    ck_assert(n > 0);

    yyjson_doc *doc = yyjson_read(buf, n, 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(!yyjson_get_bool(success));

    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    ck_assert_str_eq(yyjson_get_str(error), "Network failed");

    yyjson_val *error_code = yyjson_obj_get(root, "error_code");
    ck_assert_ptr_nonnull(error_code);
    ck_assert_str_eq(yyjson_get_str(error_code), "NETWORK_ERROR");

    yyjson_val *event = yyjson_obj_get(root, "_event");
    ck_assert_ptr_null(event);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_output_error_api_error) {
    FILE *fp = tmpfile();
    ck_assert_ptr_nonnull(fp);

    int32_t saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(fp), STDOUT_FILENO);

    output_error(test_ctx, "API failed", "API_ERROR");

    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    rewind(fp);
    char buf[4096] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);

    ck_assert(n > 0);

    yyjson_doc *doc = yyjson_read(buf, n, 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(!yyjson_get_bool(success));

    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    ck_assert_str_eq(yyjson_get_str(error), "API failed");

    yyjson_val *error_code = yyjson_obj_get(root, "error_code");
    ck_assert_ptr_nonnull(error_code);
    ck_assert_str_eq(yyjson_get_str(error_code), "API_ERROR");

    yyjson_doc_free(doc);
}

END_TEST

static Suite *web_search_google_output_suite(void)
{
    Suite *s = suite_create("WebSearchGoogleOutput");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_output_error_with_event_auth_missing);
    tcase_add_test(tc_core, test_output_error_with_event_other_code);
    tcase_add_test(tc_core, test_output_error_network_error);
    tcase_add_test(tc_core, test_output_error_api_error);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = web_search_google_output_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
