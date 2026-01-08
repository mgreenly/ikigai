#include "../../test_constants.h"
#include <check.h>
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

START_TEST(test_tool_truncate_output_null) {
    ck_assert_ptr_null(ik_tool_truncate_output(ctx, NULL, 1024));
    talloc_free(ctx);
    ctx = talloc_new(NULL);
}
END_TEST

START_TEST(test_tool_truncate_output_empty) {
    char *result = ik_tool_truncate_output(ctx, "", 1024);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "");
    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST

START_TEST(test_tool_truncate_output_under_limit) {
    char *result = ik_tool_truncate_output(ctx, "Hello, World!", 100);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "Hello, World!");
    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST

START_TEST(test_tool_truncate_output_at_limit) {
    char *result = ik_tool_truncate_output(ctx, "12345", 5);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "12345");
    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST

START_TEST(test_tool_truncate_output_over_limit) {
    const char *output = "This is a very long string that exceeds the limit";
    char *result = ik_tool_truncate_output(ctx, output, 10);
    ck_assert_ptr_nonnull(result);
    ck_assert(strncmp(result, "This is a ", 10) == 0);
    ck_assert(strstr(result, "[Output truncated:") != NULL);
    ck_assert(strstr(result, "showing first 10 of") != NULL);
    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST

START_TEST(test_tool_truncate_output_zero_limit) {
    char *result = ik_tool_truncate_output(ctx, "test", 0);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "[Output truncated:") != NULL);
    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST

// Test suite
static Suite *tool_truncate_suite(void)
{
    Suite *s = suite_create("Tool Truncate");

    TCase *tc_truncate = tcase_create("Truncate Output");
    tcase_set_timeout(tc_truncate, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_truncate, setup, teardown);
    tcase_add_test(tc_truncate, test_tool_truncate_output_null);
    tcase_add_test(tc_truncate, test_tool_truncate_output_empty);
    tcase_add_test(tc_truncate, test_tool_truncate_output_under_limit);
    tcase_add_test(tc_truncate, test_tool_truncate_output_at_limit);
    tcase_add_test(tc_truncate, test_tool_truncate_output_over_limit);
    tcase_add_test(tc_truncate, test_tool_truncate_output_zero_limit);
    suite_add_tcase(s, tc_truncate);

    return s;
}

int main(void)
{
    Suite *s = tool_truncate_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
