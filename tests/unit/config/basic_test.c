#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

START_TEST(test_config_types_exist) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // This test verifies that the config types compile
    // We'll test actual functionality in later tests
    ik_cfg_t *cfg = talloc(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    cfg->openai_api_key = talloc_strdup(ctx, "test_key");
    cfg->listen_address = talloc_strdup(ctx, "127.0.0.1");
    cfg->listen_port = 1984;

    ck_assert_str_eq(cfg->openai_api_key, "test_key");
    ck_assert_str_eq(cfg->listen_address, "127.0.0.1");
    ck_assert_int_eq(cfg->listen_port, 1984);

    talloc_free(ctx);
}

END_TEST START_TEST(test_config_load_function_exists)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // This test verifies that ik_cfg_load function exists and can be called
    // We expect it to fail since we haven't implemented it yet
    res_t result = ik_cfg_load(ctx, "/tmp/nonexistent_test_config.json");

    // For now, just verify the function exists and returns a result
    // Later tests will verify actual behavior
    (void)result;

    talloc_free(ctx);
}

END_TEST START_TEST(test_config_auto_create_directory)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Use a test directory that we can safely create/delete
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_test_%d", getpid());
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "%s/config.json", test_dir);

    // Clean up if it exists from a previous run
    unlink(test_config);
    rmdir(test_dir);

    // Call ik_cfg_load - should create directory and file
    res_t result = ik_cfg_load(ctx, test_config);

    // Should succeed
    ck_assert(!result.is_err);

    // Verify directory was created
    struct stat st;
    ck_assert_int_eq(stat(test_dir, &st), 0);
    ck_assert(S_ISDIR(st.st_mode));
    ck_assert_int_eq(st.st_mode & 0777, 0755);

    // Verify config file was created
    ck_assert_int_eq(stat(test_config, &st), 0);
    ck_assert(S_ISREG(st.st_mode));

    // Clean up
    unlink(test_config);
    rmdir(test_dir);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_auto_create_with_existing_directory)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Use a test directory
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_existing_%d", getpid());
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "%s/config.json", test_dir);

    // Clean up if it exists from a previous run
    unlink(test_config);
    rmdir(test_dir);

    // Pre-create the directory
    mkdir(test_dir, 0755);

    // Verify directory exists
    struct stat st;
    ck_assert_int_eq(stat(test_dir, &st), 0);
    ck_assert(S_ISDIR(st.st_mode));

    // Call ik_cfg_load - should create config in existing directory
    res_t result = ik_cfg_load(ctx, test_config);

    // Should succeed
    ck_assert(!result.is_err);

    // Verify config file was created
    ck_assert_int_eq(stat(test_config, &st), 0);
    ck_assert(S_ISREG(st.st_mode));

    // Clean up
    unlink(test_config);
    rmdir(test_dir);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_auto_create_defaults)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Use a test directory
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_test_%d", getpid());
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "%s/config.json", test_dir);

    // Clean up if it exists
    unlink(test_config);
    rmdir(test_dir);

    // Call ik_cfg_load - should create with defaults
    res_t result = ik_cfg_load(ctx, test_config);
    ck_assert(!result.is_err);

    // Load the config
    ik_cfg_t *cfg = result.ok;
    ck_assert_ptr_nonnull(cfg);

    // Verify default values
    ck_assert_str_eq(cfg->openai_api_key, "YOUR_API_KEY_HERE");
    ck_assert_str_eq(cfg->openai_model, "gpt-5-mini");
    ck_assert(cfg->openai_temperature >= 0.99 && cfg->openai_temperature <= 1.01);
    ck_assert_int_eq(cfg->openai_max_completion_tokens, 4096);
    ck_assert_ptr_null(cfg->openai_system_message);
    ck_assert_str_eq(cfg->listen_address, "127.0.0.1");
    ck_assert_int_eq(cfg->listen_port, 1984);

    // Clean up
    unlink(test_config);
    rmdir(test_dir);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_load_invalid_json)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a file with invalid JSON
    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_invalid_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{this is not valid JSON}");
    fclose(f);

    // Try to load - should fail with PARSE error
    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    // Clean up
    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_memory_cleanup)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config file
    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_memory_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_api_key\": \"test_key\", \"openai_model\": \"gpt-4-turbo\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\", \"listen_port\": 8080}");
    fclose(f);

    // Load config
    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(!result.is_err);

    ik_cfg_t *cfg = result.ok;
    ck_assert_ptr_nonnull(cfg);

    // Verify all strings are on the config context (child of ctx)
    ck_assert_ptr_nonnull(cfg->openai_api_key);
    ck_assert_ptr_nonnull(cfg->listen_address);
    ck_assert_str_eq(cfg->openai_api_key, "test_key");
    ck_assert_str_eq(cfg->listen_address, "127.0.0.1");
    ck_assert_int_eq(cfg->listen_port, 8080);

    // Verify memory is properly parented
    // talloc_get_type will return NULL if the pointer is not valid
    ck_assert_ptr_eq(talloc_parent(cfg), ctx);

    // Clean up - freeing ctx should free all child allocations
    unlink(test_file);
    talloc_free(ctx);

    // Note: We can't access cfg after talloc_free(ctx) as it's been freed
    // This test verifies no crashes occur during cleanup
}

END_TEST static Suite *config_basic_suite(void)
{
    Suite *s = suite_create("Config Basic");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_config_types_exist);
    tcase_add_test(tc_core, test_config_load_function_exists);
    tcase_add_test(tc_core, test_config_auto_create_directory);
    tcase_add_test(tc_core, test_config_auto_create_with_existing_directory);
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

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
