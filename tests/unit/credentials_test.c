#include "../../src/credentials.h"
#include "../../src/panic.h"

#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

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
    char *path = talloc_asprintf(test_ctx, "/tmp/ikigai_creds_test_%d.json", getpid());
    FILE *f = fopen(path, "w");
    if (!f) {
        ck_abort_msg("Failed to create temp file: %s", strerror(errno));
    }
    fprintf(f, "%s", content);
    fclose(f);
    chmod(path, 0600);
    return path;
}

// Helper to set file permissions
static void set_file_permissions(const char *path, mode_t mode)
{
    if (chmod(path, mode) != 0) {
        ck_abort_msg("Failed to set permissions: %s", strerror(errno));
    }
}

// Test: Empty credentials (no file, no env)
START_TEST(test_empty_credentials)
{
    // Ensure env vars are not set
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, "/nonexistent/credentials.json", &creds);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_ptr_null(creds->openai_api_key);
    ck_assert_ptr_null(creds->anthropic_api_key);
    ck_assert_ptr_null(creds->google_api_key);
}
END_TEST

// Test: Load from environment variables
START_TEST(test_load_from_environment)
{
    setenv("OPENAI_API_KEY", "env-openai-key", 1);
    setenv("ANTHROPIC_API_KEY", "env-anthropic-key", 1);
    setenv("GOOGLE_API_KEY", "env-google-key", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, "/nonexistent/credentials.json", &creds);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_str_eq(creds->openai_api_key, "env-openai-key");
    ck_assert_str_eq(creds->anthropic_api_key, "env-anthropic-key");
    ck_assert_str_eq(creds->google_api_key, "env-google-key");

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
}
END_TEST

// Test: Load from file
START_TEST(test_load_from_file)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *json = "{\n"
                       "  \"openai\": { \"api_key\": \"file-openai-key\" },\n"
                       "  \"anthropic\": { \"api_key\": \"file-anthropic-key\" },\n"
                       "  \"google\": { \"api_key\": \"file-google-key\" }\n"
                       "}";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_str_eq(creds->openai_api_key, "file-openai-key");
    ck_assert_str_eq(creds->anthropic_api_key, "file-anthropic-key");
    ck_assert_str_eq(creds->google_api_key, "file-google-key");

    unlink(path);
}
END_TEST

// Test: Environment precedence over file
START_TEST(test_environment_precedence)
{
    const char *json = "{\n"
                       "  \"openai\": { \"api_key\": \"file-openai-key\" },\n"
                       "  \"anthropic\": { \"api_key\": \"file-anthropic-key\" },\n"
                       "  \"google\": { \"api_key\": \"file-google-key\" }\n"
                       "}";
    char *path = create_temp_credentials(json);

    setenv("OPENAI_API_KEY", "env-openai-key", 1);
    setenv("ANTHROPIC_API_KEY", "env-anthropic-key", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    // Env vars override file
    ck_assert_str_eq(creds->openai_api_key, "env-openai-key");
    ck_assert_str_eq(creds->anthropic_api_key, "env-anthropic-key");
    // Google comes from file (no env var)
    ck_assert_str_eq(creds->google_api_key, "file-google-key");

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unlink(path);
}
END_TEST

// Test: Provider lookup via ik_credentials_get
START_TEST(test_provider_lookup)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *json = "{\n"
                       "  \"openai\": { \"api_key\": \"openai-key\" },\n"
                       "  \"anthropic\": { \"api_key\": \"anthropic-key\" }\n"
                       "}";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    ck_assert_str_eq(ik_credentials_get(creds, "openai"), "openai-key");
    ck_assert_str_eq(ik_credentials_get(creds, "anthropic"), "anthropic-key");
    ck_assert_ptr_null(ik_credentials_get(creds, "google"));

    unlink(path);
}
END_TEST

// Test: Invalid JSON returns error
START_TEST(test_invalid_json)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    char *path = create_temp_credentials("{ invalid json }");

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(result.err);

    unlink(path);
}
END_TEST

// Test: Insecure permissions (0644)
START_TEST(test_insecure_permissions)
{
    const char *json = "{ \"openai\": { \"api_key\": \"test-key\" } }";
    char *path = create_temp_credentials(json);
    set_file_permissions(path, 0644);

    // Check function should report insecure
    ck_assert(ik_credentials_insecure_permissions(path));

    unlink(path);
}
END_TEST

// Test: Secure permissions (0600)
START_TEST(test_secure_permissions)
{
    const char *json = "{ \"openai\": { \"api_key\": \"test-key\" } }";
    char *path = create_temp_credentials(json);
    set_file_permissions(path, 0600);

    // Check function should report secure
    ck_assert(!ik_credentials_insecure_permissions(path));

    unlink(path);
}
END_TEST

// Test: All providers from file
START_TEST(test_all_providers_from_file)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *json = "{\n"
                       "  \"openai\": { \"api_key\": \"sk-proj-123\" },\n"
                       "  \"anthropic\": { \"api_key\": \"sk-ant-api03-456\" },\n"
                       "  \"google\": { \"api_key\": \"AIza789\" }\n"
                       "}";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds->openai_api_key);
    ck_assert_ptr_nonnull(creds->anthropic_api_key);
    ck_assert_ptr_nonnull(creds->google_api_key);

    unlink(path);
}
END_TEST

// Test: Empty env var ignored
START_TEST(test_empty_env_ignored)
{
    const char *json = "{ \"openai\": { \"api_key\": \"file-key\" } }";
    char *path = create_temp_credentials(json);

    setenv("OPENAI_API_KEY", "", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    // Empty env should be ignored, file value used
    ck_assert_str_eq(creds->openai_api_key, "file-key");

    unsetenv("OPENAI_API_KEY");
    unlink(path);
}
END_TEST

// Test: Unknown provider returns NULL
START_TEST(test_unknown_provider)
{
    const char *json = "{ \"openai\": { \"api_key\": \"test-key\" } }";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    ck_assert_ptr_null(ik_credentials_get(creds, "unknown"));
    ck_assert_ptr_null(ik_credentials_get(creds, "aws"));
    ck_assert_ptr_null(ik_credentials_get(creds, "azure"));

    unlink(path);
}
END_TEST

// Test: Partial providers in file
START_TEST(test_partial_providers)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *json = "{ \"openai\": { \"api_key\": \"openai-only\" } }";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds->openai_api_key);
    ck_assert_ptr_null(creds->anthropic_api_key);
    ck_assert_ptr_null(creds->google_api_key);

    unlink(path);
}
END_TEST

// Test: Non-existent file returns false for insecure permissions
START_TEST(test_nonexistent_file_permissions)
{
    ck_assert(!ik_credentials_insecure_permissions("/nonexistent/file.json"));
}
END_TEST

// Test: JSON not an object
START_TEST(test_json_not_object)
{
    char *path = create_temp_credentials("[1, 2, 3]");

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(is_err(&result));

    unlink(path);
}
END_TEST

// Test: Empty API keys in file are ignored
START_TEST(test_empty_api_keys_in_file)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *json = "{\n"
                       "  \"openai\": { \"api_key\": \"\" },\n"
                       "  \"anthropic\": { \"api_key\": \"valid-key\" }\n"
                       "}";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    ck_assert_ptr_null(creds->openai_api_key);  // Empty string ignored
    ck_assert_str_eq(creds->anthropic_api_key, "valid-key");

    unlink(path);
}
END_TEST

static Suite *credentials_suite(void)
{
    Suite *s = suite_create("Credentials");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_empty_credentials);
    tcase_add_test(tc_core, test_load_from_environment);
    tcase_add_test(tc_core, test_load_from_file);
    tcase_add_test(tc_core, test_environment_precedence);
    tcase_add_test(tc_core, test_provider_lookup);
    tcase_add_test(tc_core, test_invalid_json);
    tcase_add_test(tc_core, test_insecure_permissions);
    tcase_add_test(tc_core, test_secure_permissions);
    tcase_add_test(tc_core, test_all_providers_from_file);
    tcase_add_test(tc_core, test_empty_env_ignored);
    tcase_add_test(tc_core, test_unknown_provider);
    tcase_add_test(tc_core, test_partial_providers);
    tcase_add_test(tc_core, test_nonexistent_file_permissions);
    tcase_add_test(tc_core, test_json_not_object);
    tcase_add_test(tc_core, test_empty_api_keys_in_file);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = credentials_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
