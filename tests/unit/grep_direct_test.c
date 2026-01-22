#include "tools/grep/grep.h"

#include <check.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>

// Wrapper function declarations
int glob_(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob);
void globfree_(glob_t *pglob);
int posix_stat_(const char *pathname, struct stat *statbuf);
FILE *fopen_(const char *pathname, const char *mode);
int fclose_(FILE *stream);

static void *test_ctx = NULL;

// Mock setup
static int mock_glob_return = 0;
static int mock_posix_stat_return = 0;
static FILE *mock_fopen_return = NULL;
static struct stat mock_stat_buf;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    mock_glob_return = 0;
    mock_posix_stat_return = 0;
    mock_fopen_return = NULL;
    memset(&mock_stat_buf, 0, sizeof(mock_stat_buf));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

int glob_(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob)
{
    if (mock_glob_return != 0) {
        return mock_glob_return;
    }
    return glob(pattern, flags, errfunc, pglob);
}

void globfree_(glob_t *pglob)
{
    globfree(pglob);
}

int posix_stat_(const char *pathname, struct stat *statbuf)
{
    if (mock_posix_stat_return != 0) {
        return mock_posix_stat_return;
    }
    if (mock_stat_buf.st_mode != 0) {
        *statbuf = mock_stat_buf;
        return 0;
    }
    return stat(pathname, statbuf);
}

FILE *fopen_(const char *pathname, const char *mode)
{
    if (mock_fopen_return != NULL) {
        return mock_fopen_return;
    }
    return fopen(pathname, mode);
}

int fclose_(FILE *stream)
{
    return fclose(stream);
}

START_TEST(test_grep_null_params) {
    grep_result_t result;
    int32_t ret = grep_search(test_ctx, NULL, &result);
    ck_assert_int_eq(ret, -1);
}
END_TEST

START_TEST(test_grep_null_pattern) {
    grep_params_t params = { .pattern = NULL, .glob = NULL, .path = NULL };
    grep_result_t result;
    int32_t ret = grep_search(test_ctx, &params, &result);
    ck_assert_int_eq(ret, -1);
}
END_TEST

START_TEST(test_grep_null_output) {
    grep_params_t params = { .pattern = "test", .glob = NULL, .path = NULL };
    int32_t ret = grep_search(test_ctx, &params, NULL);
    ck_assert_int_eq(ret, -1);
}
END_TEST

START_TEST(test_grep_invalid_pattern) {
    grep_params_t params = { .pattern = "[invalid", .glob = NULL, .path = NULL };
    grep_result_t result;
    int32_t ret = grep_search(test_ctx, &params, &result);
    ck_assert_int_eq(ret, -1);
}
END_TEST

START_TEST(test_grep_glob_error) {
    mock_glob_return = GLOB_ABORTED;
    grep_params_t params = { .pattern = "test", .glob = NULL, .path = "." };
    grep_result_t result;
    int32_t ret = grep_search(test_ctx, &params, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_str_eq(result.output, "");
    ck_assert_int_eq(result.count, 0);
}
END_TEST

START_TEST(test_grep_success_no_matches) {
    // Use a non-existent path to ensure no matches
    grep_params_t params = { .pattern = "test", .glob = "*.c", .path = "/nonexistent/path/12345" };
    grep_result_t result;
    int32_t ret = grep_search(test_ctx, &params, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_str_eq(result.output, "");
    ck_assert_int_eq(result.count, 0);
}
END_TEST

START_TEST(test_grep_success_with_matches) {
    grep_params_t params = { .pattern = "START_TEST", .glob = "*.c", .path = "tests/unit" };
    grep_result_t result;
    int32_t ret = grep_search(test_ctx, &params, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert(result.count > 0);
    ck_assert(strstr(result.output, "START_TEST") != NULL);
}
END_TEST

START_TEST(test_grep_with_glob_pattern) {
    grep_params_t params = { .pattern = "include", .glob = "*.h", .path = "src" };
    grep_result_t result;
    int32_t ret = grep_search(test_ctx, &params, &result);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_grep_no_glob_pattern) {
    grep_params_t params = { .pattern = "include", .glob = NULL, .path = "src" };
    grep_result_t result;
    int32_t ret = grep_search(test_ctx, &params, &result);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_grep_default_path) {
    grep_params_t params = { .pattern = "test", .glob = NULL, .path = NULL };
    grep_result_t result;
    int32_t ret = grep_search(test_ctx, &params, &result);
    ck_assert_int_eq(ret, 0);
}
END_TEST

static Suite *grep_suite(void)
{
    Suite *s = suite_create("grep_direct");
    TCase *tc = tcase_create("core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_grep_null_params);
    tcase_add_test(tc, test_grep_null_pattern);
    tcase_add_test(tc, test_grep_null_output);
    tcase_add_test(tc, test_grep_invalid_pattern);
    tcase_add_test(tc, test_grep_glob_error);
    tcase_add_test(tc, test_grep_success_no_matches);
    tcase_add_test(tc, test_grep_success_with_matches);
    tcase_add_test(tc, test_grep_with_glob_pattern);
    tcase_add_test(tc, test_grep_no_glob_pattern);
    tcase_add_test(tc, test_grep_default_path);

    suite_add_tcase(s, tc);
    return s;
}

int32_t main(void)
{
    int32_t number_failed;
    Suite *s = grep_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/grep_direct_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
