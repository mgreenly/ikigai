#include "../../test_constants.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "credentials.h"
#include "error.h"

static TALLOC_CTX *test_ctx = NULL;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

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

static Suite *credentials_suite(void)
{
    Suite *s = suite_create("Credentials_Override");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_env_var_overrides_file);
    tcase_add_test(tc_core, test_json_malformed_credentials);
    tcase_add_test(tc_core, test_tilde_expansion_no_home);
    tcase_add_test(tc_core, test_empty_env_var_ignored);
    tcase_add_test(tc_core, test_env_var_without_file_credentials);
    tcase_add_test(tc_core, test_env_var_overrides_file_brave_google_search_ntfy);
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
