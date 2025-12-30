/**
 * @file credentials_coverage_test.c
 * @brief Additional coverage tests for credentials.c
 *
 * This test file provides additional coverage for specific branches
 * and code paths in credentials.c that weren't covered by existing tests.
 */

#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "../../../src/credentials.h"
#include "../../../src/error.h"

// Test fixture
static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Helper to create a temporary credentials file
static char *create_temp_credentials(const char *content)
{
    char *path = talloc_asprintf(test_ctx, "/tmp/ikigai_creds_cov_%d.json", getpid());
    FILE *f = fopen(path, "w");
    if (!f) {
        ck_abort_msg("Failed to create temp file: %s", strerror(errno));
    }
    fprintf(f, "%s", content);
    fclose(f);
    chmod(path, 0600);
    return path;
}

// Test: Path without tilde (tests expand_tilde non-tilde branch)
START_TEST(test_non_tilde_path)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *json = "{ \"openai\": { \"api_key\": \"test-key\" } }";
    char *path = talloc_asprintf(test_ctx, "/tmp/ikigai_creds_notilde_%d.json", getpid());
    FILE *f = fopen(path, "w");
    fprintf(f, "%s", json);
    fclose(f);
    chmod(path, 0600);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_str_eq(creds->openai_api_key, "test-key");

    unlink(path);
}

END_TEST

// Test: Successfully parse valid JSON with all providers
START_TEST(test_successful_json_parsing)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *json = "{\n"
                       "  \"openai\": { \"api_key\": \"openai-key\" },\n"
                       "  \"anthropic\": { \"api_key\": \"anthropic-key\" },\n"
                       "  \"google\": { \"api_key\": \"google-key\" }\n"
                       "}";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_str_eq(creds->openai_api_key, "openai-key");
    ck_assert_str_eq(creds->anthropic_api_key, "anthropic-key");
    ck_assert_str_eq(creds->google_api_key, "google-key");

    unlink(path);
}

END_TEST

// Test: Empty string values in API keys (tests empty string branch)
START_TEST(test_empty_string_api_keys)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *json = "{\n"
                       "  \"openai\": { \"api_key\": \"\" },\n"
                       "  \"anthropic\": { \"api_key\": \"\" },\n"
                       "  \"google\": { \"api_key\": \"\" }\n"
                       "}";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    // Empty strings should not be loaded
    ck_assert_ptr_null(creds->openai_api_key);
    ck_assert_ptr_null(creds->anthropic_api_key);
    ck_assert_ptr_null(creds->google_api_key);

    unlink(path);
}

END_TEST

// Test: File has credentials, env vars override (tests override branches)
START_TEST(test_file_then_env_override)
{
    // File has all three providers
    const char *json = "{\n"
                       "  \"openai\": { \"api_key\": \"file-openai\" },\n"
                       "  \"anthropic\": { \"api_key\": \"file-anthropic\" },\n"
                       "  \"google\": { \"api_key\": \"file-google\" }\n"
                       "}";
    char *path = create_temp_credentials(json);

    // Set all env vars to override
    setenv("OPENAI_API_KEY", "env-openai", 1);
    setenv("ANTHROPIC_API_KEY", "env-anthropic", 1);
    setenv("GOOGLE_API_KEY", "env-google", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    // All should be overridden by env vars
    ck_assert_str_eq(creds->openai_api_key, "env-openai");
    ck_assert_str_eq(creds->anthropic_api_key, "env-anthropic");
    ck_assert_str_eq(creds->google_api_key, "env-google");

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    unlink(path);
}

END_TEST

// Test: Insecure permissions warning path
START_TEST(test_insecure_permissions_warning)
{
    const char *json = "{ \"openai\": { \"api_key\": \"test-key\" } }";
    char *path = create_temp_credentials(json);

    // Set insecure permissions (world-readable)
    chmod(path, 0644);

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    // Should still succeed but issue a warning
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_str_eq(creds->openai_api_key, "test-key");

    unlink(path);
}

END_TEST

// Test Suite Configuration
static Suite *credentials_coverage_suite(void)
{
    Suite *s = suite_create("Credentials Coverage");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_non_tilde_path);
    tcase_add_test(tc_core, test_successful_json_parsing);
    tcase_add_test(tc_core, test_empty_string_api_keys);
    tcase_add_test(tc_core, test_file_then_env_override);
    tcase_add_test(tc_core, test_insecure_permissions_warning);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = credentials_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
