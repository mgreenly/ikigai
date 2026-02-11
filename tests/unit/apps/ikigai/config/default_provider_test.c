#include "apps/ikigai/config.h"
#include "apps/ikigai/paths.h"

#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

START_TEST(test_get_default_provider_env_override) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with default_provider set
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->default_provider = talloc_strdup(cfg, "openai");

    // Set environment variable
    setenv("IKIGAI_DEFAULT_PROVIDER", "google", 1);

    // Should return env var value
    const char *provider = ik_config_get_default_provider(cfg);
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider, "google");

    // Clean up
    unsetenv("IKIGAI_DEFAULT_PROVIDER");
    talloc_free(ctx);
}

END_TEST

START_TEST(test_get_default_provider_env_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with default_provider set
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->default_provider = talloc_strdup(cfg, "anthropic");

    // Set environment variable to empty string
    setenv("IKIGAI_DEFAULT_PROVIDER", "", 1);

    // Should fall back to config value
    const char *provider = ik_config_get_default_provider(cfg);
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider, "anthropic");

    // Clean up
    unsetenv("IKIGAI_DEFAULT_PROVIDER");
    talloc_free(ctx);
}

END_TEST

START_TEST(test_get_default_provider_from_config) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with default_provider set
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->default_provider = talloc_strdup(cfg, "google");

    // Ensure env var is not set
    unsetenv("IKIGAI_DEFAULT_PROVIDER");

    // Should return config value
    const char *provider = ik_config_get_default_provider(cfg);
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider, "google");

    // Clean up
    talloc_free(ctx);
}

END_TEST

START_TEST(test_get_default_provider_config_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with empty default_provider
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->default_provider = talloc_strdup(cfg, "");

    // Ensure env var is not set
    unsetenv("IKIGAI_DEFAULT_PROVIDER");

    // Should return hardcoded default
    const char *provider = ik_config_get_default_provider(cfg);
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider, "openai");

    // Clean up
    talloc_free(ctx);
}

END_TEST

START_TEST(test_get_default_provider_fallback) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with NULL default_provider
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->default_provider = NULL;

    // Ensure env var is not set
    unsetenv("IKIGAI_DEFAULT_PROVIDER");

    // Should return hardcoded default
    const char *provider = ik_config_get_default_provider(cfg);
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider, "openai");

    // Clean up
    talloc_free(ctx);
}

END_TEST

START_TEST(test_default_provider_loaded_from_defaults) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Load config (uses defaults since no config file)
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);

    // Config field should be NULL (not loaded from file)
    ck_assert_ptr_null(cfg->default_provider);

    // But ik_config_get_default_provider should return compiled default
    const char *provider = ik_config_get_default_provider(cfg);
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider, "openai");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

static Suite *default_provider_suite(void)
{
    Suite *s = suite_create("Config Default Provider");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_get_default_provider_env_override);
    tcase_add_test(tc_core, test_get_default_provider_env_empty);
    tcase_add_test(tc_core, test_get_default_provider_from_config);
    tcase_add_test(tc_core, test_get_default_provider_config_empty);
    tcase_add_test(tc_core, test_get_default_provider_fallback);
    tcase_add_test(tc_core, test_default_provider_loaded_from_defaults);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = default_provider_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/config/default_provider_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
