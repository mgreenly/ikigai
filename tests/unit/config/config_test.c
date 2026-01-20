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

// Mock control: return NULL on the Nth call to yyjson_get_str_
static int32_t g_call_counter = 0;
static int32_t g_return_null_on_call = -1;

// Override weak symbol from wrapper.h
const char *yyjson_get_str_(yyjson_val *val)
{
    int32_t current_call = g_call_counter++;
    if (current_call == g_return_null_on_call) {
        return NULL;
    }
    return yyjson_get_str(val);
}

START_TEST(test_config_with_db_connection_string) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with db_connection_string
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_connection_string
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
            "  \"db_connection_string\": \"postgresql://localhost/ikigai\"\n"
            "}\n");
    fclose(f);

    // Load config
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    ck_assert_ptr_nonnull(cfg->db_connection_string);
    ck_assert_str_eq(cfg->db_connection_string, "postgresql://localhost/ikigai");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

START_TEST(test_config_without_db_connection_string) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file without db_connection_string
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config without db_connection_string
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
            "  \"max_output_size\": 1048576\n"
            "}\n");
    fclose(f);

    // Load config - should succeed with NULL db_connection_string
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    ck_assert_ptr_null(cfg->db_connection_string);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_full_connection_string) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with full connection string
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with full connection string
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
            "  \"db_connection_string\": \"postgresql://user:pass@localhost:5432/ikigai\"\n"
            "}\n");
    fclose(f);

    // Load config
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    ck_assert_ptr_nonnull(cfg->db_connection_string);
    ck_assert_str_eq(cfg->db_connection_string, "postgresql://user:pass@localhost:5432/ikigai");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_unix_socket_connection_string) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with Unix socket connection string
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with Unix socket connection string
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
            "  \"db_connection_string\": \"postgresql:///ikigai?host=/var/run/postgresql\"\n"
            "}\n");
    fclose(f);

    // Load config
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    ck_assert_ptr_nonnull(cfg->db_connection_string);
    ck_assert_str_eq(cfg->db_connection_string, "postgresql:///ikigai?host=/var/run/postgresql");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_empty_db_connection_string) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with empty db_connection_string
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with empty db_connection_string
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
            "  \"db_connection_string\": \"\"\n"
            "}\n");
    fclose(f);

    // Load config - should succeed with NULL db_connection_string
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    ck_assert_ptr_null(cfg->db_connection_string);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_invalid_db_connection_string_type) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with invalid db_connection_string type (number instead of string)
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_connection_string as a number
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
            "  \"db_connection_string\": 12345\n"
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

START_TEST(test_config_with_explicit_null_db_connection_string) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with explicit null db_connection_string
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with explicit null db_connection_string
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
            "  \"db_connection_string\": null\n"
            "}\n");
    fclose(f);

    // Load config - should succeed with NULL db_connection_string
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    ck_assert_ptr_null(cfg->db_connection_string);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_structure_has_db_connection_string_field) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Test that we can directly access the db_connection_string field
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Field should be NULL by default
    ck_assert_ptr_null(cfg->db_connection_string);

    // We can assign to it
    cfg->db_connection_string = talloc_strdup(cfg, "postgresql://test/db");
    ck_assert_ptr_nonnull(cfg->db_connection_string);
    ck_assert_str_eq(cfg->db_connection_string, "postgresql://test/db");

    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_db_connection_string_null_value) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with db_connection_string
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with db_connection_string
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
            "  \"db_connection_string\": \"postgresql://localhost/ikigai\"\n"
            "}\n");
    fclose(f);

    // Reset mock counter and set to return NULL on the call for db_connection_string
    // Calls are: model, address, db_connection_string
    // (system_message is null so yyjson_get_str_ is not called for it)
    // So db_connection_string is the 3rd call (index 2)
    g_call_counter = 0;
    g_return_null_on_call = 2;

    // Load config - should succeed with NULL db_connection_string due to mock
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    // Even though JSON has a value, mock returns NULL, so config should be NULL
    ck_assert_ptr_null(cfg->db_connection_string);

    // Reset mock
    g_return_null_on_call = -1;

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

static Suite *config_suite(void)
{
    Suite *s = suite_create("Config Database");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_config_with_db_connection_string);
    tcase_add_test(tc_core, test_config_without_db_connection_string);
    tcase_add_test(tc_core, test_config_with_full_connection_string);
    tcase_add_test(tc_core, test_config_with_unix_socket_connection_string);
    tcase_add_test(tc_core, test_config_with_empty_db_connection_string);
    tcase_add_test(tc_core, test_config_with_db_connection_string_null_value);
    tcase_add_test(tc_core, test_config_with_invalid_db_connection_string_type);
    tcase_add_test(tc_core, test_config_with_explicit_null_db_connection_string);
    tcase_add_test(tc_core, test_config_structure_has_db_connection_string_field);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = config_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
