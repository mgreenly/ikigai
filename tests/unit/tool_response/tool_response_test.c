#include <check.h>
#include <talloc.h>
#include <string.h>
#include <signal.h>
#include "vendor/yyjson/yyjson.h"
#include "error.h"
#include "tool_response.h"

START_TEST(test_tool_response_error_basic) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *result = NULL;

    res_t res = ik_tool_response_error(ctx, "Test error message", &result);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);

    // Parse and verify JSON structure
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(!yyjson_get_bool(success));

    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    ck_assert_str_eq(yyjson_get_str(error), "Test error message");

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_tool_response_error_special_chars) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *result = NULL;

    res_t res = ik_tool_response_error(ctx, "Error with \"quotes\" and\nnewlines", &result);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);

    // Parse and verify JSON structure
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_str_eq(yyjson_get_str(error), "Error with \"quotes\" and\nnewlines");

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_tool_response_success_basic) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *result = NULL;

    res_t res = ik_tool_response_success(ctx, "Command output", &result);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);

    // Parse and verify JSON structure
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success));

    yyjson_val *output = yyjson_obj_get(root, "output");
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(yyjson_get_str(output), "Command output");

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_tool_response_success_empty_output) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *result = NULL;

    res_t res = ik_tool_response_success(ctx, "", &result);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);

    // Parse and verify JSON structure
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *output = yyjson_obj_get(root, "output");
    ck_assert_str_eq(yyjson_get_str(output), "");

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

// Callback to add custom fields
static void add_custom_fields(yyjson_mut_doc *doc, yyjson_mut_val *root, void *user_ctx) {
    (void)user_ctx; // unused
    yyjson_mut_obj_add_int(doc, root, "exit_code", 42);
    yyjson_mut_obj_add_str(doc, root, "custom", "value");
}

START_TEST(test_tool_response_success_ex_with_fields) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *result = NULL;

    res_t res = ik_tool_response_success_ex(ctx, "Output text", add_custom_fields, NULL, &result);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);

    // Parse and verify JSON structure
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success));

    yyjson_val *output = yyjson_obj_get(root, "output");
    ck_assert_str_eq(yyjson_get_str(output), "Output text");

    yyjson_val *exit_code = yyjson_obj_get(root, "exit_code");
    ck_assert_int_eq(yyjson_get_int(exit_code), 42);

    yyjson_val *custom = yyjson_obj_get(root, "custom");
    ck_assert_str_eq(yyjson_get_str(custom), "value");

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_tool_response_success_ex_without_fields) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *result = NULL;

    res_t res = ik_tool_response_success_ex(ctx, "Basic output", NULL, NULL, &result);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);

    // Parse and verify JSON structure
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success));

    yyjson_val *output = yyjson_obj_get(root, "output");
    ck_assert_str_eq(yyjson_get_str(output), "Basic output");

    // Verify no custom fields were added
    yyjson_val *exit_code = yyjson_obj_get(root, "exit_code");
    ck_assert_ptr_null(exit_code);

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST

#ifndef SKIP_SIGNAL_TESTS
START_TEST(test_tool_response_null_ctx) {
    char *result = NULL;

    // Should assert when ctx is NULL
    ik_tool_response_error(NULL, "error", &result);
}
END_TEST

START_TEST(test_tool_response_null_message) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *result = NULL;

    // Should assert when message is NULL
    ik_tool_response_error(ctx, NULL, &result);

    talloc_free(ctx);
}
END_TEST
#endif

static Suite *tool_response_suite(void) {
    Suite *s = suite_create("Tool Response");

    TCase *tc_error = tcase_create("Error Response");
    tcase_add_test(tc_error, test_tool_response_error_basic);
    tcase_add_test(tc_error, test_tool_response_error_special_chars);
    suite_add_tcase(s, tc_error);

    TCase *tc_success = tcase_create("Success Response");
    tcase_add_test(tc_success, test_tool_response_success_basic);
    tcase_add_test(tc_success, test_tool_response_success_empty_output);
    tcase_add_test(tc_success, test_tool_response_success_ex_with_fields);
    tcase_add_test(tc_success, test_tool_response_success_ex_without_fields);
    suite_add_tcase(s, tc_success);

    TCase *tc_null = tcase_create("NULL Arguments");
#ifndef SKIP_SIGNAL_TESTS
    tcase_add_test_raise_signal(tc_null, test_tool_response_null_ctx, SIGABRT);
    tcase_add_test_raise_signal(tc_null, test_tool_response_null_message, SIGABRT);
#endif
    suite_add_tcase(s, tc_null);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s = tool_response_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
