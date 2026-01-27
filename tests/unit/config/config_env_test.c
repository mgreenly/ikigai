#include "../../../src/config.h"
#include "../../../src/paths.h"

#include "../../../src/error.h"
#include "../../../src/vendor/yyjson/yyjson.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

START_TEST(test_config_with_env_var_overrides) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Set environment variables
    setenv("IKIGAI_DB_HOST", "envhost", 1);
    setenv("IKIGAI_DB_PORT", "9876", 1);
    setenv("IKIGAI_DB_NAME", "envdb", 1);
    setenv("IKIGAI_DB_USER", "envuser", 1);

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with different database fields
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config - env vars should override these
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
            "  \"db_host\": \"confighost\",\n"
            "  \"db_port\": 5433,\n"
            "  \"db_name\": \"configdb\",\n"
            "  \"db_user\": \"configuser\"\n"
            "}\n");
    fclose(f);

    // Load config
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);

    // Environment variables should override config file
    ck_assert_ptr_nonnull(cfg->db_host);
    ck_assert_str_eq(cfg->db_host, "envhost");
    ck_assert_int_eq(cfg->db_port, 9876);
    ck_assert_ptr_nonnull(cfg->db_name);
    ck_assert_str_eq(cfg->db_name, "envdb");
    ck_assert_ptr_nonnull(cfg->db_user);
    ck_assert_str_eq(cfg->db_user, "envuser");

    // Clean up environment variables
    unsetenv("IKIGAI_DB_HOST");
    unsetenv("IKIGAI_DB_PORT");
    unsetenv("IKIGAI_DB_NAME");
    unsetenv("IKIGAI_DB_USER");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_env_var_overrides_no_file) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Set environment variables BEFORE creating paths
    setenv("IKIGAI_DB_HOST", "envhost", 1);
    setenv("IKIGAI_DB_PORT", "9876", 1);
    setenv("IKIGAI_DB_NAME", "envdb", 1);
    setenv("IKIGAI_DB_USER", "envuser", 1);

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Do NOT create config file - will use defaults + env var overrides

    // Load config
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);

    // Environment variables should override defaults
    ck_assert_ptr_nonnull(cfg->db_host);
    ck_assert_str_eq(cfg->db_host, "envhost");
    ck_assert_int_eq(cfg->db_port, 9876);
    ck_assert_ptr_nonnull(cfg->db_name);
    ck_assert_str_eq(cfg->db_name, "envdb");
    ck_assert_ptr_nonnull(cfg->db_user);
    ck_assert_str_eq(cfg->db_user, "envuser");

    // Clean up environment variables
    unsetenv("IKIGAI_DB_HOST");
    unsetenv("IKIGAI_DB_PORT");
    unsetenv("IKIGAI_DB_NAME");
    unsetenv("IKIGAI_DB_USER");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_empty_string_env_vars) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Set environment variables to empty strings (not unset, but empty)
    setenv("IKIGAI_DB_HOST", "", 1);
    setenv("IKIGAI_DB_PORT", "", 1);
    setenv("IKIGAI_DB_NAME", "", 1);
    setenv("IKIGAI_DB_USER", "", 1);

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db values
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
            "  \"db_host\": \"confighost\",\n"
            "  \"db_port\": 5433,\n"
            "  \"db_name\": \"configdb\",\n"
            "  \"db_user\": \"configuser\"\n"
            "}\n");
    fclose(f);

    // Load config - empty env vars should be ignored, config values used
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);

    // Empty string env vars should be ignored, config file values used
    ck_assert_ptr_nonnull(cfg->db_host);
    ck_assert_str_eq(cfg->db_host, "confighost");
    ck_assert_int_eq(cfg->db_port, 5433);
    ck_assert_ptr_nonnull(cfg->db_name);
    ck_assert_str_eq(cfg->db_name, "configdb");
    ck_assert_ptr_nonnull(cfg->db_user);
    ck_assert_str_eq(cfg->db_user, "configuser");

    // Clean up environment variables
    unsetenv("IKIGAI_DB_HOST");
    unsetenv("IKIGAI_DB_PORT");
    unsetenv("IKIGAI_DB_NAME");
    unsetenv("IKIGAI_DB_USER");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_empty_string_env_vars_no_file) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Set environment variables to empty strings
    setenv("IKIGAI_DB_HOST", "", 1);
    setenv("IKIGAI_DB_PORT", "", 1);
    setenv("IKIGAI_DB_NAME", "", 1);
    setenv("IKIGAI_DB_USER", "", 1);

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Do NOT create config file - will use defaults + empty env var override

    // Load config - empty env vars should be ignored, defaults used
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);

    // Empty string env vars should be ignored, defaults used
    ck_assert_ptr_nonnull(cfg->db_host);
    ck_assert_str_eq(cfg->db_host, "localhost");
    ck_assert_int_eq(cfg->db_port, 5432);
    ck_assert_ptr_nonnull(cfg->db_name);
    ck_assert_str_eq(cfg->db_name, "ikigai");
    ck_assert_ptr_nonnull(cfg->db_user);
    ck_assert_str_eq(cfg->db_user, "ikigai");

    // Clean up environment variables
    unsetenv("IKIGAI_DB_HOST");
    unsetenv("IKIGAI_DB_PORT");
    unsetenv("IKIGAI_DB_NAME");
    unsetenv("IKIGAI_DB_USER");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_invalid_env_port) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Set invalid port environment variable
    setenv("IKIGAI_DB_PORT", "not_a_number", 1);

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_port = 5433
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
            "  \"db_host\": \"confighost\",\n"
            "  \"db_port\": 5433,\n"
            "  \"db_name\": \"configdb\",\n"
            "  \"db_user\": \"configuser\"\n"
            "}\n");
    fclose(f);

    // Load config - invalid env var should be ignored, config file value used
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);

    // Invalid port env var should be ignored, config file value used
    ck_assert_int_eq(cfg->db_port, 5433);

    // Clean up environment variable
    unsetenv("IKIGAI_DB_PORT");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_env_port_trailing_chars) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Set port with trailing non-numeric characters
    setenv("IKIGAI_DB_PORT", "5432abc", 1);

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_port = 9999
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
            "  \"db_port\": 9999\n"
            "}\n");
    fclose(f);

    // Load config - invalid env var should be ignored, config file value used
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);

    // Port with trailing chars should be ignored, config file value used
    ck_assert_int_eq(cfg->db_port, 9999);

    // Clean up environment variable
    unsetenv("IKIGAI_DB_PORT");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_env_port_too_low) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Set port too low
    setenv("IKIGAI_DB_PORT", "0", 1);

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_port = 8888
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
            "  \"db_port\": 8888\n"
            "}\n");
    fclose(f);

    // Load config - invalid env var should be ignored, config file value used
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);

    // Port too low should be ignored, config file value used
    ck_assert_int_eq(cfg->db_port, 8888);

    // Clean up environment variable
    unsetenv("IKIGAI_DB_PORT");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_env_port_too_high) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Set port too high
    setenv("IKIGAI_DB_PORT", "70000", 1);

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_port = 7777
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
            "  \"db_port\": 7777\n"
            "}\n");
    fclose(f);

    // Load config - invalid env var should be ignored, config file value used
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);

    // Port too high should be ignored, config file value used
    ck_assert_int_eq(cfg->db_port, 7777);

    // Clean up environment variable
    unsetenv("IKIGAI_DB_PORT");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

static Suite *config_suite(void)
{
    Suite *s = suite_create("Config Environment Variables");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_config_with_env_var_overrides);
    tcase_add_test(tc_core, test_config_with_env_var_overrides_no_file);
    tcase_add_test(tc_core, test_config_with_empty_string_env_vars);
    tcase_add_test(tc_core, test_config_with_empty_string_env_vars_no_file);
    tcase_add_test(tc_core, test_config_with_invalid_env_port);
    tcase_add_test(tc_core, test_config_with_env_port_trailing_chars);
    tcase_add_test(tc_core, test_config_with_env_port_too_low);
    tcase_add_test(tc_core, test_config_with_env_port_too_high);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = config_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/config/config_env_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
