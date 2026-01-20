#include "paths.h"
#include "error.h"
#include "../../test_utils_helper.h"
#include <check.h>
#include <stdlib.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    test_paths_cleanup_env();
    talloc_free(test_ctx);
}

START_TEST(test_tools_system_dir) {
    // Setup
    test_paths_setup_env();

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert - system tools dir should equal libexec dir
    const char *system_dir = ik_paths_get_tools_system_dir(paths);
    const char *libexec_dir = ik_paths_get_libexec_dir(paths);
    ck_assert_str_eq(system_dir, libexec_dir);

    test_paths_cleanup_env();
}
END_TEST

START_TEST(test_tools_user_dir) {
    // Setup
    test_paths_setup_env();
    setenv("HOME", "/home/testuser", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert - user tools dir should be ~/.ikigai/tools/ (expanded)
    const char *user_dir = ik_paths_get_tools_user_dir(paths);
    ck_assert_ptr_nonnull(user_dir);
    ck_assert_str_eq(user_dir, "/home/testuser/.ikigai/tools/");

    test_paths_cleanup_env();
}
END_TEST

START_TEST(test_tools_project_dir) {
    // Setup
    test_paths_setup_env();

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert - project tools dir should be .ikigai/tools/
    const char *project_dir = ik_paths_get_tools_project_dir(paths);
    ck_assert_ptr_nonnull(project_dir);
    ck_assert_str_eq(project_dir, ".ikigai/tools/");

    test_paths_cleanup_env();
}
END_TEST

START_TEST(test_tools_all_three_accessible) {
    // Setup
    test_paths_setup_env();
    setenv("HOME", "/home/testuser", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert - all three getters should work simultaneously
    const char *system_dir = ik_paths_get_tools_system_dir(paths);
    const char *user_dir = ik_paths_get_tools_user_dir(paths);
    const char *project_dir = ik_paths_get_tools_project_dir(paths);

    ck_assert_ptr_nonnull(system_dir);
    ck_assert_ptr_nonnull(user_dir);
    ck_assert_ptr_nonnull(project_dir);

    // Verify they're different values
    ck_assert_str_ne(system_dir, user_dir);
    ck_assert_str_ne(system_dir, project_dir);
    ck_assert_str_ne(user_dir, project_dir);

    test_paths_cleanup_env();
}
END_TEST

START_TEST(test_tools_user_dir_expands_tilde) {
    // Setup
    test_paths_setup_env();
    setenv("HOME", "/custom/home/path", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert - tilde should be expanded to custom HOME value
    const char *user_dir = ik_paths_get_tools_user_dir(paths);
    ck_assert_ptr_nonnull(user_dir);
    ck_assert_str_eq(user_dir, "/custom/home/path/.ikigai/tools/");

    test_paths_cleanup_env();
}
END_TEST

START_TEST(test_tools_project_dir_relative) {
    // Setup
    test_paths_setup_env();

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert - project dir should be relative path
    const char *project_dir = ik_paths_get_tools_project_dir(paths);
    ck_assert_ptr_nonnull(project_dir);
    ck_assert_str_eq(project_dir, ".ikigai/tools/");

    // Verify it doesn't start with /
    ck_assert(project_dir[0] != '/');

    test_paths_cleanup_env();
}
END_TEST

static Suite *paths_tool_dirs_suite(void)
{
    Suite *s = suite_create("Paths Tool Directories");
    TCase *tc = tcase_create("Tool Dirs");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_tools_system_dir);
    tcase_add_test(tc, test_tools_user_dir);
    tcase_add_test(tc, test_tools_project_dir);
    tcase_add_test(tc, test_tools_all_three_accessible);
    tcase_add_test(tc, test_tools_user_dir_expands_tilde);
    tcase_add_test(tc, test_tools_project_dir_relative);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = paths_tool_dirs_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
