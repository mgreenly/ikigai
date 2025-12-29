#include "../../../src/config.h"

#include "../../../src/error.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Mock control for yyjson_get_str_
static int32_t g_call_counter = 0;
static int32_t g_return_null_on_call = -1;

const char *yyjson_get_str_(yyjson_val *val)
{
    int32_t current_call = g_call_counter++;
    if (current_call == g_return_null_on_call) {
        return NULL;
    }
    return yyjson_get_str(val);
}

START_TEST(test_default_provider_with_value) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a test config file with default_provider
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_provider_test_%d.json", getpid());

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 50,\n"
            "  \"max_output_size\": 1048576,\n"
            "  \"default_provider\": \"anthropic\"\n"
            "}\n");
    fclose(f);

    // Load config
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, test_config, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    ck_assert_ptr_nonnull(cfg->default_provider);
    ck_assert_str_eq(cfg->default_provider, "anthropic");

    // Clean up
    unlink(test_config);
    talloc_free(ctx);
}
END_TEST START_TEST(test_default_provider_invalid_type)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a test config file with invalid default_provider type
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_provider_invalid_%d.json", getpid());

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 50,\n"
            "  \"max_output_size\": 1048576,\n"
            "  \"default_provider\": 123\n"
            "}\n");
    fclose(f);

    // Load config - should fail
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, test_config, &cfg);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_PARSE);

    // Clean up
    unlink(test_config);
    talloc_free(ctx);
}

END_TEST START_TEST(test_default_provider_empty_string)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a test config file with empty default_provider
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_provider_empty_%d.json", getpid());

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 50,\n"
            "  \"max_output_size\": 1048576,\n"
            "  \"default_provider\": \"\"\n"
            "}\n");
    fclose(f);

    // Load config - should succeed with NULL default_provider
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, test_config, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    ck_assert_ptr_null(cfg->default_provider);

    // Clean up
    unlink(test_config);
    talloc_free(ctx);
}

END_TEST START_TEST(test_get_default_provider_env_override)
{
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

END_TEST START_TEST(test_get_default_provider_env_empty)
{
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

END_TEST START_TEST(test_get_default_provider_from_config)
{
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

END_TEST START_TEST(test_get_default_provider_config_empty)
{
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

END_TEST START_TEST(test_get_default_provider_fallback)
{
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

END_TEST START_TEST(test_default_provider_null_from_yyjson)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a test config file with default_provider
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_provider_null_%d.json", getpid());

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 50,\n"
            "  \"max_output_size\": 1048576,\n"
            "  \"default_provider\": \"google\"\n"
            "}\n");
    fclose(f);

    // Reset mock counter and set to return NULL on the call for default_provider
    // Calls are: model, address, default_provider
    // (system_message is null so yyjson_get_str_ is not called for it)
    // So default_provider is the 3rd call (index 2)
    g_call_counter = 0;
    g_return_null_on_call = 2;

    // Load config - should succeed with NULL default_provider due to mock
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, test_config, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    // Even though JSON has a value, mock returns NULL, so config should be NULL
    ck_assert_ptr_null(cfg->default_provider);

    // Reset mock
    g_return_null_on_call = -1;

    // Clean up
    unlink(test_config);
    talloc_free(ctx);
}

END_TEST

static Suite *default_provider_suite(void)
{
    Suite *s = suite_create("Config Default Provider");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_default_provider_with_value);
    tcase_add_test(tc_core, test_default_provider_invalid_type);
    tcase_add_test(tc_core, test_default_provider_empty_string);
    tcase_add_test(tc_core, test_default_provider_null_from_yyjson);
    tcase_add_test(tc_core, test_get_default_provider_env_override);
    tcase_add_test(tc_core, test_get_default_provider_env_empty);
    tcase_add_test(tc_core, test_get_default_provider_from_config);
    tcase_add_test(tc_core, test_get_default_provider_config_empty);
    tcase_add_test(tc_core, test_get_default_provider_fallback);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = default_provider_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
