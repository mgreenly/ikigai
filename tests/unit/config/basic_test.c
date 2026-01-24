#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../../src/paths.h"
#include "../../test_utils_helper.h"

START_TEST(test_config_types_exist) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // This test verifies that the config types compile
    // We'll test actual functionality in later tests
    ik_config_t *cfg = talloc(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    cfg->listen_address = talloc_strdup(ctx, "127.0.0.1");
    cfg->listen_port = 1984;

    ck_assert_str_eq(cfg->listen_address, "127.0.0.1");
    ck_assert_int_eq(cfg->listen_port, 1984);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_load_function_exists) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // This test verifies that ik_config_load function exists and can be called
    ik_config_t *config = NULL;
    res_t result = ik_config_load(ctx, paths, &config);

    // For now, just verify the function exists and returns a result
    // Later tests will verify actual behavior
    (void)result;

    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_load_without_directory) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment (creates test directory structure)
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Get config directory from paths
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *config_path = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Delete config dir to ensure no config file exists
    unlink(config_path);
    rmdir(config_dir);

    // Call ik_config_load - should succeed with defaults, NOT create files
    ik_config_t *config = NULL;
    res_t result = ik_config_load(ctx, paths, &config);

    // Should succeed with defaults
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(config);

    // Verify NO directory was created
    struct stat st;
    ck_assert_int_eq(stat(config_dir, &st), -1);

    // Verify NO config file was created
    ck_assert_int_eq(stat(config_path, &st), -1);

    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_load_without_file) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment (creates directory structure)
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Get config directory from paths
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *config_path = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Delete config file but leave directory
    unlink(config_path);

    // Verify directory exists
    struct stat st;
    ck_assert_int_eq(stat(config_dir, &st), 0);
    ck_assert(S_ISDIR(st.st_mode));

    // Call ik_config_load - should succeed with defaults, NOT create file
    ik_config_t *config = NULL;
    res_t result = ik_config_load(ctx, paths, &config);

    // Should succeed with defaults
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(config);

    // Verify NO config file was created
    ck_assert_int_eq(stat(config_path, &st), -1);

    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_auto_create_defaults) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Call ik_config_load - should create with defaults
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);

    // Verify default values
    ck_assert_str_eq(cfg->openai_model, "gpt-5-mini");
    ck_assert(cfg->openai_temperature >= 0.99 && cfg->openai_temperature <= 1.01);
    ck_assert_int_eq(cfg->openai_max_completion_tokens, 4096);
    ck_assert_str_eq(cfg->openai_system_message, "You are a personal agent and are operating inside the Ikigai orchestration platform.");
    ck_assert_str_eq(cfg->listen_address, "127.0.0.1");
    ck_assert_int_eq(cfg->listen_port, 1984);

    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_load_invalid_json) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Get config path and write invalid JSON
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *config_path = talloc_asprintf(ctx, "%s/config.json", config_dir);

    FILE *f = fopen(config_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{this is not valid JSON}");
    fclose(f);

    // Try to load - should fail with PARSE error
    ik_config_t *config = NULL;
    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_memory_cleanup) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Get config path and write valid JSON
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *config_path = talloc_asprintf(ctx, "%s/config.json", config_dir);

    FILE *f = fopen(config_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_model\": \"gpt-4-turbo\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\", \"listen_port\": 8080, \"max_tool_turns\": 50, \"max_output_size\": 1048576}");
    fclose(f);

    // Load config
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);

    // Verify all strings are on the config context (child of ctx)
    ck_assert_ptr_nonnull(cfg->listen_address);
    ck_assert_str_eq(cfg->listen_address, "127.0.0.1");
    ck_assert_int_eq(cfg->listen_port, 8080);

    // Verify memory is properly parented
    // talloc_get_type will return NULL if the pointer is not valid
    ck_assert_ptr_eq(talloc_parent(cfg), ctx);

    // Clean up - freeing ctx should free all child allocations
    test_paths_cleanup_env();
    talloc_free(ctx);

    // Note: We can't access cfg after talloc_free(ctx) as it's been freed
    // This test verifies no crashes occur during cleanup
}

END_TEST static Suite *config_basic_suite(void)
{
    Suite *s = suite_create("Config Basic");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_config_types_exist);
    tcase_add_test(tc_core, test_config_load_function_exists);
    tcase_add_test(tc_core, test_config_load_without_directory);
    tcase_add_test(tc_core, test_config_load_without_file);
    tcase_add_test(tc_core, test_config_auto_create_defaults);
    tcase_add_test(tc_core, test_config_load_invalid_json);
    tcase_add_test(tc_core, test_config_memory_cleanup);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = config_basic_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/config/basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
