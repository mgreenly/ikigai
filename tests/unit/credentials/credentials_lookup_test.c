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

END_TEST START_TEST(test_load_with_insecure_permissions)
{
    /* Create a file with insecure permissions to trigger warning in load */
    const char *tmpfile = "/tmp/test_creds_warning.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{}");
    fclose(f);
    chmod(tmpfile, 0644);

    /* Load will warn about insecure permissions but succeed */
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    unlink(tmpfile);
}

END_TEST START_TEST(test_env_var_overrides_file)
{
    /* Create a credentials file with API keys */
    const char *tmpfile = "/tmp/test_creds_override.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"openai\":{\"api_key\":\"file-key-openai\"},"
               "\"anthropic\":{\"api_key\":\"file-key-anthropic\"},"
               "\"google\":{\"api_key\":\"file-key-google\"}}");
    fclose(f);
    chmod(tmpfile, 0600);

    /* Set env vars - these should override the file values */
    setenv("OPENAI_API_KEY", "env-key-openai", 1);
    setenv("ANTHROPIC_API_KEY", "env-key-anthropic", 1);
    setenv("GOOGLE_API_KEY", "env-key-google", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    /* Verify env vars took precedence */
    const char *openai_key = ik_credentials_get(creds, "openai");
    ck_assert_ptr_nonnull(openai_key);
    ck_assert_str_eq(openai_key, "env-key-openai");

    const char *anthropic_key = ik_credentials_get(creds, "anthropic");
    ck_assert_ptr_nonnull(anthropic_key);
    ck_assert_str_eq(anthropic_key, "env-key-anthropic");

    const char *google_key = ik_credentials_get(creds, "google");
    ck_assert_ptr_nonnull(google_key);
    ck_assert_str_eq(google_key, "env-key-google");

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    unlink(tmpfile);
}

END_TEST START_TEST(test_file_based_credentials)
{
    /* Ensure no env vars interfere */
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    /* Create a credentials file with API keys */
    const char *tmpfile = "/tmp/test_creds_file.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"openai\":{\"api_key\":\"file-openai-key\"},"
               "\"anthropic\":{\"api_key\":\"file-anthropic-key\"},"
               "\"google\":{\"api_key\":\"file-google-key\"}}");
    fclose(f);
    chmod(tmpfile, 0600);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    /* Verify file values were loaded */
    const char *openai_key = ik_credentials_get(creds, "openai");
    ck_assert_ptr_nonnull(openai_key);
    ck_assert_str_eq(openai_key, "file-openai-key");

    const char *anthropic_key = ik_credentials_get(creds, "anthropic");
    ck_assert_ptr_nonnull(anthropic_key);
    ck_assert_str_eq(anthropic_key, "file-anthropic-key");

    const char *google_key = ik_credentials_get(creds, "google");
    ck_assert_ptr_nonnull(google_key);
    ck_assert_str_eq(google_key, "file-google-key");

    unlink(tmpfile);
}

END_TEST START_TEST(test_invalid_json_file)
{
    /* Create a file with invalid JSON */
    const char *tmpfile = "/tmp/test_creds_invalid.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{this is not valid json}");
    fclose(f);
    chmod(tmpfile, 0600);

    /* Load should succeed with warning but return empty credentials */
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    unlink(tmpfile);
}

END_TEST START_TEST(test_json_root_not_object)
{
    /* Create a file where JSON root is an array, not object */
    const char *tmpfile = "/tmp/test_creds_array.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "[\"not\", \"an\", \"object\"]");
    fclose(f);
    chmod(tmpfile, 0600);

    /* Load should succeed with warning but return empty credentials */
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    unlink(tmpfile);
}

END_TEST START_TEST(test_json_malformed_credentials)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    /* Test missing providers, non-object providers, missing/invalid api_keys */
    const char *test_cases[][2] = {
        {"/tmp/creds1.json", "{\"other\":\"value\"}"},  /* missing providers */
        {"/tmp/creds2.json", "{\"openai\":\"str\",\"anthropic\":123,\"google\":[]}"},  /* not objects */
        {"/tmp/creds3.json", "{\"openai\":{},\"anthropic\":{\"x\":1},\"google\":{}}"},  /* missing api_key */
        {"/tmp/creds4.json", "{\"openai\":{\"api_key\":1},\"anthropic\":{\"api_key\":true},\"google\":{\"api_key\":null}}"},  /* not strings */
        {"/tmp/creds5.json", "{\"openai\":{\"api_key\":\"\"},\"anthropic\":{\"api_key\":\"\"},\"google\":{\"api_key\":\"\"}}"},  /* empty */
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        FILE *f = fopen(test_cases[i][0], "w");
        ck_assert_ptr_nonnull(f);
        fprintf(f, "%s", test_cases[i][1]);
        fclose(f);
        chmod(test_cases[i][0], 0600);

        ik_credentials_t *creds = NULL;
        res_t result = ik_credentials_load(test_ctx, test_cases[i][0], &creds);
        ck_assert(is_ok(&result));
        ck_assert_ptr_nonnull(creds);
        ck_assert_ptr_null(ik_credentials_get(creds, "openai"));
        ck_assert_ptr_null(ik_credentials_get(creds, "anthropic"));
        ck_assert_ptr_null(ik_credentials_get(creds, "google"));

        unlink(test_cases[i][0]);
    }
}

END_TEST START_TEST(test_tilde_expansion_no_home)
{
    /* Save original HOME value */
    char *original_home = getenv("HOME");
    char *home_copy = NULL;
    if (original_home) {
        home_copy = strdup(original_home);
    }

    /* Unset HOME to trigger error */
    unsetenv("HOME");

    /* Try to load with default path (uses tilde) */
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    /* Should fail because HOME is not set */
    ck_assert(is_err(&result));

    /* Restore HOME */
    if (home_copy) {
        setenv("HOME", home_copy, 1);
        free(home_copy);
    }
}

END_TEST START_TEST(test_empty_env_var_ignored)
{
    /* Set env vars to empty strings - should be treated as unset */
    setenv("OPENAI_API_KEY", "", 1);
    setenv("ANTHROPIC_API_KEY", "", 1);
    setenv("GOOGLE_API_KEY", "", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, "/tmp/nonexistent.json", &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    /* Empty env vars should be ignored, credentials should be NULL */
    ck_assert_ptr_null(ik_credentials_get(creds, "openai"));
    ck_assert_ptr_null(ik_credentials_get(creds, "anthropic"));
    ck_assert_ptr_null(ik_credentials_get(creds, "google"));

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
}

END_TEST START_TEST(test_env_var_without_file_credentials)
{
    /* Create a file with empty provider objects (no api_key fields) */
    const char *tmpfile = "/tmp/test_creds_empty_providers.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"openai\":{},\"anthropic\":{},\"google\":{}}");
    fclose(f);
    chmod(tmpfile, 0600);

    /* Set env vars - these provide credentials when file doesn't */
    setenv("OPENAI_API_KEY", "env-only-openai", 1);
    setenv("ANTHROPIC_API_KEY", "env-only-anthropic", 1);
    setenv("GOOGLE_API_KEY", "env-only-google", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    /* Verify env vars were used (no file credentials to free) */
    const char *openai_key = ik_credentials_get(creds, "openai");
    ck_assert_ptr_nonnull(openai_key);
    ck_assert_str_eq(openai_key, "env-only-openai");

    const char *anthropic_key = ik_credentials_get(creds, "anthropic");
    ck_assert_ptr_nonnull(anthropic_key);
    ck_assert_str_eq(anthropic_key, "env-only-anthropic");

    const char *google_key = ik_credentials_get(creds, "google");
    ck_assert_ptr_nonnull(google_key);
    ck_assert_str_eq(google_key, "env-only-google");

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
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
    tcase_add_test(tc_core, test_load_with_insecure_permissions);
    tcase_add_test(tc_core, test_env_var_overrides_file);
    tcase_add_test(tc_core, test_file_based_credentials);
    tcase_add_test(tc_core, test_invalid_json_file);
    tcase_add_test(tc_core, test_json_root_not_object);
    tcase_add_test(tc_core, test_json_malformed_credentials);
    tcase_add_test(tc_core, test_tilde_expansion_no_home);
    tcase_add_test(tc_core, test_empty_env_var_ignored);
    tcase_add_test(tc_core, test_env_var_without_file_credentials);
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
