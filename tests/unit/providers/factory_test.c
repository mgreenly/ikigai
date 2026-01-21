#include "../../test_constants.h"
/**
 * @file factory_test.c
 * @brief Unit tests for provider factory
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "providers/factory.h"
#include "error.h"

// Helper macro to check if a string contains a substring
#define ck_assert_str_contains(haystack, needle) \
        ck_assert_msg(strstr(haystack, needle) != NULL, \
                      "Expected '%s' to contain '%s'", haystack, needle)

/* ================================================================
 * Environment Variable Mapping Tests
 * ================================================================ */

START_TEST(test_env_var_openai) {
    const char *env_var = ik_provider_env_var("openai");
    ck_assert_ptr_nonnull(env_var);
    ck_assert_str_eq(env_var, "OPENAI_API_KEY");
}
END_TEST

START_TEST(test_env_var_anthropic) {
    const char *env_var = ik_provider_env_var("anthropic");
    ck_assert_ptr_nonnull(env_var);
    ck_assert_str_eq(env_var, "ANTHROPIC_API_KEY");
}
END_TEST

START_TEST(test_env_var_google) {
    const char *env_var = ik_provider_env_var("google");
    ck_assert_ptr_nonnull(env_var);
    ck_assert_str_eq(env_var, "GOOGLE_API_KEY");
}
END_TEST

START_TEST(test_env_var_unknown) {
    const char *env_var = ik_provider_env_var("unknown_provider");
    ck_assert_ptr_null(env_var);
}
END_TEST

START_TEST(test_env_var_null) {
    const char *env_var = ik_provider_env_var(NULL);
    ck_assert_ptr_null(env_var);
}

END_TEST
/* ================================================================
 * Provider Validation Tests
 * ================================================================ */

START_TEST(test_is_valid_openai) {
    ck_assert(ik_provider_is_valid("openai"));
}

END_TEST

START_TEST(test_is_valid_anthropic) {
    ck_assert(ik_provider_is_valid("anthropic"));
}

END_TEST

START_TEST(test_is_valid_google) {
    ck_assert(ik_provider_is_valid("google"));
}

END_TEST

START_TEST(test_is_valid_unknown) {
    ck_assert(!ik_provider_is_valid("unknown_provider"));
}

END_TEST

START_TEST(test_is_valid_null) {
    ck_assert(!ik_provider_is_valid(NULL));
}

END_TEST

START_TEST(test_is_valid_case_sensitive) {
    // Provider names are case-sensitive
    ck_assert(!ik_provider_is_valid("OpenAI"));
    ck_assert(!ik_provider_is_valid("ANTHROPIC"));
}

END_TEST
/* ================================================================
 * Provider List Tests
 * ================================================================ */

START_TEST(test_provider_list) {
    const char **list = ik_provider_list();
    ck_assert_ptr_nonnull(list);

    // Count providers
    size_t count = 0;
    for (size_t i = 0; list[i] != NULL; i++) {
        count++;
    }
    ck_assert_uint_eq(count, 3);

    // Check all three providers are present
    bool found_openai = false;
    bool found_anthropic = false;
    bool found_google = false;

    for (size_t i = 0; list[i] != NULL; i++) {
        if (strcmp(list[i], "openai") == 0) {
            found_openai = true;
        } else if (strcmp(list[i], "anthropic") == 0) {
            found_anthropic = true;
        } else if (strcmp(list[i], "google") == 0) {
            found_google = true;
        }
    }

    ck_assert(found_openai);
    ck_assert(found_anthropic);
    ck_assert(found_google);
}

END_TEST
/* ================================================================
 * Provider Creation Tests (Error Paths)
 * ================================================================ */

START_TEST(test_create_unknown_provider) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_provider_t *provider = NULL;

    res_t res = ik_provider_create(ctx, "unknown_provider", &provider);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);
    ck_assert_str_contains(error_message(res.err), "Unknown provider");

    talloc_free(ctx);
}

END_TEST

START_TEST(test_create_credentials_load_error) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_provider_t *provider = NULL;

    // Save current HOME value
    const char *home = getenv("HOME");
    char *saved_home = NULL;
    if (home != NULL) {
        saved_home = strdup(home);
    }

    // Save current IKIGAI_CONFIG_DIR value
    const char *config_dir = getenv("IKIGAI_CONFIG_DIR");
    char *saved_config_dir = NULL;
    if (config_dir != NULL) {
        saved_config_dir = strdup(config_dir);
    }

    // Unset HOME and IKIGAI_CONFIG_DIR to trigger expand_tilde error in ik_credentials_load
    unsetenv("HOME");
    unsetenv("IKIGAI_CONFIG_DIR");

    // Clear environment variables so it must use the file
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Now try to create provider - should fail because HOME is not set
    res_t res = ik_provider_create(ctx, "openai", &provider);

    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(res.err);
    ck_assert_str_contains(error_message(res.err), "HOME");

    // Restore HOME
    if (saved_home != NULL) {
        setenv("HOME", saved_home, 1);
        free(saved_home);
    }

    // Restore IKIGAI_CONFIG_DIR
    if (saved_config_dir != NULL) {
        setenv("IKIGAI_CONFIG_DIR", saved_config_dir, 1);
        free(saved_config_dir);
    }

    talloc_free(ctx);
}
END_TEST

START_TEST(test_create_missing_credentials) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_provider_t *provider = NULL;
    FILE *f = NULL;

    // Create test-specific config directory
    char *config_dir = talloc_asprintf(ctx, "/tmp/ikigai_factory_test_%d", getpid());
    char *creds_path = talloc_asprintf(ctx, "%s/credentials.json", config_dir);

    // Create directory
    mkdir(config_dir, 0700);

    // Set IKIGAI_CONFIG_DIR to use our test directory
    const char *orig_config_dir = getenv("IKIGAI_CONFIG_DIR");
    setenv("IKIGAI_CONFIG_DIR", config_dir, 1);

    // Create credentials file WITHOUT the provider we're requesting
    f = fopen(creds_path, "w");
    ck_assert_ptr_nonnull(f);
    // Create valid JSON but without openai credentials (using flat format)
    fprintf(f, "{\"ANTHROPIC_API_KEY\":\"test-key\"}");
    fclose(f);
    chmod(creds_path, 0600);

    // Clear ALL environment variables
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Try to create openai provider - should fail with missing credentials
    res_t res = ik_provider_create(ctx, "openai", &provider);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_MISSING_CREDENTIALS);
    ck_assert_str_contains(error_message(res.err), "OPENAI_API_KEY");

    // Cleanup: restore environment and remove test files
    unlink(creds_path);
    rmdir(config_dir);
    if (orig_config_dir != NULL) {
        setenv("IKIGAI_CONFIG_DIR", orig_config_dir, 1);
    } else {
        unsetenv("IKIGAI_CONFIG_DIR");
    }

    talloc_free(ctx);
}
END_TEST

START_TEST(test_create_success_openai) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_provider_t *provider = NULL;
    FILE *f = NULL;

    // Create test-specific config directory
    char *config_dir = talloc_asprintf(ctx, "/tmp/ikigai_factory_test_%d", getpid());
    char *creds_path = talloc_asprintf(ctx, "%s/credentials.json", config_dir);

    // Create directory
    mkdir(config_dir, 0700);

    // Set IKIGAI_CONFIG_DIR to use our test directory
    const char *orig_config_dir = getenv("IKIGAI_CONFIG_DIR");
    setenv("IKIGAI_CONFIG_DIR", config_dir, 1);

    // Create credentials file with openai credentials (using flat format)
    f = fopen(creds_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"OPENAI_API_KEY\":\"test-openai-key\"}");
    fclose(f);
    chmod(creds_path, 0600);

    // Clear environment variables
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Try to create openai provider - should succeed
    res_t res = ik_provider_create(ctx, "openai", &provider);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(provider);

    // Cleanup: restore environment and remove test files
    unlink(creds_path);
    rmdir(config_dir);
    if (orig_config_dir != NULL) {
        setenv("IKIGAI_CONFIG_DIR", orig_config_dir, 1);
    } else {
        unsetenv("IKIGAI_CONFIG_DIR");
    }

    talloc_free(ctx);
}
END_TEST

START_TEST(test_create_success_anthropic) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_provider_t *provider = NULL;
    FILE *f = NULL;

    // Create test-specific config directory
    char *config_dir = talloc_asprintf(ctx, "/tmp/ikigai_factory_test_%d", getpid());
    char *creds_path = talloc_asprintf(ctx, "%s/credentials.json", config_dir);

    // Create directory
    mkdir(config_dir, 0700);

    // Set IKIGAI_CONFIG_DIR to use our test directory
    const char *orig_config_dir = getenv("IKIGAI_CONFIG_DIR");
    setenv("IKIGAI_CONFIG_DIR", config_dir, 1);

    // Create credentials file with anthropic credentials (using flat format)
    f = fopen(creds_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"ANTHROPIC_API_KEY\":\"test-anthropic-key\"}");
    fclose(f);
    chmod(creds_path, 0600);

    // Clear environment variables
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Try to create anthropic provider - should succeed
    res_t res = ik_provider_create(ctx, "anthropic", &provider);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(provider);

    // Cleanup: restore environment and remove test files
    unlink(creds_path);
    rmdir(config_dir);
    if (orig_config_dir != NULL) {
        setenv("IKIGAI_CONFIG_DIR", orig_config_dir, 1);
    } else {
        unsetenv("IKIGAI_CONFIG_DIR");
    }

    talloc_free(ctx);
}
END_TEST

START_TEST(test_create_success_google) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_provider_t *provider = NULL;
    FILE *f = NULL;

    // Create test-specific config directory
    char *config_dir = talloc_asprintf(ctx, "/tmp/ikigai_factory_test_%d", getpid());
    char *creds_path = talloc_asprintf(ctx, "%s/credentials.json", config_dir);

    // Create directory
    mkdir(config_dir, 0700);

    // Set IKIGAI_CONFIG_DIR to use our test directory
    const char *orig_config_dir = getenv("IKIGAI_CONFIG_DIR");
    setenv("IKIGAI_CONFIG_DIR", config_dir, 1);

    // Create credentials file with google credentials (using flat format)
    f = fopen(creds_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"GOOGLE_API_KEY\":\"test-google-key\"}");
    fclose(f);
    chmod(creds_path, 0600);

    // Clear environment variables
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    // Try to create google provider - should succeed
    res_t res = ik_provider_create(ctx, "google", &provider);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(provider);

    // Cleanup: restore environment and remove test files
    unlink(creds_path);
    rmdir(config_dir);
    if (orig_config_dir != NULL) {
        setenv("IKIGAI_CONFIG_DIR", orig_config_dir, 1);
    } else {
        unsetenv("IKIGAI_CONFIG_DIR");
    }

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *factory_suite(void)
{
    Suite *s = suite_create("Provider Factory");

    TCase *tc_env_var = tcase_create("Environment Variable Mapping");
    tcase_set_timeout(tc_env_var, IK_TEST_TIMEOUT);
    tcase_add_test(tc_env_var, test_env_var_openai);
    tcase_add_test(tc_env_var, test_env_var_anthropic);
    tcase_add_test(tc_env_var, test_env_var_google);
    tcase_add_test(tc_env_var, test_env_var_unknown);
    tcase_add_test(tc_env_var, test_env_var_null);
    suite_add_tcase(s, tc_env_var);

    TCase *tc_validation = tcase_create("Provider Validation");
    tcase_set_timeout(tc_validation, IK_TEST_TIMEOUT);
    tcase_add_test(tc_validation, test_is_valid_openai);
    tcase_add_test(tc_validation, test_is_valid_anthropic);
    tcase_add_test(tc_validation, test_is_valid_google);
    tcase_add_test(tc_validation, test_is_valid_unknown);
    tcase_add_test(tc_validation, test_is_valid_null);
    tcase_add_test(tc_validation, test_is_valid_case_sensitive);
    suite_add_tcase(s, tc_validation);

    TCase *tc_list = tcase_create("Provider List");
    tcase_set_timeout(tc_list, IK_TEST_TIMEOUT);
    tcase_add_test(tc_list, test_provider_list);
    suite_add_tcase(s, tc_list);

    TCase *tc_create = tcase_create("Provider Creation");
    tcase_set_timeout(tc_create, IK_TEST_TIMEOUT);
    tcase_add_test(tc_create, test_create_unknown_provider);
    tcase_add_test(tc_create, test_create_credentials_load_error);
    tcase_add_test(tc_create, test_create_missing_credentials);
    tcase_add_test(tc_create, test_create_success_openai);
    tcase_add_test(tc_create, test_create_success_anthropic);
    tcase_add_test(tc_create, test_create_success_google);
    suite_add_tcase(s, tc_create);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = factory_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/factory_test.xml");

    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
