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
#include <time.h>
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

START_TEST(test_home_not_set)
{
    // Save and unset HOME
    char *old_home = getenv("HOME");
    char *saved_home = old_home ? talloc_strdup(test_ctx, old_home) : NULL;
    unsetenv("HOME");

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    ik_credentials_t *creds = NULL;
    // Use tilde path to trigger expand_tilde
    res_t result = ik_credentials_load(test_ctx, "~/credentials.json", &creds);

    // Should fail with ERR_INVALID_ARG error
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);

    // Restore HOME
    if (saved_home) {
        setenv("HOME", saved_home, 1);
    }
}

END_TEST

START_TEST(test_invalid_json_root_not_object)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // JSON root is an array, not an object
    const char *json = "[\"not\", \"an\", \"object\"]";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    // Should continue with warning, but return success with NULL credentials
    ck_assert(!is_err(&result));

    unlink(path);
}

END_TEST

START_TEST(test_provider_not_object)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // openai, anthropic, google are strings instead of objects
    const char *json = "{\n"
                       "  \"openai\": \"not-an-object\",\n"
                       "  \"anthropic\": \"also-not-object\",\n"
                       "  \"google\": \"still-not-object\"\n"
                       "}";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    // Should succeed but skip invalid providers
    ck_assert(!is_err(&result));
    ck_assert_ptr_null(creds->openai_api_key);
    ck_assert_ptr_null(creds->anthropic_api_key);
    ck_assert_ptr_null(creds->google_api_key);

    unlink(path);
}

END_TEST

START_TEST(test_api_key_not_string)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // api_key fields are numbers and objects instead of strings
    const char *json = "{\n"
                       "  \"openai\": { \"api_key\": 12345 },\n"
                       "  \"anthropic\": { \"api_key\": {\"nested\": \"object\"} },\n"
                       "  \"google\": { \"api_key\": [\"array\", \"value\"] }\n"
                       "}";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    // Should succeed but skip invalid api_key values
    ck_assert(!is_err(&result));
    ck_assert_ptr_null(creds->openai_api_key);
    ck_assert_ptr_null(creds->anthropic_api_key);
    ck_assert_ptr_null(creds->google_api_key);

    unlink(path);
}

END_TEST

START_TEST(test_env_empty_string)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Create file with valid credentials
    const char *json = "{\n"
                       "  \"openai\": { \"api_key\": \"file-key\" }\n"
                       "}";
    char *path = create_temp_credentials(json);

    // Set env var to empty string
    setenv("OPENAI_API_KEY", "", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    // Empty env var should not override file credential
    ck_assert(!is_err(&result));
    ck_assert_str_eq(creds->openai_api_key, "file-key");

    unsetenv("OPENAI_API_KEY");
    unlink(path);
}

END_TEST

START_TEST(test_env_override_null_file_credential)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Create file with NO credentials
    const char *json = "{}";
    char *path = create_temp_credentials(json);

    // Set all env vars
    setenv("OPENAI_API_KEY", "env-only-openai", 1);
    setenv("ANTHROPIC_API_KEY", "env-only-anthropic", 1);
    setenv("GOOGLE_API_KEY", "env-only-google", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    // Should use env vars since file has none
    ck_assert(!is_err(&result));
    ck_assert_str_eq(creds->openai_api_key, "env-only-openai");
    ck_assert_str_eq(creds->anthropic_api_key, "env-only-anthropic");
    ck_assert_str_eq(creds->google_api_key, "env-only-google");

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    unlink(path);
}

END_TEST

START_TEST(test_missing_api_key_field)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Provider objects exist but have no api_key field
    const char *json = "{\n"
                       "  \"openai\": { \"other_field\": \"value\" },\n"
                       "  \"anthropic\": { \"something\": \"else\" },\n"
                       "  \"google\": { \"random\": \"data\" }\n"
                       "}";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    // Should succeed but have no credentials
    ck_assert(!is_err(&result));
    ck_assert_ptr_null(creds->openai_api_key);
    ck_assert_ptr_null(creds->anthropic_api_key);
    ck_assert_ptr_null(creds->google_api_key);

    unlink(path);
}

END_TEST

START_TEST(test_corrupted_json_file)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Invalid JSON syntax
    const char *json = "{\"openai\": {\"api_key\": \"test\" CORRUPTED";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    // Should continue with warning and return success with NULL credentials
    ck_assert(!is_err(&result));

    unlink(path);
}

END_TEST

START_TEST(test_secure_permissions_no_warning)
{
    const char *json = "{ \"openai\": { \"api_key\": \"test-key\" } }";
    char *path = create_temp_credentials(json);

    // Set secure permissions (0600) - should not trigger warning
    chmod(path, 0600);

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    // Should succeed without warning
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);

    // Also test direct insecure_permissions check
    bool is_insecure = ik_credentials_insecure_permissions(path);
    ck_assert(!is_insecure);

    unlink(path);
}

END_TEST

START_TEST(test_insecure_permissions_nonexistent_file)
{
    // Check permissions on file that doesn't exist
    char *path = talloc_asprintf(test_ctx, "/tmp/ikigai_nonexistent_%d.json", getpid());

    bool is_insecure = ik_credentials_insecure_permissions(path);

    // Non-existent file should not be considered insecure
    ck_assert(!is_insecure);
}

END_TEST

START_TEST(test_various_permission_modes)
{
    const char *json = "{ \"openai\": { \"api_key\": \"test\" } }";

    // Test 0640 (group readable)
    char *path1 = talloc_asprintf(test_ctx, "/tmp/ikigai_perms_640_%d.json", getpid());
    FILE *f1 = fopen(path1, "w");
    fprintf(f1, "%s", json);
    fclose(f1);
    chmod(path1, 0640);
    ck_assert(ik_credentials_insecure_permissions(path1));
    unlink(path1);

    // Test 0604 (other readable)
    char *path2 = talloc_asprintf(test_ctx, "/tmp/ikigai_perms_604_%d.json", getpid());
    FILE *f2 = fopen(path2, "w");
    fprintf(f2, "%s", json);
    fclose(f2);
    chmod(path2, 0604);
    ck_assert(ik_credentials_insecure_permissions(path2));
    unlink(path2);

    // Test 0700 (owner execute)
    char *path3 = talloc_asprintf(test_ctx, "/tmp/ikigai_perms_700_%d.json", getpid());
    FILE *f3 = fopen(path3, "w");
    fprintf(f3, "%s", json);
    fclose(f3);
    chmod(path3, 0700);
    ck_assert(ik_credentials_insecure_permissions(path3));
    unlink(path3);
}
END_TEST

START_TEST(test_file_not_found)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    char *path = talloc_asprintf(test_ctx, "/tmp/ikigai_nonexistent_%d_%ld.json", getpid(), (long)time(NULL));
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_ptr_null(creds->openai_api_key);
    ck_assert_ptr_null(creds->anthropic_api_key);
    ck_assert_ptr_null(creds->google_api_key);
}
END_TEST

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
    tcase_add_test(tc_core, test_home_not_set);
    tcase_add_test(tc_core, test_invalid_json_root_not_object);
    tcase_add_test(tc_core, test_provider_not_object);
    tcase_add_test(tc_core, test_api_key_not_string);
    tcase_add_test(tc_core, test_env_empty_string);
    tcase_add_test(tc_core, test_env_override_null_file_credential);
    tcase_add_test(tc_core, test_missing_api_key_field);
    tcase_add_test(tc_core, test_corrupted_json_file);
    tcase_add_test(tc_core, test_secure_permissions_no_warning);
    tcase_add_test(tc_core, test_insecure_permissions_nonexistent_file);
    tcase_add_test(tc_core, test_various_permission_modes);
    tcase_add_test(tc_core, test_file_not_found);

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
