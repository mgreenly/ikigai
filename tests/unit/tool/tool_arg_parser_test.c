#include <check.h>
#include <stdbool.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/tool.h"

// Test fixtures
static TALLOC_CTX *ctx = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
}

// ============================================================================
// ik_tool_arg_get_string tests
// ============================================================================

START_TEST(test_tool_arg_get_string_valid) {
    const char *arguments_json = "{\"pattern\": \"*.c\", \"path\": \"src/\"}";

    char *result = ik_tool_arg_get_string(ctx, arguments_json, "pattern");
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "*.c");
}
END_TEST

START_TEST(test_tool_arg_get_string_second_param) {
    const char *arguments_json = "{\"pattern\": \"*.c\", \"path\": \"src/\"}";

    char *result = ik_tool_arg_get_string(ctx, arguments_json, "path");
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "src/");
}

END_TEST

START_TEST(test_tool_arg_get_string_missing_key) {
    const char *arguments_json = "{\"pattern\": \"*.c\"}";

    char *result = ik_tool_arg_get_string(ctx, arguments_json, "nonexistent");
    ck_assert_ptr_null(result);
}

END_TEST

START_TEST(test_tool_arg_get_string_wrong_type_number) {
    const char *arguments_json = "{\"count\": 42}";

    char *result = ik_tool_arg_get_string(ctx, arguments_json, "count");
    ck_assert_ptr_null(result);
}

END_TEST

START_TEST(test_tool_arg_get_string_wrong_type_bool) {
    const char *arguments_json = "{\"enabled\": true}";

    char *result = ik_tool_arg_get_string(ctx, arguments_json, "enabled");
    ck_assert_ptr_null(result);
}

END_TEST

START_TEST(test_tool_arg_get_string_malformed_json) {
    const char *arguments_json = "{\"pattern\": invalid}";

    char *result = ik_tool_arg_get_string(ctx, arguments_json, "pattern");
    ck_assert_ptr_null(result);
}

END_TEST

START_TEST(test_tool_arg_get_string_null_arguments) {
    char *result = ik_tool_arg_get_string(ctx, NULL, "pattern");
    ck_assert_ptr_null(result);
}

END_TEST

START_TEST(test_tool_arg_get_string_empty_json) {
    const char *arguments_json = "{}";

    char *result = ik_tool_arg_get_string(ctx, arguments_json, "pattern");
    ck_assert_ptr_null(result);
}

END_TEST

START_TEST(test_tool_arg_get_string_talloc_hierarchy) {
    const char *arguments_json = "{\"path\": \"/etc/hosts\"}";

    char *result = ik_tool_arg_get_string(ctx, arguments_json, "path");
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "/etc/hosts");
    ck_assert_ptr_eq(talloc_parent(result), ctx);
}

END_TEST

START_TEST(test_tool_arg_get_string_null_key) {
    const char *arguments_json = "{\"pattern\": \"*.c\"}";

    char *result = ik_tool_arg_get_string(ctx, arguments_json, NULL);
    ck_assert_ptr_null(result);
}

END_TEST

START_TEST(test_tool_arg_get_string_non_object_json) {
    const char *arguments_json = "[\"array\", \"not\", \"object\"]";

    char *result = ik_tool_arg_get_string(ctx, arguments_json, "pattern");
    ck_assert_ptr_null(result);
}

END_TEST
// ============================================================================
// ik_tool_arg_get_int tests
// ============================================================================

START_TEST(test_tool_arg_get_int_valid) {
    const char *arguments_json = "{\"timeout\": 30, \"retries\": 3}";
    int value = -1;

    bool result = ik_tool_arg_get_int(arguments_json, "timeout", &value);
    ck_assert(result == true);
    ck_assert_int_eq(value, 30);
}

END_TEST

START_TEST(test_tool_arg_get_int_second_param) {
    const char *arguments_json = "{\"timeout\": 30, \"retries\": 3}";
    int value = -1;

    bool result = ik_tool_arg_get_int(arguments_json, "retries", &value);
    ck_assert(result == true);
    ck_assert_int_eq(value, 3);
}

END_TEST

START_TEST(test_tool_arg_get_int_missing_key) {
    const char *arguments_json = "{\"timeout\": 30}";
    int value = -1;

    bool result = ik_tool_arg_get_int(arguments_json, "nonexistent", &value);
    ck_assert(result == false);
    ck_assert_int_eq(value, -1);
}

END_TEST

START_TEST(test_tool_arg_get_int_wrong_type_string) {
    const char *arguments_json = "{\"count\": \"abc\"}";
    int value = -1;

    bool result = ik_tool_arg_get_int(arguments_json, "count", &value);
    ck_assert(result == false);
    ck_assert_int_eq(value, -1);
}

END_TEST

START_TEST(test_tool_arg_get_int_null_out_value) {
    const char *arguments_json = "{\"timeout\": 30}";

    bool result = ik_tool_arg_get_int(arguments_json, "timeout", NULL);
    ck_assert(result == false);
}

END_TEST

START_TEST(test_tool_arg_get_int_malformed_json) {
    const char *arguments_json = "{\"timeout\": invalid}";
    int value = -1;

    bool result = ik_tool_arg_get_int(arguments_json, "timeout", &value);
    ck_assert(result == false);
    ck_assert_int_eq(value, -1);
}

END_TEST

START_TEST(test_tool_arg_get_int_null_arguments) {
    int value = -1;

    bool result = ik_tool_arg_get_int(NULL, "timeout", &value);
    ck_assert(result == false);
    ck_assert_int_eq(value, -1);
}

END_TEST

START_TEST(test_tool_arg_get_int_zero) {
    const char *arguments_json = "{\"timeout\": 0}";
    int value = -1;

    bool result = ik_tool_arg_get_int(arguments_json, "timeout", &value);
    ck_assert(result == true);
    ck_assert_int_eq(value, 0);
}

END_TEST

START_TEST(test_tool_arg_get_int_negative) {
    const char *arguments_json = "{\"offset\": -10}";
    int value = 0;

    bool result = ik_tool_arg_get_int(arguments_json, "offset", &value);
    ck_assert(result == true);
    ck_assert_int_eq(value, -10);
}

END_TEST

START_TEST(test_tool_arg_get_int_null_key) {
    const char *arguments_json = "{\"timeout\": 30}";
    int value = -1;

    bool result = ik_tool_arg_get_int(arguments_json, NULL, &value);
    ck_assert(result == false);
    ck_assert_int_eq(value, -1);
}

END_TEST

START_TEST(test_tool_arg_get_int_non_object_json) {
    const char *arguments_json = "[1, 2, 3]";
    int value = -1;

    bool result = ik_tool_arg_get_int(arguments_json, "timeout", &value);
    ck_assert(result == false);
    ck_assert_int_eq(value, -1);
}

END_TEST

// Test suite
static Suite *tool_arg_parser_suite(void)
{
    Suite *s = suite_create("Tool Argument Parser");

    TCase *tc_arg_get_string = tcase_create("Get String");
    tcase_set_timeout(tc_arg_get_string, 30);
    tcase_add_checked_fixture(tc_arg_get_string, setup, teardown);
    tcase_add_test(tc_arg_get_string, test_tool_arg_get_string_valid);
    tcase_add_test(tc_arg_get_string, test_tool_arg_get_string_second_param);
    tcase_add_test(tc_arg_get_string, test_tool_arg_get_string_missing_key);
    tcase_add_test(tc_arg_get_string, test_tool_arg_get_string_wrong_type_number);
    tcase_add_test(tc_arg_get_string, test_tool_arg_get_string_wrong_type_bool);
    tcase_add_test(tc_arg_get_string, test_tool_arg_get_string_malformed_json);
    tcase_add_test(tc_arg_get_string, test_tool_arg_get_string_null_arguments);
    tcase_add_test(tc_arg_get_string, test_tool_arg_get_string_empty_json);
    tcase_add_test(tc_arg_get_string, test_tool_arg_get_string_talloc_hierarchy);
    tcase_add_test(tc_arg_get_string, test_tool_arg_get_string_null_key);
    tcase_add_test(tc_arg_get_string, test_tool_arg_get_string_non_object_json);
    suite_add_tcase(s, tc_arg_get_string);

    TCase *tc_arg_get_int = tcase_create("Get Integer");
    tcase_set_timeout(tc_arg_get_int, 30);
    tcase_add_checked_fixture(tc_arg_get_int, setup, teardown);
    tcase_add_test(tc_arg_get_int, test_tool_arg_get_int_valid);
    tcase_add_test(tc_arg_get_int, test_tool_arg_get_int_second_param);
    tcase_add_test(tc_arg_get_int, test_tool_arg_get_int_missing_key);
    tcase_add_test(tc_arg_get_int, test_tool_arg_get_int_wrong_type_string);
    tcase_add_test(tc_arg_get_int, test_tool_arg_get_int_null_out_value);
    tcase_add_test(tc_arg_get_int, test_tool_arg_get_int_malformed_json);
    tcase_add_test(tc_arg_get_int, test_tool_arg_get_int_null_arguments);
    tcase_add_test(tc_arg_get_int, test_tool_arg_get_int_zero);
    tcase_add_test(tc_arg_get_int, test_tool_arg_get_int_negative);
    tcase_add_test(tc_arg_get_int, test_tool_arg_get_int_null_key);
    tcase_add_test(tc_arg_get_int, test_tool_arg_get_int_non_object_json);
    suite_add_tcase(s, tc_arg_get_int);

    return s;
}

int main(void)
{
    Suite *s = tool_arg_parser_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
