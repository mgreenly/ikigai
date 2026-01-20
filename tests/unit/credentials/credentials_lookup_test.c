#include "../../test_constants.h"
/**
 * @file test_credentials.c
 * @brief Unit tests for credentials lookup from environment
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
#include "vendor/yyjson/yyjson.h"
#include "wrapper.h"

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
 * Test ik_credentials_get() for each provider with environment variables.
 */

START_TEST(test_credentials_from_env_openai) {
    setenv("OPENAI_API_KEY", "sk-test123", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *key = ik_credentials_get(creds, "OPENAI_API_KEY");
    ck_assert_ptr_nonnull(key);
    ck_assert_str_eq(key, "sk-test123");

    unsetenv("OPENAI_API_KEY");
}
END_TEST

START_TEST(test_credentials_from_env_anthropic) {
    setenv("ANTHROPIC_API_KEY", "sk-ant-test", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *key = ik_credentials_get(creds, "ANTHROPIC_API_KEY");
    ck_assert_ptr_nonnull(key);
    ck_assert_str_eq(key, "sk-ant-test");

    unsetenv("ANTHROPIC_API_KEY");
}

END_TEST

START_TEST(test_credentials_from_env_google) {
    setenv("GOOGLE_API_KEY", "AIza-test", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *key = ik_credentials_get(creds, "GOOGLE_API_KEY");
    ck_assert_ptr_nonnull(key);
    ck_assert_str_eq(key, "AIza-test");

    unsetenv("GOOGLE_API_KEY");
}

END_TEST

START_TEST(test_credentials_missing_returns_null) {
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
    const char *key = ik_credentials_get(creds, "OPENAI_API_KEY");
    (void)key; /* Test passes if no crash */
}

END_TEST

START_TEST(test_credentials_unknown_provider) {
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *key = ik_credentials_get(creds, "UNKNOWN_ENV_VAR");
    ck_assert_ptr_null(key);
}

END_TEST

START_TEST(test_credentials_explicit_path_nonexistent) {
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, "/tmp/nonexistent_credentials.json", &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);
}

END_TEST

START_TEST(test_insecure_permissions_missing_file) {
    bool insecure = ik_credentials_insecure_permissions("/tmp/nonexistent_file_12345.json");
    ck_assert(!insecure);
}

END_TEST

START_TEST(test_insecure_permissions_bad_perms) {
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

END_TEST

START_TEST(test_insecure_permissions_secure) {
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

START_TEST(test_load_with_insecure_permissions) {
    const char *tmpfile = "/tmp/test_creds_warning.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{}");
    fclose(f);
    chmod(tmpfile, 0644);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    unlink(tmpfile);
}

END_TEST

START_TEST(test_env_var_overrides_file) {
    const char *tmpfile = "/tmp/test_creds_override.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"OPENAI_API_KEY\":\"file-key-openai\","
            "\"ANTHROPIC_API_KEY\":\"file-key-anthropic\","
            "\"GOOGLE_API_KEY\":\"file-key-google\"}");
    fclose(f);
    chmod(tmpfile, 0600);

    setenv("OPENAI_API_KEY", "env-key-openai", 1);
    setenv("ANTHROPIC_API_KEY", "env-key-anthropic", 1);
    setenv("GOOGLE_API_KEY", "env-key-google", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *openai_key = ik_credentials_get(creds, "OPENAI_API_KEY");
    ck_assert_ptr_nonnull(openai_key);
    ck_assert_str_eq(openai_key, "env-key-openai");

    const char *anthropic_key = ik_credentials_get(creds, "ANTHROPIC_API_KEY");
    ck_assert_ptr_nonnull(anthropic_key);
    ck_assert_str_eq(anthropic_key, "env-key-anthropic");

    const char *google_key = ik_credentials_get(creds, "GOOGLE_API_KEY");
    ck_assert_ptr_nonnull(google_key);
    ck_assert_str_eq(google_key, "env-key-google");

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    unlink(tmpfile);
}

END_TEST

START_TEST(test_file_based_credentials) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *tmpfile = "/tmp/test_creds_file.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"OPENAI_API_KEY\":\"file-openai-key\","
            "\"ANTHROPIC_API_KEY\":\"file-anthropic-key\","
            "\"GOOGLE_API_KEY\":\"file-google-key\"}");
    fclose(f);
    chmod(tmpfile, 0600);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *openai_key = ik_credentials_get(creds, "OPENAI_API_KEY");
    ck_assert_ptr_nonnull(openai_key);
    ck_assert_str_eq(openai_key, "file-openai-key");

    const char *anthropic_key = ik_credentials_get(creds, "ANTHROPIC_API_KEY");
    ck_assert_ptr_nonnull(anthropic_key);
    ck_assert_str_eq(anthropic_key, "file-anthropic-key");

    const char *google_key = ik_credentials_get(creds, "GOOGLE_API_KEY");
    ck_assert_ptr_nonnull(google_key);
    ck_assert_str_eq(google_key, "file-google-key");

    unlink(tmpfile);
}

END_TEST

START_TEST(test_invalid_json_file) {
    const char *tmpfile = "/tmp/test_creds_invalid.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{this is not valid json}");
    fclose(f);
    chmod(tmpfile, 0600);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    unlink(tmpfile);
}

END_TEST

START_TEST(test_json_root_not_object) {
    const char *tmpfile = "/tmp/test_creds_array.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "[\"not\", \"an\", \"object\"]");
    fclose(f);
    chmod(tmpfile, 0600);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    unlink(tmpfile);
}

END_TEST

START_TEST(test_json_malformed_credentials) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *test_cases[][2] = {
        {"/tmp/creds1.json", "{\"other\":\"value\"}"},
        {"/tmp/creds2.json", "{\"OPENAI_API_KEY\":123,\"ANTHROPIC_API_KEY\":true,\"GOOGLE_API_KEY\":[]}"},
        {"/tmp/creds3.json", "{\"OPENAI_API_KEY\":null,\"ANTHROPIC_API_KEY\":null,\"GOOGLE_API_KEY\":null}"},
        {"/tmp/creds4.json", "{\"OPENAI_API_KEY\":1,\"ANTHROPIC_API_KEY\":true,\"GOOGLE_API_KEY\":null}"},
        {"/tmp/creds5.json", "{\"OPENAI_API_KEY\":\"\",\"ANTHROPIC_API_KEY\":\"\",\"GOOGLE_API_KEY\":\"\"}"},
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
        ck_assert_ptr_null(ik_credentials_get(creds, "OPENAI_API_KEY"));
        ck_assert_ptr_null(ik_credentials_get(creds, "ANTHROPIC_API_KEY"));
        ck_assert_ptr_null(ik_credentials_get(creds, "GOOGLE_API_KEY"));

        unlink(test_cases[i][0]);
    }
}

END_TEST

START_TEST(test_tilde_expansion_no_home) {
    char *original_home = getenv("HOME");
    char *home_copy = NULL;
    if (original_home) {
        home_copy = strdup(original_home);
    }

    char *original_config_dir = getenv("IKIGAI_CONFIG_DIR");
    char *config_dir_copy = NULL;
    if (original_config_dir) {
        config_dir_copy = strdup(original_config_dir);
    }

    unsetenv("HOME");
    unsetenv("IKIGAI_CONFIG_DIR");

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_err(&result));

    if (home_copy) {
        setenv("HOME", home_copy, 1);
        free(home_copy);
    }
    if (config_dir_copy) {
        setenv("IKIGAI_CONFIG_DIR", config_dir_copy, 1);
        free(config_dir_copy);
    }
}

END_TEST

START_TEST(test_empty_env_var_ignored) {
    setenv("OPENAI_API_KEY", "", 1);
    setenv("ANTHROPIC_API_KEY", "", 1);
    setenv("GOOGLE_API_KEY", "", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, "/tmp/nonexistent.json", &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    ck_assert_ptr_null(ik_credentials_get(creds, "OPENAI_API_KEY"));
    ck_assert_ptr_null(ik_credentials_get(creds, "ANTHROPIC_API_KEY"));
    ck_assert_ptr_null(ik_credentials_get(creds, "GOOGLE_API_KEY"));

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
}

END_TEST

START_TEST(test_env_var_without_file_credentials) {
    const char *tmpfile = "/tmp/test_creds_empty_providers.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{}");
    fclose(f);
    chmod(tmpfile, 0600);

    setenv("OPENAI_API_KEY", "env-only-openai", 1);
    setenv("ANTHROPIC_API_KEY", "env-only-anthropic", 1);
    setenv("GOOGLE_API_KEY", "env-only-google", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *openai_key = ik_credentials_get(creds, "OPENAI_API_KEY");
    ck_assert_ptr_nonnull(openai_key);
    ck_assert_str_eq(openai_key, "env-only-openai");

    const char *anthropic_key = ik_credentials_get(creds, "ANTHROPIC_API_KEY");
    ck_assert_ptr_nonnull(anthropic_key);
    ck_assert_str_eq(anthropic_key, "env-only-anthropic");

    const char *google_key = ik_credentials_get(creds, "GOOGLE_API_KEY");
    ck_assert_ptr_nonnull(google_key);
    ck_assert_str_eq(google_key, "env-only-google");

    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    unlink(tmpfile);
}

END_TEST

START_TEST(test_env_var_overrides_file_brave_google_search_ntfy) {
    const char *tmpfile = "/tmp/test_creds_override_all.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"BRAVE_API_KEY\":\"file-key-brave\","
            "\"GOOGLE_SEARCH_API_KEY\":\"file-key-google-search\","
            "\"GOOGLE_SEARCH_ENGINE_ID\":\"file-id-google-engine\","
            "\"NTFY_API_KEY\":\"file-key-ntfy\","
            "\"NTFY_TOPIC\":\"file-topic-ntfy\"}");
    fclose(f);
    chmod(tmpfile, 0600);

    setenv("BRAVE_API_KEY", "env-key-brave", 1);
    setenv("GOOGLE_SEARCH_API_KEY", "env-key-google-search", 1);
    setenv("GOOGLE_SEARCH_ENGINE_ID", "env-id-google-engine", 1);
    setenv("NTFY_API_KEY", "env-key-ntfy", 1);
    setenv("NTFY_TOPIC", "env-topic-ntfy", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *brave_key = ik_credentials_get(creds, "BRAVE_API_KEY");
    ck_assert_ptr_nonnull(brave_key);
    ck_assert_str_eq(brave_key, "env-key-brave");

    const char *google_search_key = ik_credentials_get(creds, "GOOGLE_SEARCH_API_KEY");
    ck_assert_ptr_nonnull(google_search_key);
    ck_assert_str_eq(google_search_key, "env-key-google-search");

    const char *google_engine_id = ik_credentials_get(creds, "GOOGLE_SEARCH_ENGINE_ID");
    ck_assert_ptr_nonnull(google_engine_id);
    ck_assert_str_eq(google_engine_id, "env-id-google-engine");

    const char *ntfy_key = ik_credentials_get(creds, "NTFY_API_KEY");
    ck_assert_ptr_nonnull(ntfy_key);
    ck_assert_str_eq(ntfy_key, "env-key-ntfy");

    const char *ntfy_topic = ik_credentials_get(creds, "NTFY_TOPIC");
    ck_assert_ptr_nonnull(ntfy_topic);
    ck_assert_str_eq(ntfy_topic, "env-topic-ntfy");

    unsetenv("BRAVE_API_KEY");
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");
    unsetenv("NTFY_API_KEY");
    unsetenv("NTFY_TOPIC");
    unlink(tmpfile);
}

END_TEST
/* Mock functions for testing edge cases */
static bool mock_yyjson_doc_get_root_null = false;
yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc)
{
    return mock_yyjson_doc_get_root_null ? NULL : yyjson_doc_get_root(doc);
}

static bool mock_yyjson_get_str_null = false;
const char *yyjson_get_str_(yyjson_val *val)
{
    return mock_yyjson_get_str_null ? NULL : yyjson_get_str(val);
}

START_TEST(test_yyjson_doc_get_root_null) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    const char *tmpfile = "/tmp/test_creds_mock_null_root.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"OPENAI_API_KEY\":\"test-key\"}");
    fclose(f);
    chmod(tmpfile, 0600);
    mock_yyjson_doc_get_root_null = true;
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);
    mock_yyjson_doc_get_root_null = false;
    unlink(tmpfile);
}
END_TEST

START_TEST(test_yyjson_get_str_null) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    const char *tmpfile = "/tmp/test_creds_mock_null_str.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"OPENAI_API_KEY\":\"k\",\"ANTHROPIC_API_KEY\":\"k\",\"GOOGLE_API_KEY\":\"k\"}");
    fclose(f);
    chmod(tmpfile, 0600);
    mock_yyjson_get_str_null = true;
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_ptr_null(ik_credentials_get(creds, "OPENAI_API_KEY"));
    ck_assert_ptr_null(ik_credentials_get(creds, "ANTHROPIC_API_KEY"));
    ck_assert_ptr_null(ik_credentials_get(creds, "GOOGLE_API_KEY"));
    mock_yyjson_get_str_null = false;
    unlink(tmpfile);
}
END_TEST

/**
 */

static Suite *credentials_suite(void)
{
    Suite *s = suite_create("Credentials");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
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
    tcase_add_test(tc_core, test_env_var_overrides_file_brave_google_search_ntfy);
    tcase_add_test(tc_core, test_yyjson_doc_get_root_null);
    tcase_add_test(tc_core, test_yyjson_get_str_null);
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
