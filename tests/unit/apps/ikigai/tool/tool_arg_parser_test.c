#include "tests/test_constants.h"
#include <check.h>
#include <stdbool.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/tool.h"

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

// Test suite
static Suite *tool_arg_parser_suite(void)
{
    Suite *s = suite_create("Tool Argument Parser");

    TCase *tc_arg_get_string = tcase_create("Get String");
    tcase_set_timeout(tc_arg_get_string, IK_TEST_TIMEOUT);
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

    return s;
}

int main(void)
{
    Suite *s = tool_arg_parser_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/tool/tool_arg_parser_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
