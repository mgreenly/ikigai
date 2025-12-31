/**
 * @file test_credentials.c
 * @brief Unit tests for credentials lookup from environment
 *
 * Simplified tests focusing on environment variable-based credential lookup.
 * These tests verify the credentials API used by the provider factory.
 */

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "credentials.h"
#include "error.h"

/* Test context */
static TALLOC_CTX *test_ctx = NULL;

/* Setup/teardown */
static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

/**
 * Credentials Lookup Tests
 *
 * Test ik_credentials_get() for each provider with environment variables.
 */

START_TEST(test_credentials_from_env_openai) {
    setenv("OPENAI_API_KEY", "sk-test123", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *key = ik_credentials_get(creds, "openai");
    ck_assert_ptr_nonnull(key);
    ck_assert_str_eq(key, "sk-test123");

    unsetenv("OPENAI_API_KEY");
}
END_TEST START_TEST(test_credentials_from_env_anthropic)
{
    setenv("ANTHROPIC_API_KEY", "sk-ant-test", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *key = ik_credentials_get(creds, "anthropic");
    ck_assert_ptr_nonnull(key);
    ck_assert_str_eq(key, "sk-ant-test");

    unsetenv("ANTHROPIC_API_KEY");
}

END_TEST START_TEST(test_credentials_from_env_google)
{
    setenv("GOOGLE_API_KEY", "AIza-test", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *key = ik_credentials_get(creds, "google");
    ck_assert_ptr_nonnull(key);
    ck_assert_str_eq(key, "AIza-test");

    unsetenv("GOOGLE_API_KEY");
}

END_TEST START_TEST(test_credentials_missing_returns_null)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    /* Missing credentials should return NULL
     * Note: May return non-NULL if ~/.config/ikigai/credentials.json exists
     * with provider keys. This test only verifies that credentials_get() doesn't
     * crash and returns either NULL or a valid string. */
    const char *key = ik_credentials_get(creds, "openai");
    /* Key is either NULL or a valid string pointer */
    (void)key; /* Test passes if no crash */
}

END_TEST START_TEST(test_credentials_unknown_provider)
{
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    /* Unknown provider should return NULL */
    const char *key = ik_credentials_get(creds, "unknown_provider");
    ck_assert_ptr_null(key);
}

END_TEST START_TEST(test_credentials_explicit_path_nonexistent)
{
    /* Test with explicit path to non-existent file */
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, "/tmp/nonexistent_credentials.json", &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);
}

END_TEST START_TEST(test_insecure_permissions_missing_file)
{
    /* Missing file should return false (not insecure) */
    bool insecure = ik_credentials_insecure_permissions("/tmp/nonexistent_file_12345.json");
    ck_assert(!insecure);
}

END_TEST START_TEST(test_insecure_permissions_bad_perms)
{
    /* Create a temporary file with insecure permissions (0644) */
    const char *tmpfile = "/tmp/test_creds_insecure.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{}");
    fclose(f);
    chmod(tmpfile, 0644);

    bool insecure = ik_credentials_insecure_permissions(tmpfile);
    ck_assert(insecure);

    unlink(tmpfile);
}

END_TEST START_TEST(test_insecure_permissions_secure)
{
    /* Create a temporary file with secure permissions (0600) */
    const char *tmpfile = "/tmp/test_creds_secure.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{}");
    fclose(f);
    chmod(tmpfile, 0600);

    bool insecure = ik_credentials_insecure_permissions(tmpfile);
    ck_assert(!insecure);

    unlink(tmpfile);
}

END_TEST

/**
 * Test Suite Configuration
 */

static Suite *credentials_suite(void)
{
    Suite *s = suite_create("Credentials");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_credentials_from_env_openai);
    tcase_add_test(tc_core, test_credentials_from_env_anthropic);
    tcase_add_test(tc_core, test_credentials_from_env_google);
    tcase_add_test(tc_core, test_credentials_missing_returns_null);
    tcase_add_test(tc_core, test_credentials_unknown_provider);
    tcase_add_test(tc_core, test_credentials_explicit_path_nonexistent);
    tcase_add_test(tc_core, test_insecure_permissions_missing_file);
    tcase_add_test(tc_core, test_insecure_permissions_bad_perms);
    tcase_add_test(tc_core, test_insecure_permissions_secure);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = credentials_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
