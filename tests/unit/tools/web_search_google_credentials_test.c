#include "../../test_constants.h"

#include <check.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "../../../src/tools/web_search_google/credentials.h"
#include "../../../src/wrapper.h"

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
}

static void teardown(void)
{
    if (test_config_file != NULL) {
        unlink(test_config_file);
    }
    if (test_config_dir != NULL) {
        rmdir(test_config_dir);
    }
    talloc_free(test_ctx);
}

START_TEST(test_load_from_env_both_set) {
    setenv("GOOGLE_SEARCH_API_KEY", "test-api-key", 1);
    setenv("GOOGLE_SEARCH_ENGINE_ID", "test-engine-id", 1);

    char *api_key = NULL;
    char *engine_id = NULL;
    int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

    ck_assert_int_eq(result, 0);
    ck_assert_str_eq(api_key, "test-api-key");
    ck_assert_str_eq(engine_id, "test-engine-id");

    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");
}
END_TEST

START_TEST(test_load_from_env_api_key_only) {
    setenv("GOOGLE_SEARCH_API_KEY", "test-api-key", 1);
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    char *api_key = NULL;
    char *engine_id = NULL;
    int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

    ck_assert_int_eq(result, -1);

    unsetenv("GOOGLE_SEARCH_API_KEY");
}
END_TEST

START_TEST(test_load_from_env_engine_id_only) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    setenv("GOOGLE_SEARCH_ENGINE_ID", "test-engine-id", 1);

    char *api_key = NULL;
    char *engine_id = NULL;
    int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

    ck_assert_int_eq(result, -1);

    unsetenv("GOOGLE_SEARCH_ENGINE_ID");
}
END_TEST

START_TEST(test_load_from_file_both_keys) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    const char *json = "{\"web_search\":{\"google\":{\"api_key\":\"file-api-key\",\"engine_id\":\"file-engine-id\"}}}";

    const char *home = getenv("HOME");
    char *config_dir = talloc_asprintf(test_ctx, "%s/.config/ikigai", home);
    char *config_file = talloc_asprintf(test_ctx, "%s/credentials.json", config_dir);

    mkdir(config_dir, 0700);
    FILE *f = fopen(config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(config_file, 0600);

        char *api_key = NULL;
        char *engine_id = NULL;
        int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

        ck_assert_int_eq(result, 0);
        ck_assert_str_eq(api_key, "file-api-key");
        ck_assert_str_eq(engine_id, "file-engine-id");

        unlink(config_file);
    }
    rmdir(config_dir);
}
END_TEST

START_TEST(test_file_missing_web_search_key) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    const char *json = "{\"other_key\":{}}";

    const char *home = getenv("HOME");
    char *config_dir = talloc_asprintf(test_ctx, "%s/.config/ikigai", home);
    char *config_file = talloc_asprintf(test_ctx, "%s/credentials.json", config_dir);

    mkdir(config_dir, 0700);
    FILE *f = fopen(config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(config_file, 0600);

        char *api_key = NULL;
        char *engine_id = NULL;
        int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

        ck_assert_int_eq(result, -1);

        unlink(config_file);
    }
    rmdir(config_dir);
}
END_TEST

START_TEST(test_file_missing_google_key) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    const char *json = "{\"web_search\":{\"other_provider\":{}}}";

    const char *home = getenv("HOME");
    char *config_dir = talloc_asprintf(test_ctx, "%s/.config/ikigai", home);
    char *config_file = talloc_asprintf(test_ctx, "%s/credentials.json", config_dir);

    mkdir(config_dir, 0700);
    FILE *f = fopen(config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(config_file, 0600);

        char *api_key = NULL;
        char *engine_id = NULL;
        int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

        ck_assert_int_eq(result, -1);

        unlink(config_file);
    }
    rmdir(config_dir);
}
END_TEST

START_TEST(test_file_missing_api_key_field) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    const char *json = "{\"web_search\":{\"google\":{\"engine_id\":\"id-only\"}}}";

    const char *home = getenv("HOME");
    char *config_dir = talloc_asprintf(test_ctx, "%s/.config/ikigai", home);
    char *config_file = talloc_asprintf(test_ctx, "%s/credentials.json", config_dir);

    mkdir(config_dir, 0700);
    FILE *f = fopen(config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(config_file, 0600);

        char *api_key = NULL;
        char *engine_id = NULL;
        int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

        ck_assert_int_eq(result, -1);

        unlink(config_file);
    }
    rmdir(config_dir);
}
END_TEST

START_TEST(test_file_api_key_not_string) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    const char *json = "{\"web_search\":{\"google\":{\"api_key\":123,\"engine_id\":\"valid-id\"}}}";

    const char *home = getenv("HOME");
    char *config_dir = talloc_asprintf(test_ctx, "%s/.config/ikigai", home);
    char *config_file = talloc_asprintf(test_ctx, "%s/credentials.json", config_dir);

    mkdir(config_dir, 0700);
    FILE *f = fopen(config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(config_file, 0600);

        char *api_key = NULL;
        char *engine_id = NULL;
        int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

        ck_assert_int_eq(result, -1);

        unlink(config_file);
    }
    rmdir(config_dir);
}
END_TEST

START_TEST(test_file_invalid_json) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    const char *json = "{invalid json here}";

    const char *home = getenv("HOME");
    char *config_dir = talloc_asprintf(test_ctx, "%s/.config/ikigai", home);
    char *config_file = talloc_asprintf(test_ctx, "%s/credentials.json", config_dir);

    mkdir(config_dir, 0700);
    FILE *f = fopen(config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(config_file, 0600);

        char *api_key = NULL;
        char *engine_id = NULL;
        int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

        ck_assert_int_eq(result, -1);

        unlink(config_file);
    }
    rmdir(config_dir);
}
END_TEST

START_TEST(test_empty_api_key_string) {
    setenv("GOOGLE_SEARCH_API_KEY", "", 1);
    setenv("GOOGLE_SEARCH_ENGINE_ID", "valid-id", 1);

    char *api_key = NULL;
    char *engine_id = NULL;
    int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

    ck_assert_int_eq(result, -1);

    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");
}
END_TEST

START_TEST(test_empty_engine_id_string) {
    setenv("GOOGLE_SEARCH_API_KEY", "valid-key", 1);
    setenv("GOOGLE_SEARCH_ENGINE_ID", "", 1);

    char *api_key = NULL;
    char *engine_id = NULL;
    int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

    ck_assert_int_eq(result, -1);

    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");
}
END_TEST

START_TEST(test_large_file_buffer_growth) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    char *large_json = talloc_asprintf(test_ctx, "{\"web_search\":{\"google\":{\"api_key\":\"");
    for (int32_t i = 0; i < 5000; i++) {
        large_json = talloc_asprintf_append(large_json, "x");
    }
    large_json = talloc_asprintf_append(large_json, "\",\"engine_id\":\"");
    for (int32_t i = 0; i < 5000; i++) {
        large_json = talloc_asprintf_append(large_json, "y");
    }
    large_json = talloc_asprintf_append(large_json, "\"}}}");

    const char *home = getenv("HOME");
    char *config_dir = talloc_asprintf(test_ctx, "%s/.config/ikigai", home);
    char *config_file = talloc_asprintf(test_ctx, "%s/credentials.json", config_dir);

    mkdir(config_dir, 0700);
    FILE *f = fopen(config_file, "w");
    if (f) {
        fprintf(f, "%s", large_json);
        fclose(f);
        chmod(config_file, 0600);

        char *api_key = NULL;
        char *engine_id = NULL;
        int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

        ck_assert_int_eq(result, 0);
        ck_assert_uint_eq(strlen(api_key), 5000);
        ck_assert_uint_eq(strlen(engine_id), 5000);

        unlink(config_file);
    }
    rmdir(config_dir);
}
END_TEST

START_TEST(test_no_env_no_file) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    const char *home = getenv("HOME");
    char *config_file = talloc_asprintf(test_ctx, "%s/.config/ikigai/credentials.json", home);

    struct stat st;
    bool had_file = (stat(config_file, &st) == 0);
    char *backup_file = NULL;
    if (had_file) {
        backup_file = talloc_asprintf(test_ctx, "%s.backup_%d", config_file, getpid());
        rename(config_file, backup_file);
    }

    char *api_key = NULL;
    char *engine_id = NULL;
    int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

    ck_assert_int_eq(result, -1);

    if (had_file && backup_file != NULL) {
        rename(backup_file, config_file);
    }
}
END_TEST

START_TEST(test_getpwuid_fallback) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    const char *json = "{\"web_search\":{\"google\":{\"api_key\":\"fallback-key\",\"engine_id\":\"fallback-id\"}}}";

    const char *orig_home = getenv("HOME");
    ck_assert_ptr_nonnull(orig_home);

    char *config_dir = talloc_asprintf(test_ctx, "%s/.config/ikigai", orig_home);
    char *config_file = talloc_asprintf(test_ctx, "%s/credentials.json", config_dir);

    mkdir(config_dir, 0700);
    FILE *f = fopen(config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(config_file, 0600);

        unsetenv("HOME");

        char *api_key = NULL;
        char *engine_id = NULL;
        int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

        ck_assert_int_eq(result, 0);
        ck_assert_str_eq(api_key, "fallback-key");
        ck_assert_str_eq(engine_id, "fallback-id");

        setenv("HOME", orig_home, 1);
        unlink(config_file);
    }
    rmdir(config_dir);
    setenv("HOME", orig_home, 1);
}
END_TEST

START_TEST(test_file_permission_error) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    const char *json = "{\"web_search\":{\"google\":{\"api_key\":\"test-key\",\"engine_id\":\"test-id\"}}}";

    const char *home = getenv("HOME");
    char *config_dir = talloc_asprintf(test_ctx, "%s/.config/ikigai", home);
    char *config_file = talloc_asprintf(test_ctx, "%s/credentials.json", config_dir);

    mkdir(config_dir, 0700);
    FILE *f = fopen(config_file, "w");
    if (f) {
        fprintf(f, "%s", json);
        fclose(f);
        chmod(config_file, 0000);

        char *api_key = NULL;
        char *engine_id = NULL;
        int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

        ck_assert_int_eq(result, -1);

        chmod(config_file, 0600);
        unlink(config_file);
    }
    rmdir(config_dir);
}
END_TEST

static bool mock_getenv_home_null = false;
static bool mock_getpwuid_null = false;

char *getenv_(const char *name)
{
    if (mock_getenv_home_null && strcmp(name, "HOME") == 0) {
        return NULL;
    }
    return getenv(name);
}

struct passwd *getpwuid_(uid_t uid)
{
    if (mock_getpwuid_null) {
        return NULL;
    }
    return getpwuid(uid);
}

START_TEST(test_no_home_no_getpwuid) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    mock_getenv_home_null = true;
    mock_getpwuid_null = true;

    char *api_key = NULL;
    char *engine_id = NULL;
    int32_t result = load_credentials(test_ctx, &api_key, &engine_id);

    ck_assert_int_eq(result, -1);
    ck_assert_ptr_null(api_key);
    ck_assert_ptr_null(engine_id);

    mock_getenv_home_null = false;
    mock_getpwuid_null = false;
}
END_TEST

static Suite *web_search_google_credentials_suite(void)
{
    Suite *s = suite_create("WebSearchGoogleCredentials");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_load_from_env_both_set);
    tcase_add_test(tc_core, test_load_from_env_api_key_only);
    tcase_add_test(tc_core, test_load_from_env_engine_id_only);
    tcase_add_test(tc_core, test_load_from_file_both_keys);
    tcase_add_test(tc_core, test_file_missing_web_search_key);
    tcase_add_test(tc_core, test_file_missing_google_key);
    tcase_add_test(tc_core, test_file_missing_api_key_field);
    tcase_add_test(tc_core, test_file_api_key_not_string);
    tcase_add_test(tc_core, test_file_invalid_json);
    tcase_add_test(tc_core, test_empty_api_key_string);
    tcase_add_test(tc_core, test_empty_engine_id_string);
    tcase_add_test(tc_core, test_large_file_buffer_growth);
    tcase_add_test(tc_core, test_no_env_no_file);
    tcase_add_test(tc_core, test_getpwuid_fallback);
    tcase_add_test(tc_core, test_file_permission_error);
    tcase_add_test(tc_core, test_no_home_no_getpwuid);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = web_search_google_credentials_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
