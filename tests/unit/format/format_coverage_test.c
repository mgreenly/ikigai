/**
 * @file format_coverage_test.c
 * @brief Tests to achieve 100% coverage for format.c
 */

#include <check.h>
#include <stdbool.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/format.h"
#include "../../../src/tool.h"
#include "../../../src/vendor/yyjson/yyjson.h"
#include "../../test_utils.h"

// Test fixture
static TALLOC_CTX *ctx;

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);
}

static void teardown(void)
{
    talloc_free(ctx);
}

START_TEST(test_format_tool_call_null_arguments) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_null", "tool_name", "");
    ck_assert_ptr_nonnull(call);
    call->arguments = NULL;
    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert_str_eq(formatted, "→ tool_name");
}
END_TEST

START_TEST(test_format_tool_call_json_array) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_array", "tool", "[1, 2, 3]");
    ck_assert_ptr_nonnull(call);
    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert_str_eq(formatted, "→ tool: [1, 2, 3]");
}

END_TEST

START_TEST(test_format_tool_call_bool_false) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_bool", "tool", "{\"enabled\": false}");
    ck_assert_ptr_nonnull(call);
    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert_str_eq(formatted, "→ tool: enabled=false");
}

END_TEST

START_TEST(test_format_tool_call_array_value) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_arr", "tool", "{\"items\": [\"a\", \"b\", \"c\"]}");
    ck_assert_ptr_nonnull(call);
    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "→ tool:") != NULL && strstr(formatted, "items=") != NULL);
}

END_TEST

START_TEST(test_format_tool_call_object_value) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_obj", "tool", "{\"config\": {\"key\": \"value\"}}");
    ck_assert_ptr_nonnull(call);
    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "→ tool:") != NULL && strstr(formatted, "config=") != NULL);
}

END_TEST

START_TEST(test_format_tool_result_zero_length_content) {
    const char *formatted = ik_format_tool_result(ctx, "tool", "[\"\"]");
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strlen(formatted) > 0);
}

END_TEST

START_TEST(test_format_tool_result_array_with_numbers) {
    const char *formatted = ik_format_tool_result(ctx, "tool", "[1, 2, 3]");
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "← tool:") != NULL);
    ck_assert(strstr(formatted, "1") != NULL);
}

END_TEST

START_TEST(test_format_tool_result_array_mixed_types) {
    const char *formatted = ik_format_tool_result(ctx, "tool", "[\"str\", 42, true, null]");
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "← tool:") != NULL && strstr(formatted, "str") != NULL);
}

END_TEST

START_TEST(test_format_tool_result_null_content_path) {
    const char *formatted = ik_format_tool_result(ctx, "tool", "{}");
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "← tool:") != NULL);
}

END_TEST

START_TEST(test_format_tool_call_first_vs_subsequent) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call", "tool", "{\"a\": 1, \"b\": 2}");
    ck_assert_ptr_nonnull(call);
    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "a=") != NULL && strstr(formatted, "b=") != NULL);
}

END_TEST

START_TEST(test_format_tool_call_json_number) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_num", "tool", "42");
    ck_assert_ptr_nonnull(call);
    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert_str_eq(formatted, "→ tool: 42");
}

END_TEST

START_TEST(test_format_tool_result_number) {
    const char *formatted = ik_format_tool_result(ctx, "tool", "42");
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "← tool:") != NULL && strstr(formatted, "42") != NULL);
}

END_TEST

START_TEST(test_format_tool_result_boolean) {
    const char *formatted = ik_format_tool_result(ctx, "tool", "true");
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "← tool:") != NULL && strstr(formatted, "true") != NULL);
}

END_TEST

START_TEST(test_format_tool_result_null_value) {
    const char *formatted = ik_format_tool_result(ctx, "tool", "null");
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "← tool:") != NULL && strstr(formatted, "null") != NULL);
}

END_TEST

START_TEST(test_format_tool_call_real_value) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_real", "tool", "{\"price\": 3.14159}");
    ck_assert_ptr_nonnull(call);
    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "price=") != NULL && strstr(formatted, "3.14") != NULL);
}

END_TEST

START_TEST(test_format_tool_call_null_value) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_null_val", "tool", "{\"value\": null}");
    ck_assert_ptr_nonnull(call);
    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "value=null") != NULL);
}

END_TEST

START_TEST(test_format_tool_result_empty_string) {
    const char *formatted = ik_format_tool_result(ctx, "tool", "\"\"");
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "(no output)") != NULL);
}

END_TEST

START_TEST(test_format_tool_call_empty_object) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_empty", "tool", "{}");
    ck_assert_ptr_nonnull(call);
    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert_str_eq(formatted, "→ tool");
}

END_TEST

START_TEST(test_format_tool_call_nested_object) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_nested", "tool", "{\"nested\": {\"deep\": {\"value\": 42}}}");
    ck_assert_ptr_nonnull(call);
    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "nested=") != NULL);
}

END_TEST

START_TEST(test_format_tool_result_simple_object) {
    const char *formatted = ik_format_tool_result(ctx, "tool", "{\"key\": \"value\"}");
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "← tool:") != NULL);
}

END_TEST

// Mock control for ik_format_yyjson_val_write_wrapper
static bool g_mock_yyjson_val_write_return_null = false;
static bool g_mock_yyjson_doc_get_root_return_null = false;

char *ik_format_yyjson_val_write_wrapper(yyjson_val *val);
yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc);

char *ik_format_yyjson_val_write_wrapper(yyjson_val *val)
{
    return g_mock_yyjson_val_write_return_null ? NULL : yyjson_val_write(val, 0, NULL);
}

yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc)
{
    return g_mock_yyjson_doc_get_root_return_null ? NULL : yyjson_doc_get_root(doc);
}

START_TEST(test_format_tool_call_array_value_oom) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_arr_oom", "tool", "{\"items\": [\"a\", \"b\", \"c\"]}");
    ck_assert_ptr_nonnull(call);
    g_mock_yyjson_val_write_return_null = true;
    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "→ tool") != NULL);
    g_mock_yyjson_val_write_return_null = false;
}
END_TEST

START_TEST(test_format_tool_result_array_numbers_oom) {
    g_mock_yyjson_val_write_return_null = true;
    const char *formatted = ik_format_tool_result(ctx, "tool", "[1, 2, 3]");
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "← tool:") != NULL);
    g_mock_yyjson_val_write_return_null = false;
}

END_TEST

START_TEST(test_format_tool_result_object_oom) {
    g_mock_yyjson_val_write_return_null = true;
    const char *formatted = ik_format_tool_result(ctx, "tool", "{\"key\": \"value\"}");
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "← tool:") != NULL && strstr(formatted, "(no output)") != NULL);
    g_mock_yyjson_val_write_return_null = false;
}

END_TEST

START_TEST(test_format_tool_result_null_root) {
    g_mock_yyjson_doc_get_root_return_null = true;
    const char *formatted = ik_format_tool_result(ctx, "tool", "\"test\"");
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "← tool:") != NULL && strstr(formatted, "(no output)") != NULL);
    g_mock_yyjson_doc_get_root_return_null = false;
}

END_TEST

START_TEST(test_yyjson_obj_iter_init_wrapper_null_obj) {
    yyjson_obj_iter iter;
    ik_format_yyjson_obj_iter_init_wrapper(NULL, &iter);
}

END_TEST

START_TEST(test_yyjson_obj_iter_next_wrapper_null_iter) {
    // Call with NULL iter - defensive branch
    yyjson_val *result = ik_format_yyjson_obj_iter_next_wrapper(NULL);
    ck_assert_ptr_null(result);
}

END_TEST

START_TEST(test_yyjson_obj_iter_get_val_wrapper_null_key) {
    // Call with NULL key - defensive branch
    yyjson_val *result = ik_format_yyjson_obj_iter_get_val_wrapper(NULL);
    ck_assert_ptr_null(result);
}

END_TEST

START_TEST(test_yyjson_val_write_wrapper_null_val) {
    // Call with NULL val - defensive branch
    char *result = ik_format_yyjson_val_write_wrapper(NULL);
    ck_assert_ptr_null(result);
}

END_TEST

static Suite *format_coverage_suite(void)
{
    Suite *s = suite_create("Format Coverage");
    TCase *tc = tcase_create("edge_cases");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_format_tool_call_null_arguments);
    tcase_add_test(tc, test_format_tool_call_json_array);
    tcase_add_test(tc, test_format_tool_call_bool_false);
    tcase_add_test(tc, test_format_tool_call_array_value);
    tcase_add_test(tc, test_format_tool_call_object_value);
    tcase_add_test(tc, test_format_tool_result_zero_length_content);
    tcase_add_test(tc, test_format_tool_result_array_with_numbers);
    tcase_add_test(tc, test_format_tool_result_array_mixed_types);
    tcase_add_test(tc, test_format_tool_result_null_content_path);
    tcase_add_test(tc, test_format_tool_call_first_vs_subsequent);
    tcase_add_test(tc, test_format_tool_call_json_number);
    tcase_add_test(tc, test_format_tool_result_number);
    tcase_add_test(tc, test_format_tool_result_boolean);
    tcase_add_test(tc, test_format_tool_result_null_value);
    tcase_add_test(tc, test_format_tool_call_real_value);
    tcase_add_test(tc, test_format_tool_call_null_value);
    tcase_add_test(tc, test_format_tool_result_empty_string);
    tcase_add_test(tc, test_format_tool_call_empty_object);
    tcase_add_test(tc, test_format_tool_call_nested_object);
    tcase_add_test(tc, test_format_tool_result_simple_object);
    tcase_add_test(tc, test_format_tool_call_array_value_oom);
    tcase_add_test(tc, test_format_tool_result_array_numbers_oom);
    tcase_add_test(tc, test_format_tool_result_object_oom);
    tcase_add_test(tc, test_format_tool_result_null_root);
    tcase_add_test(tc, test_yyjson_obj_iter_init_wrapper_null_obj);
    tcase_add_test(tc, test_yyjson_obj_iter_next_wrapper_null_iter);
    tcase_add_test(tc, test_yyjson_obj_iter_get_val_wrapper_null_key);
    tcase_add_test(tc, test_yyjson_val_write_wrapper_null_val);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = format_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
