#include "tests/test_constants.h"
/**
 * @file credentials_optional_test.c
 * @brief Coverage tests for optional credentials in credentials.c
 *
 * Tests for optional credentials: GOOGLE_SEARCH_API_KEY, GOOGLE_SEARCH_ENGINE_ID,
 * IKIGAI_DB_PASS, BRAVE_API_KEY, NTFY_API_KEY, NTFY_TOPIC.
 */

#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
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
    char *path = talloc_asprintf(test_ctx, "/tmp/ikigai_creds_opt_%d.json", getpid());
    FILE *f = fopen(path, "w");
    if (!f) {
        ck_abort_msg("Failed to create temp file: %s", strerror(errno));
    }
    fprintf(f, "%s", content);
    fclose(f);
    chmod(path, 0600);
    return path;
}

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

static Suite *credentials_optional_suite(void)
{
    Suite *s = suite_create("Credentials Optional");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_optional_credentials_from_file);
    tcase_add_test(tc_core, test_optional_credentials_from_env);
    tcase_add_test(tc_core, test_optional_credentials_env_override);
    tcase_add_test(tc_core, test_credentials_get_optional);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = credentials_optional_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/credentials/credentials_optional_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
