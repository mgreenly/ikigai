#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

START_TEST(test_config_expand_tilde) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *home = getenv("HOME");
    ck_assert_ptr_nonnull(home);

    // Test path with tilde
    res_t res = ik_cfg_expand_tilde(ctx, "~/test/path");
    ck_assert(is_ok(&res));
    char *expanded = res.ok;
    ck_assert_ptr_nonnull(expanded);

    char expected[256];
    snprintf(expected, sizeof(expected), "%s/test/path", home);
    ck_assert_str_eq(expanded, expected);

    // Test path without tilde (should return unchanged)
    res = ik_cfg_expand_tilde(ctx, "/absolute/path");
    ck_assert(is_ok(&res));
    char *no_tilde = res.ok;
    ck_assert_ptr_nonnull(no_tilde);
    ck_assert_str_eq(no_tilde, "/absolute/path");

    talloc_free(ctx);
}

END_TEST START_TEST(test_config_expand_tilde_home_unset)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Save original HOME
    const char *original_home = getenv("HOME");
    char *saved_home = NULL;
    if (original_home) {
        saved_home = strdup(original_home);
    }

    // Unset HOME
    unsetenv("HOME");

    // Should return error when HOME is not set
    res_t result = ik_cfg_expand_tilde(ctx, "~/test");
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);

    // Restore HOME
    if (saved_home) {
        setenv("HOME", saved_home, 1);
        free(saved_home);
    }

    talloc_free(ctx);
}

END_TEST START_TEST(test_config_load_tilde_home_unset)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Save original HOME
    const char *original_home = getenv("HOME");
    char *saved_home = NULL;
    if (original_home) {
        saved_home = strdup(original_home);
    }

    // Unset HOME
    unsetenv("HOME");

    // Try to load with tilde path - should fail
    res_t result = ik_cfg_load(ctx, "~/test/config.json");
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);

    // Restore HOME
    if (saved_home) {
        setenv("HOME", saved_home, 1);
        free(saved_home);
    }

    talloc_free(ctx);
}

END_TEST static Suite *config_tilde_suite(void)
{
    Suite *s = suite_create("Config Tilde");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_config_expand_tilde);
    tcase_add_test(tc_core, test_config_expand_tilde_home_unset);
    tcase_add_test(tc_core, test_config_load_tilde_home_unset);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = config_tilde_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
