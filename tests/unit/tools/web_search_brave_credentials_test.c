#include "../../test_constants.h"

#include <check.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "../../../src/tools/web_search_brave/credentials.h"

static TALLOC_CTX *test_ctx;
static char *test_config_dir;
static char *test_config_file;

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    const char *home = getenv("HOME");
    test_config_dir = talloc_asprintf(test_ctx, "%s/.config/ikigai_test_%d", home, getpid());
    test_config_file = talloc_asprintf(test_ctx, "%s/credentials.json", test_config_dir);

    mkdir(test_config_dir, 0700);

    setenv("IKIGAI_CONFIG_DIR", test_config_dir, 1);
    setenv("IKIGAI_BIN_DIR", "/tmp/test_bin", 1);
    setenv("IKIGAI_DATA_DIR", "/tmp/test_data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/tmp/test_libexec", 1);
}

static void teardown(void)
{
    if (test_config_file != NULL) {
        unlink(test_config_file);
    }
    if (test_config_dir != NULL) {
        rmdir(test_config_dir);
    }
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
    talloc_free(test_ctx);
}

START_TEST(test_load_from_env) {
    setenv("BRAVE_API_KEY", "test-api-key", 1);

    char *api_key = NULL;
    int32_t result = load_api_key(test_ctx, &api_key);

    ck_assert_int_eq(result, 0);
    ck_assert_str_eq(api_key, "test-api-key");

    unsetenv("BRAVE_API_KEY");
}
END_TEST

START_TEST(test_load_from_env_empty) {
    setenv("BRAVE_API_KEY", "", 1);

    char *api_key = NULL;
    int32_t result = load_api_key(test_ctx, &api_key);

    ck_assert_int_eq(result, -1);

    unsetenv("BRAVE_API_KEY");
}
END_TEST

START_TEST(test_load_from_file) {
    unsetenv("BRAVE_API_KEY");

    const char *json = "{\"web_search\":{\"brave\":{\"api_key\":\"file-api-key\"}}}";

    FILE *f = fopen(test_config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(test_config_file, 0600);

        char *api_key = NULL;
        int32_t result = load_api_key(test_ctx, &api_key);

        ck_assert_int_eq(result, 0);
        ck_assert_str_eq(api_key, "file-api-key");

        unlink(test_config_file);
    }
}
END_TEST

START_TEST(test_file_missing_web_search_key) {
    unsetenv("BRAVE_API_KEY");

    const char *json = "{\"other_key\":{}}";

    FILE *f = fopen(test_config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(test_config_file, 0600);

        char *api_key = NULL;
        int32_t result = load_api_key(test_ctx, &api_key);

        ck_assert_int_eq(result, -1);

        unlink(test_config_file);
    }
}
END_TEST

START_TEST(test_file_missing_brave_key) {
    unsetenv("BRAVE_API_KEY");

    const char *json = "{\"web_search\":{\"other_provider\":{}}}";

    FILE *f = fopen(test_config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(test_config_file, 0600);

        char *api_key = NULL;
        int32_t result = load_api_key(test_ctx, &api_key);

        ck_assert_int_eq(result, -1);

        unlink(test_config_file);
    }
}
END_TEST

START_TEST(test_file_missing_api_key_field) {
    unsetenv("BRAVE_API_KEY");

    const char *json = "{\"web_search\":{\"brave\":{\"other_field\":\"value\"}}}";

    FILE *f = fopen(test_config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(test_config_file, 0600);

        char *api_key = NULL;
        int32_t result = load_api_key(test_ctx, &api_key);

        ck_assert_int_eq(result, -1);

        unlink(test_config_file);
    }
}
END_TEST

START_TEST(test_file_api_key_not_string) {
    unsetenv("BRAVE_API_KEY");

    const char *json = "{\"web_search\":{\"brave\":{\"api_key\":123}}}";

    FILE *f = fopen(test_config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(test_config_file, 0600);

        char *api_key = NULL;
        int32_t result = load_api_key(test_ctx, &api_key);

        ck_assert_int_eq(result, -1);

        unlink(test_config_file);
    }
}
END_TEST

START_TEST(test_file_invalid_json) {
    unsetenv("BRAVE_API_KEY");

    const char *json = "{invalid json here}";

    FILE *f = fopen(test_config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(test_config_file, 0600);

        char *api_key = NULL;
        int32_t result = load_api_key(test_ctx, &api_key);

        ck_assert_int_eq(result, -1);

        unlink(test_config_file);
    }
}
END_TEST

START_TEST(test_no_env_no_file) {
    unsetenv("BRAVE_API_KEY");

    struct stat st;
    bool had_file = (stat(test_config_file, &st) == 0);
    char *backup_file = NULL;
    if (had_file) {
        backup_file = talloc_asprintf(test_ctx, "%s.backup_%d", test_config_file, getpid());
        rename(test_config_file, backup_file);
    }

    char *api_key = NULL;
    int32_t result = load_api_key(test_ctx, &api_key);

    ck_assert_int_eq(result, -1);

    if (had_file && backup_file != NULL) {
        rename(backup_file, test_config_file);
    }
}
END_TEST

START_TEST(test_no_home_fails) {
    unsetenv("BRAVE_API_KEY");

    const char *json = "{\"web_search\":{\"brave\":{\"api_key\":\"test-key\"}}}";

    FILE *f = fopen(test_config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(test_config_file, 0600);

        const char *orig_home = getenv("HOME");
        unsetenv("HOME");

        char *api_key = NULL;
        int32_t result = load_api_key(test_ctx, &api_key);

        ck_assert_int_eq(result, -1);

        if (orig_home != NULL) {
            setenv("HOME", orig_home, 1);
        }
        unlink(test_config_file);
    }
}
END_TEST

START_TEST(test_file_permission_error) {
    unsetenv("BRAVE_API_KEY");

    const char *json = "{\"web_search\":{\"brave\":{\"api_key\":\"test-key\"}}}";

    FILE *f = fopen(test_config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(test_config_file, 0000);

        char *api_key = NULL;
        int32_t result = load_api_key(test_ctx, &api_key);

        ck_assert_int_eq(result, -1);

        chmod(test_config_file, 0600);
        unlink(test_config_file);
    }
}
END_TEST

static Suite *web_search_brave_credentials_suite(void)
{
    Suite *s = suite_create("WebSearchBraveCredentials");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_load_from_env);
    tcase_add_test(tc_core, test_load_from_env_empty);
    tcase_add_test(tc_core, test_load_from_file);
    tcase_add_test(tc_core, test_file_missing_web_search_key);
    tcase_add_test(tc_core, test_file_missing_brave_key);
    tcase_add_test(tc_core, test_file_missing_api_key_field);
    tcase_add_test(tc_core, test_file_api_key_not_string);
    tcase_add_test(tc_core, test_file_invalid_json);
    tcase_add_test(tc_core, test_no_env_no_file);
    tcase_add_test(tc_core, test_no_home_fails);
    tcase_add_test(tc_core, test_file_permission_error);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = web_search_brave_credentials_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
