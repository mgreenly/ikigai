#include "../../src/credentials.h"
#include "../../src/panic.h"
#include "../../src/vendor/yyjson/yyjson.h"
#include "../../src/wrapper.h"

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

// Test: Empty credentials and load from environment
START_TEST(test_empty_and_env_credentials)
{
    // Test 1: No file, no env
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    ik_credentials_t *creds1 = NULL;
    res_t result1 = ik_credentials_load(test_ctx, "/nonexistent/credentials.json", &creds1);
    ck_assert(!is_err(&result1));
    ck_assert_ptr_null(creds1->openai_api_key);

    // Test 2: Load from environment
    setenv("OPENAI_API_KEY", "env-openai-key", 1);
    setenv("ANTHROPIC_API_KEY", "env-anthropic-key", 1);
    setenv("GOOGLE_API_KEY", "env-google-key", 1);
    ik_credentials_t *creds2 = NULL;
    res_t result2 = ik_credentials_load(test_ctx, "/nonexistent/credentials.json", &creds2);
    ck_assert(!is_err(&result2));
    ck_assert_str_eq(creds2->openai_api_key, "env-openai-key");
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
// Test: File permissions (secure and insecure)
START_TEST(test_file_permissions)
{
    const char *json = "{ \"openai\": { \"api_key\": \"test-key\" } }";
    char *path = create_temp_credentials(json);

    set_file_permissions(path, 0644);
    ck_assert(ik_credentials_insecure_permissions(path));

    set_file_permissions(path, 0600);
    ck_assert(!ik_credentials_insecure_permissions(path));

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
// Test: Edge cases - partial providers, nonexistent file perms, invalid JSON
START_TEST(test_misc_edge_cases)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Test 1: Partial providers in file
    const char *json1 = "{ \"openai\": { \"api_key\": \"openai-only\" } }";
    char *path1 = create_temp_credentials(json1);
    ik_credentials_t *creds1 = NULL;
    res_t result1 = ik_credentials_load(test_ctx, path1, &creds1);
    ck_assert(!is_err(&result1));
    ck_assert_ptr_nonnull(creds1->openai_api_key);
    ck_assert_ptr_null(creds1->anthropic_api_key);
    unlink(path1);

    // Test 2: Nonexistent file permissions
    ck_assert(!ik_credentials_insecure_permissions("/nonexistent/file.json"));

    // Test 3: JSON not an object
    char *path3 = create_temp_credentials("[1, 2, 3]");
    ik_credentials_t *creds3 = NULL;
    res_t result3 = ik_credentials_load(test_ctx, path3, &creds3);
    ck_assert(is_err(&result3));
    unlink(path3);
}

END_TEST
// Test: Tilde expansion in path
START_TEST(test_tilde_expansion)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Create file in /tmp with a key
    const char *json = "{ \"openai\": { \"api_key\": \"test-key\" } }";
    char *path = create_temp_credentials(json);

    // Create a tilde path that will expand
    const char *home = getenv("HOME");
    ck_assert_ptr_nonnull(home);

    // Create a file in home directory
    char *home_path = talloc_asprintf(test_ctx, "%s/.ikigai_test_creds_%d.json", home, getpid());
    FILE *f = fopen(home_path, "w");
    fprintf(f, "%s", json);
    fclose(f);
    chmod(home_path, 0600);

    // Load using tilde path
    char *tilde_path = talloc_asprintf(test_ctx, "~/.ikigai_test_creds_%d.json", getpid());
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tilde_path, &creds);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_str_eq(creds->openai_api_key, "test-key");

    unlink(home_path);
    unlink(path);
}

END_TEST
// Test: HOME not set returns error
START_TEST(test_home_not_set)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Save HOME and temporarily unset it
    const char *home = getenv("HOME");
    ck_assert_ptr_nonnull(home);
    char *saved_home = talloc_strdup(test_ctx, home);
    unsetenv("HOME");

    // Try to load with tilde path
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, "~/.config/ikigai/credentials.json", &creds);

    // Should return error
    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(result.err);

    // Restore HOME
    setenv("HOME", saved_home, 1);
}

END_TEST
// Test: Provider parsing edge cases (no api_key, non-string, not object)
START_TEST(test_provider_parsing)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    ik_credentials_t *c = NULL;
    res_t r;

    // No api_key field
    char *p = create_temp_credentials("{ \"openai\": { \"other_field\": \"value\" } }");
    r = ik_credentials_load(test_ctx, p, &c);
    ck_assert(!is_err(&r));
    ck_assert_ptr_null(c->openai_api_key);
    unlink(p);

    // api_key is not a string
    p = create_temp_credentials("{ \"openai\": { \"api_key\": 12345 } }");
    r = ik_credentials_load(test_ctx, p, &c);
    ck_assert(!is_err(&r));
    ck_assert_ptr_null(c->openai_api_key);
    unlink(p);

    // Provider is not an object
    p = create_temp_credentials("{ \"openai\": \"not-an-object\" }");
    r = ik_credentials_load(test_ctx, p, &c);
    ck_assert(!is_err(&r));
    ck_assert_ptr_null(c->openai_api_key);
    unlink(p);
}

END_TEST
// Test: Path handling (no tilde, NULL path)
START_TEST(test_path_handling)
{
    ik_credentials_t *c = NULL;
    res_t r;

    // Test 1: Path without tilde
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    char *p = create_temp_credentials("{ \"openai\": { \"api_key\": \"test-key\" } }");
    r = ik_credentials_load(test_ctx, p, &c);
    ck_assert(!is_err(&r));
    ck_assert_str_eq(c->openai_api_key, "test-key");
    unlink(p);

    // Test 2: NULL path uses default
    setenv("OPENAI_API_KEY", "env-key", 1);
    r = ik_credentials_load(test_ctx, NULL, &c);
    ck_assert(!is_err(&r));
    ck_assert_str_eq(c->openai_api_key, "env-key");
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
}

END_TEST
// Test: Environment override when no file value exists
START_TEST(test_env_override_no_file_value)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // File has only anthropic key
    const char *json = "{ \"anthropic\": { \"api_key\": \"file-anthropic\" } }";
    char *path = create_temp_credentials(json);

    // Set env vars for openai and google (which are not in file)
    setenv("OPENAI_API_KEY", "env-openai", 1);
    setenv("GOOGLE_API_KEY", "env-google", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    ck_assert_str_eq(creds->openai_api_key, "env-openai");
    ck_assert_str_eq(creds->anthropic_api_key, "file-anthropic");
    ck_assert_str_eq(creds->google_api_key, "env-google");

    unsetenv("OPENAI_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    unlink(path);
}

END_TEST

// Mock for yyjson_read_file_ to return NULL
static bool mock_yyjson_read_file_fail = false;
yyjson_doc *yyjson_read_file_(const char *path, yyjson_read_flag flg, const yyjson_alc *alc, yyjson_read_err *err)
{
    if (mock_yyjson_read_file_fail) {
        if (err) {
            err->code = YYJSON_READ_ERROR_FILE_OPEN;
            err->msg = "mock";
            err->pos = 0;
        }
        return NULL;
    }
    return yyjson_read_file(path, flg, alc, err);
}

// Mock for yyjson_doc_get_root_ to return NULL
static bool mock_yyjson_doc_get_root_null = false;
yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc)
{
    if (mock_yyjson_doc_get_root_null) {
        return NULL;
    }
    return yyjson_doc_get_root(doc);
}

// Test: yyjson_read_file_ returns NULL
START_TEST(test_yyjson_read_file_error) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *json = "{ \"openai\": { \"api_key\": \"test-key\" } }";
    char *path = create_temp_credentials(json);

    // Mock yyjson_read_file_ to fail
    mock_yyjson_read_file_fail = true;

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    // Should get error from JSON parse failure
    ck_assert(is_err(&result));

    mock_yyjson_read_file_fail = false;
    unlink(path);
}
END_TEST
// Test: yyjson_doc_get_root_ returns NULL
START_TEST(test_yyjson_doc_get_root_null)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *json = "{ \"openai\": { \"api_key\": \"test-key\" } }";
    char *path = create_temp_credentials(json);

    // Mock yyjson_doc_get_root_ to return NULL
    mock_yyjson_doc_get_root_null = true;

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    // Should get error for NULL root
    ck_assert(is_err(&result));

    mock_yyjson_doc_get_root_null = false;
    unlink(path);
}

END_TEST

// Test: Only google provider in file (ensure lines 104-113 fully covered)
START_TEST(test_only_google_provider)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *json = "{ \"google\": { \"api_key\": \"google-only-key\" } }";
    char *path = create_temp_credentials(json);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_ptr_null(creds->openai_api_key);
    ck_assert_ptr_null(creds->anthropic_api_key);
    ck_assert_str_eq(creds->google_api_key, "google-only-key");

    unlink(path);
}

END_TEST

static Suite *credentials_suite(void)
{
    Suite *s = suite_create("Credentials");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_empty_and_env_credentials);
    tcase_add_test(tc_core, test_load_from_file);
    tcase_add_test(tc_core, test_environment_precedence);
    tcase_add_test(tc_core, test_provider_lookup);
    tcase_add_test(tc_core, test_invalid_json);
    tcase_add_test(tc_core, test_file_permissions);
    tcase_add_test(tc_core, test_unknown_provider);
    tcase_add_test(tc_core, test_misc_edge_cases);
    tcase_add_test(tc_core, test_tilde_expansion);
    tcase_add_test(tc_core, test_home_not_set);
    tcase_add_test(tc_core, test_provider_parsing);
    tcase_add_test(tc_core, test_path_handling);
    tcase_add_test(tc_core, test_env_override_no_file_value);
    tcase_add_test(tc_core, test_yyjson_read_file_error);
    tcase_add_test(tc_core, test_yyjson_doc_get_root_null);
    tcase_add_test(tc_core, test_only_google_provider);

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
