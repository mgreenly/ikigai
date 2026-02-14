#include "tests/test_constants.h"
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

#include "shared/credentials.h"
#include "shared/error.h"

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

START_TEST(test_non_tilde_path) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *json = "{ \"OPENAI_API_KEY\": \"test-key\" }";
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

START_TEST(test_successful_json_parsing) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    const char *json =
        "{\"OPENAI_API_KEY\":\"openai-key\",\"ANTHROPIC_API_KEY\":\"anthropic-key\",\"GOOGLE_API_KEY\":\"google-key\"}";
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

START_TEST(test_empty_and_missing_api_keys) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Test 1: Empty string api_keys should not be loaded
    const char *json1 =
        "{\"OPENAI_API_KEY\": \"\", \"ANTHROPIC_API_KEY\": \"\", \"GOOGLE_API_KEY\": \"\"}";
    char *path1 = create_temp_credentials(json1);
    ik_credentials_t *creds1 = NULL;
    res_t result1 = ik_credentials_load(test_ctx, path1, &creds1);
    ck_assert(!is_err(&result1));
    ck_assert_ptr_null(creds1->openai_api_key);
    ck_assert_ptr_null(creds1->anthropic_api_key);
    ck_assert_ptr_null(creds1->google_api_key);
    unlink(path1);

    // Test 2: Missing api_key fields
    const char *json2 = "{\"other\": \"val\"}";
    char *path2 = create_temp_credentials(json2);
    ik_credentials_t *creds2 = NULL;
    res_t result2 = ik_credentials_load(test_ctx, path2, &creds2);
    ck_assert(!is_err(&result2));
    ck_assert_ptr_null(creds2->openai_api_key);
    ck_assert_ptr_null(creds2->anthropic_api_key);
    ck_assert_ptr_null(creds2->google_api_key);
    unlink(path2);
}
END_TEST

START_TEST(test_file_then_env_override) {
    const char *json =
        "{\"OPENAI_API_KEY\":\"file-openai\",\"ANTHROPIC_API_KEY\":\"file-anthropic\",\"GOOGLE_API_KEY\":\"file-google\"}";
    char *path = create_temp_credentials(json);
    setenv("OPENAI_API_KEY", "env-openai", 1);
    setenv("ANTHROPIC_API_KEY", "env-anthropic", 1);
    setenv("GOOGLE_API_KEY", "env-google", 1);
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);
    ck_assert(!is_err(&result));
    ck_assert_str_eq(creds->openai_api_key, "env-openai");
    ck_assert_str_eq(creds->anthropic_api_key, "env-anthropic");
    ck_assert_str_eq(creds->google_api_key, "env-google");
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    unlink(path);
}
END_TEST

START_TEST(test_insecure_permissions_warning) {
    const char *json = "{ \"OPENAI_API_KEY\": \"test-key\" }";
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

START_TEST(test_home_not_set) {
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

START_TEST(test_invalid_json_structures) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Test 1: JSON root is an array, not an object
    const char *json1 = "[\"not\", \"an\", \"object\"]";
    char *path1 = create_temp_credentials(json1);
    ik_credentials_t *creds1 = NULL;
    res_t result1 = ik_credentials_load(test_ctx, path1, &creds1);
    ck_assert(!is_err(&result1));
    unlink(path1);

    // Test 2: Providers are strings instead of objects
    const char *json2 = "{\"openai\": \"not-obj\", \"anthropic\": \"not-obj\", \"google\": \"not-obj\"}";
    char *path2 = create_temp_credentials(json2);
    ik_credentials_t *creds2 = NULL;
    res_t result2 = ik_credentials_load(test_ctx, path2, &creds2);
    ck_assert(!is_err(&result2));
    ck_assert_ptr_null(creds2->openai_api_key);
    ck_assert_ptr_null(creds2->anthropic_api_key);
    ck_assert_ptr_null(creds2->google_api_key);
    unlink(path2);

    // Test 3: api_key fields are not strings
    const char *json3 =
        "{\"openai\": {\"api_key\": 123}, \"anthropic\": {\"api_key\": {}}, \"google\": {\"api_key\": []}}";
    char *path3 = create_temp_credentials(json3);
    ik_credentials_t *creds3 = NULL;
    res_t result3 = ik_credentials_load(test_ctx, path3, &creds3);
    ck_assert(!is_err(&result3));
    ck_assert_ptr_null(creds3->openai_api_key);
    ck_assert_ptr_null(creds3->anthropic_api_key);
    ck_assert_ptr_null(creds3->google_api_key);
    unlink(path3);
}
END_TEST

START_TEST(test_env_var_behaviors) {
    // Test 1: Empty env var should not override file credential
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    const char *json1 = "{\"OPENAI_API_KEY\": \"file-key\"}";
    char *path1 = create_temp_credentials(json1);
    setenv("OPENAI_API_KEY", "", 1);
    ik_credentials_t *creds1 = NULL;
    res_t result1 = ik_credentials_load(test_ctx, path1, &creds1);
    ck_assert(!is_err(&result1));
    ck_assert_str_eq(creds1->openai_api_key, "file-key");
    unsetenv("OPENAI_API_KEY");
    unlink(path1);

    // Test 2: Env vars should work when file has no credentials
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    const char *json2 = "{}";
    char *path2 = create_temp_credentials(json2);
    setenv("OPENAI_API_KEY", "env-openai", 1);
    setenv("ANTHROPIC_API_KEY", "env-anthropic", 1);
    setenv("GOOGLE_API_KEY", "env-google", 1);
    ik_credentials_t *creds2 = NULL;
    res_t result2 = ik_credentials_load(test_ctx, path2, &creds2);
    ck_assert(!is_err(&result2));
    ck_assert_str_eq(creds2->openai_api_key, "env-openai");
    ck_assert_str_eq(creds2->anthropic_api_key, "env-anthropic");
    ck_assert_str_eq(creds2->google_api_key, "env-google");
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    unlink(path2);
}
END_TEST

START_TEST(test_corrupted_json_file) {
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


START_TEST(test_file_not_found) {
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

START_TEST(test_default_path) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Call with NULL path to use default path
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    // Should succeed (file may or may not exist at default location)
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
}
END_TEST

START_TEST(test_successful_tilde_expansion) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Create credentials file in temp location
    const char *json = "{ \"OPENAI_API_KEY\": \"tilde-test-key\" }";
    char *actual_path = talloc_asprintf(test_ctx, "/tmp/ikigai_tilde_%d.json", getpid());
    FILE *f = fopen(actual_path, "w");
    fprintf(f, "%s", json);
    fclose(f);
    chmod(actual_path, 0600);

    // Now set HOME to /tmp and use ~/ikigai_tilde_<pid>.json as path
    char *old_home = getenv("HOME");
    char *saved_home = old_home ? talloc_strdup(test_ctx, old_home) : NULL;
    setenv("HOME", "/tmp", 1);

    char *tilde_path = talloc_asprintf(test_ctx, "~/ikigai_tilde_%d.json", getpid());
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tilde_path, &creds);

    // Should successfully expand and load
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_str_eq(creds->openai_api_key, "tilde-test-key");

    // Restore HOME
    if (saved_home) {
        setenv("HOME", saved_home, 1);
    } else {
        unsetenv("HOME");
    }

    unlink(actual_path);
}
END_TEST

START_TEST(test_credentials_get_all_providers) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    const char *json =
        "{\"OPENAI_API_KEY\":\"openai-test\",\"ANTHROPIC_API_KEY\":\"anthropic-test\",\"GOOGLE_API_KEY\":\"google-test\"}";
    char *path = create_temp_credentials(json);
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_ptr_nonnull(ik_credentials_get(creds, "OPENAI_API_KEY"));
    ck_assert_str_eq(ik_credentials_get(creds, "OPENAI_API_KEY"), "openai-test");
    ck_assert_ptr_nonnull(ik_credentials_get(creds, "ANTHROPIC_API_KEY"));
    ck_assert_str_eq(ik_credentials_get(creds, "ANTHROPIC_API_KEY"), "anthropic-test");
    ck_assert_ptr_nonnull(ik_credentials_get(creds, "GOOGLE_API_KEY"));
    ck_assert_str_eq(ik_credentials_get(creds, "GOOGLE_API_KEY"), "google-test");
    ck_assert_ptr_null(ik_credentials_get(creds, "unknown"));
    unlink(path);
}
END_TEST

START_TEST(test_partial_env_override) {
    const char *json =
        "{\"OPENAI_API_KEY\":\"f1\",\"ANTHROPIC_API_KEY\":\"f2\",\"GOOGLE_API_KEY\":\"f3\"}";
    char *path = create_temp_credentials(json);
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    setenv("OPENAI_API_KEY", "env", 1);
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);
    ck_assert(!is_err(&result));
    ck_assert_str_eq(creds->openai_api_key, "env");
    ck_assert_str_eq(creds->anthropic_api_key, "f2");
    ck_assert_str_eq(creds->google_api_key, "f3");
    unsetenv("OPENAI_API_KEY");
    unlink(path);
}
END_TEST

START_TEST(test_optional_credentials_from_file) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");
    unsetenv("IKIGAI_DB_PASS");
    unsetenv("BRAVE_API_KEY");
    unsetenv("NTFY_API_KEY");
    unsetenv("NTFY_TOPIC");

    const char *json = "{"
        "\"GOOGLE_SEARCH_API_KEY\":\"gs-key\","
        "\"GOOGLE_SEARCH_ENGINE_ID\":\"gs-engine\","
        "\"IKIGAI_DB_PASS\":\"db-pass\","
        "\"BRAVE_API_KEY\":\"brave-key\","
        "\"NTFY_API_KEY\":\"ntfy-key\","
        "\"NTFY_TOPIC\":\"ntfy-topic\""
    "}";
    char *path = create_temp_credentials(json);
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_str_eq(creds->google_search_api_key, "gs-key");
    ck_assert_str_eq(creds->google_search_engine_id, "gs-engine");
    ck_assert_str_eq(creds->db_pass, "db-pass");
    ck_assert_str_eq(creds->brave_api_key, "brave-key");
    ck_assert_str_eq(creds->ntfy_api_key, "ntfy-key");
    ck_assert_str_eq(creds->ntfy_topic, "ntfy-topic");
    unlink(path);
}
END_TEST

START_TEST(test_optional_credentials_from_env) {
    const char *json = "{}";
    char *path = create_temp_credentials(json);

    setenv("GOOGLE_SEARCH_API_KEY", "env-gs-key", 1);
    setenv("GOOGLE_SEARCH_ENGINE_ID", "env-gs-engine", 1);
    setenv("IKIGAI_DB_PASS", "env-db-pass", 1);
    setenv("BRAVE_API_KEY", "env-brave-key", 1);
    setenv("NTFY_API_KEY", "env-ntfy-key", 1);
    setenv("NTFY_TOPIC", "env-ntfy-topic", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_str_eq(creds->google_search_api_key, "env-gs-key");
    ck_assert_str_eq(creds->google_search_engine_id, "env-gs-engine");
    ck_assert_str_eq(creds->db_pass, "env-db-pass");
    ck_assert_str_eq(creds->brave_api_key, "env-brave-key");
    ck_assert_str_eq(creds->ntfy_api_key, "env-ntfy-key");
    ck_assert_str_eq(creds->ntfy_topic, "env-ntfy-topic");

    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");
    unsetenv("IKIGAI_DB_PASS");
    unsetenv("BRAVE_API_KEY");
    unsetenv("NTFY_API_KEY");
    unsetenv("NTFY_TOPIC");
    unlink(path);
}
END_TEST

START_TEST(test_optional_credentials_env_override) {
    const char *json = "{"
        "\"GOOGLE_SEARCH_API_KEY\":\"file-gs-key\","
        "\"GOOGLE_SEARCH_ENGINE_ID\":\"file-gs-engine\","
        "\"IKIGAI_DB_PASS\":\"file-db-pass\""
    "}";
    char *path = create_temp_credentials(json);

    setenv("GOOGLE_SEARCH_API_KEY", "env-gs-key", 1);
    setenv("GOOGLE_SEARCH_ENGINE_ID", "env-gs-engine", 1);
    setenv("IKIGAI_DB_PASS", "env-db-pass", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);
    ck_assert(!is_err(&result));
    ck_assert_str_eq(creds->google_search_api_key, "env-gs-key");
    ck_assert_str_eq(creds->google_search_engine_id, "env-gs-engine");
    ck_assert_str_eq(creds->db_pass, "env-db-pass");

    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");
    unsetenv("IKIGAI_DB_PASS");
    unlink(path);
}
END_TEST

START_TEST(test_credentials_get_optional) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");
    unsetenv("IKIGAI_DB_PASS");
    unsetenv("BRAVE_API_KEY");
    unsetenv("NTFY_API_KEY");
    unsetenv("NTFY_TOPIC");

    const char *json = "{"
        "\"GOOGLE_SEARCH_API_KEY\":\"gs-test\","
        "\"GOOGLE_SEARCH_ENGINE_ID\":\"engine-test\","
        "\"IKIGAI_DB_PASS\":\"pass-test\","
        "\"BRAVE_API_KEY\":\"brave-test\","
        "\"NTFY_API_KEY\":\"ntfy-key-test\","
        "\"NTFY_TOPIC\":\"ntfy-topic-test\""
    "}";
    char *path = create_temp_credentials(json);
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, path, &creds);
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(creds);

    ck_assert_ptr_nonnull(ik_credentials_get(creds, "GOOGLE_SEARCH_API_KEY"));
    ck_assert_str_eq(ik_credentials_get(creds, "GOOGLE_SEARCH_API_KEY"), "gs-test");

    ck_assert_ptr_nonnull(ik_credentials_get(creds, "GOOGLE_SEARCH_ENGINE_ID"));
    ck_assert_str_eq(ik_credentials_get(creds, "GOOGLE_SEARCH_ENGINE_ID"), "engine-test");

    ck_assert_ptr_nonnull(ik_credentials_get(creds, "IKIGAI_DB_PASS"));
    ck_assert_str_eq(ik_credentials_get(creds, "IKIGAI_DB_PASS"), "pass-test");

    ck_assert_ptr_nonnull(ik_credentials_get(creds, "BRAVE_API_KEY"));
    ck_assert_str_eq(ik_credentials_get(creds, "BRAVE_API_KEY"), "brave-test");

    ck_assert_ptr_nonnull(ik_credentials_get(creds, "NTFY_API_KEY"));
    ck_assert_str_eq(ik_credentials_get(creds, "NTFY_API_KEY"), "ntfy-key-test");

    ck_assert_ptr_nonnull(ik_credentials_get(creds, "NTFY_TOPIC"));
    ck_assert_str_eq(ik_credentials_get(creds, "NTFY_TOPIC"), "ntfy-topic-test");

    unlink(path);
}
END_TEST

static Suite *credentials_coverage_suite(void)
{
    Suite *s = suite_create("Credentials Coverage");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_non_tilde_path);
    tcase_add_test(tc_core, test_successful_json_parsing);
    tcase_add_test(tc_core, test_empty_and_missing_api_keys);
    tcase_add_test(tc_core, test_file_then_env_override);
    tcase_add_test(tc_core, test_insecure_permissions_warning);
    tcase_add_test(tc_core, test_home_not_set);
    tcase_add_test(tc_core, test_invalid_json_structures);
    tcase_add_test(tc_core, test_env_var_behaviors);
    tcase_add_test(tc_core, test_corrupted_json_file);
    tcase_add_test(tc_core, test_file_not_found);
    tcase_add_test(tc_core, test_default_path);
    tcase_add_test(tc_core, test_successful_tilde_expansion);
    tcase_add_test(tc_core, test_credentials_get_all_providers);
    tcase_add_test(tc_core, test_partial_env_override);
    tcase_add_test(tc_core, test_optional_credentials_from_file);
    tcase_add_test(tc_core, test_optional_credentials_from_env);
    tcase_add_test(tc_core, test_optional_credentials_env_override);
    tcase_add_test(tc_core, test_credentials_get_optional);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = credentials_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/credentials/credentials_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
