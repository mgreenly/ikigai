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

START_TEST(test_config_with_invalid_db_port_type) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with invalid db_port type (string instead of number)
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_port as a string
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
            "  \"db_port\": \"not a number\"\n"
            "}\n");
    fclose(f);

    // Load config - should fail with invalid type error
    ik_config_t *config = NULL;
    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_PARSE);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_invalid_db_host_type) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with invalid db_host type (number instead of string)
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_host as a number
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
            "  \"db_host\": 12345\n"
            "}\n");
    fclose(f);

    // Load config - should fail with invalid type error
    ik_config_t *config = NULL;
    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_PARSE);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_invalid_db_name_type) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with invalid db_name type (boolean instead of string)
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_name as a boolean
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
            "  \"db_name\": true\n"
            "}\n");
    fclose(f);

    // Load config - should fail with invalid type error
    ik_config_t *config = NULL;
    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_PARSE);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_invalid_db_user_type) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with invalid db_user type (array instead of string)
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_user as an array
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
            "  \"db_user\": [\"user1\", \"user2\"]\n"
            "}\n");
    fclose(f);

    // Load config - should fail with invalid type error
    ik_config_t *config = NULL;
    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_PARSE);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_out_of_range_db_port_low) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with db_port too low (< 1)
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_port = 0
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
            "  \"db_port\": 0\n"
            "}\n");
    fclose(f);

    // Load config - should fail with out of range error
    ik_config_t *config = NULL;
    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_OUT_OF_RANGE);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_out_of_range_db_port_high) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with db_port too high (> 65535)
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_port = 70000
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
            "  \"db_port\": 70000\n"
            "}\n");
    fclose(f);

    // Load config - should fail with out of range error
    ik_config_t *config = NULL;
    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_OUT_OF_RANGE);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

static Suite *config_suite(void)
{
    Suite *s = suite_create("Config Validation");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_config_with_invalid_db_port_type);
    tcase_add_test(tc_core, test_config_with_invalid_db_host_type);
    tcase_add_test(tc_core, test_config_with_invalid_db_name_type);
    tcase_add_test(tc_core, test_config_with_invalid_db_user_type);
    tcase_add_test(tc_core, test_config_with_out_of_range_db_port_low);
    tcase_add_test(tc_core, test_config_with_out_of_range_db_port_high);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = config_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/config/config_validation_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
