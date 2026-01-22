#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "../../../src/config.h"
#include "../../../src/paths.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"

START_TEST(test_error_code_strings) {
    // Test that new error codes have string representations
    const char *io_str = error_code_str(ERR_IO);
    ck_assert_str_eq(io_str, "IO error");

    const char *parse_str = error_code_str(ERR_PARSE);
    ck_assert_str_eq(parse_str, "Parse error");
}

END_TEST

START_TEST(test_config_port_too_low) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a config with port below 1024
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_model\": \"gpt-4-turbo\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\", \"listen_port\": 80}");
    fclose(f);

    // Try to load - should fail with OUT_OF_RANGE error
    ik_config_t *config = NULL;

    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_OUT_OF_RANGE);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_port_too_high) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a config with port above 65535
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_model\": \"gpt-4-turbo\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\", \"listen_port\": 70000}");
    fclose(f);

    // Try to load - should fail with OUT_OF_RANGE error
    ik_config_t *config = NULL;

    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_OUT_OF_RANGE);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_port_valid_range) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Get config path
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Test minimum valid port (1024)
    FILE *f1 = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f1);
    fprintf(f1,
            "{\"openai_model\": \"gpt-4-turbo\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\", \"listen_port\": 1024, \"max_tool_turns\": 50, \"max_output_size\": 1048576}");
    fclose(f1);

    ik_config_t *config1 = NULL;
    res_t result1 = ik_config_load(ctx, paths, &config1);
    ck_assert(!result1.is_err);
    ck_assert_int_eq(config1->listen_port, 1024);

    // Test maximum valid port (65535)
    FILE *f2 = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f2);
    fprintf(f2,
            "{\"openai_model\": \"gpt-4-turbo\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\", \"listen_port\": 65535, \"max_tool_turns\": 50, \"max_output_size\": 1048576}");
    fclose(f2);

    ik_config_t *config2 = NULL;
    res_t result2 = ik_config_load(ctx, paths, &config2);
    ck_assert(!result2.is_err);
    ck_assert_int_eq(config2->listen_port, 65535);

    // Test default port (1984)
    FILE *f3 = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f3);
    fprintf(f3,
            "{\"openai_model\": \"gpt-4-turbo\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984, \"max_tool_turns\": 50, \"max_output_size\": 1048576}");
    fclose(f3);

    ik_config_t *config3 = NULL;
    res_t result3 = ik_config_load(ctx, paths, &config3);
    ck_assert(!result3.is_err);
    ck_assert_int_eq(config3->listen_port, 1984);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_temperature_too_low) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_model\": \"gpt-5-mini\", \"openai_temperature\": -0.1, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    ik_config_t *config = NULL;

    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_OUT_OF_RANGE);
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_temperature_too_high) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_model\": \"gpt-5-mini\", \"openai_temperature\": 2.1, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    ik_config_t *config = NULL;

    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_OUT_OF_RANGE);
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_max_tokens_too_low) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_model\": \"gpt-5-mini\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 0, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    ik_config_t *config = NULL;

    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_OUT_OF_RANGE);
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_max_tokens_too_high) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_model\": \"gpt-5-mini\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 130000, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    ik_config_t *config = NULL;

    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_OUT_OF_RANGE);
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_valid_openai_system_message) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_model\": \"gpt-5-mini\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": \"You are a helpful assistant\", \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984, \"max_tool_turns\": 50, \"max_output_size\": 1048576}");
    fclose(f);

    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_str_eq(cfg->openai_system_message, "You are a helpful assistant");
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

static Suite *config_validation_ranges_suite(void)
{
    Suite *s = suite_create("Config Validation - Ranges");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_error_code_strings);
    tcase_add_test(tc_core, test_config_port_too_low);
    tcase_add_test(tc_core, test_config_port_too_high);
    tcase_add_test(tc_core, test_config_port_valid_range);
    tcase_add_test(tc_core, test_config_temperature_too_low);
    tcase_add_test(tc_core, test_config_temperature_too_high);
    tcase_add_test(tc_core, test_config_max_tokens_too_low);
    tcase_add_test(tc_core, test_config_max_tokens_too_high);
    tcase_add_test(tc_core, test_config_valid_openai_system_message);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = config_validation_ranges_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/config/validation_ranges_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
