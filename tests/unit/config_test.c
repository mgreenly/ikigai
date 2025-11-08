#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "../../src/config.h"
#include "../../src/error.h"
#include "../test_utils.h"

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

END_TEST START_TEST(test_config_expand_tilde)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *home = getenv("HOME");
    ck_assert_ptr_nonnull(home);

    // Test path with tilde
    res_t res = expand_tilde(ctx, "~/test/path");
    ck_assert(is_ok(&res));
    char *expanded = res.ok;
    ck_assert_ptr_nonnull(expanded);

    char expected[256];
    snprintf(expected, sizeof(expected), "%s/test/path", home);
    ck_assert_str_eq(expanded, expected);

    // Test path without tilde (should return unchanged)
    res = expand_tilde(ctx, "/absolute/path");
    ck_assert(is_ok(&res));
    char *no_tilde = res.ok;
    ck_assert_ptr_nonnull(no_tilde);
    ck_assert_str_eq(no_tilde, "/absolute/path");

    talloc_free(ctx);
}

END_TEST START_TEST(test_config_expand_tilde_home_unset)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Save original HOME
    const char *original_home = getenv("HOME");
    char *saved_home = NULL;
    if (original_home) {
        saved_home = strdup(original_home);
    }

    // Unset HOME
    unsetenv("HOME");

    // Should return error when HOME is not set
    res_t result = expand_tilde(ctx, "~/test");
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);

    // Restore HOME
    if (saved_home) {
        setenv("HOME", saved_home, 1);
        free(saved_home);
    }

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

END_TEST START_TEST(test_config_missing_field_openai_key)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config missing openai_api_key
    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_missing_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    // Try to load - should fail with PARSE error
    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    // Clean up
    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_missing_field_listen_address)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config missing listen_address
    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_missing2_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"openai_api_key\": \"test\", \"listen_port\": 1984}");
    fclose(f);

    // Try to load - should fail with PARSE error
    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    // Clean up
    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_missing_field_listen_port)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config missing listen_port
    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_missing3_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"openai_api_key\": \"test\", \"listen_address\": \"127.0.0.1\"}");
    fclose(f);

    // Try to load - should fail with PARSE error
    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    // Clean up
    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_wrong_type_port)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with listen_port as string
    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_wrongtype_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"openai_api_key\": \"test\", \"listen_address\": \"127.0.0.1\", \"listen_port\": \"1984\"}");
    fclose(f);

    // Try to load - should fail with PARSE error
    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    // Clean up
    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_wrong_type_api_key)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with openai_api_key as number instead of string
    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_wrongtype_apikey_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"openai_api_key\": 12345, \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    // Try to load - should fail with PARSE error
    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    // Clean up
    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_wrong_type_address)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with listen_address as number instead of string
    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_wrongtype_address_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"openai_api_key\": \"test\", \"listen_address\": 12345, \"listen_port\": 1984}");
    fclose(f);

    // Try to load - should fail with PARSE error
    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    // Clean up
    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_load_tilde_home_unset)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Save original HOME
    const char *original_home = getenv("HOME");
    char *saved_home = NULL;
    if (original_home) {
        saved_home = strdup(original_home);
    }

    // Unset HOME
    unsetenv("HOME");

    // Try to load with tilde path - should fail
    res_t result = ik_cfg_load(ctx, "~/test/config.json");
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);

    // Restore HOME
    if (saved_home) {
        setenv("HOME", saved_home, 1);
        free(saved_home);
    }

    talloc_free(ctx);
}

END_TEST START_TEST(test_error_code_strings)
{
    // Test that new error codes have string representations
    const char *io_str = error_code_str(ERR_IO);
    ck_assert_str_eq(io_str, "IO error");

    const char *parse_str = error_code_str(ERR_PARSE);
    ck_assert_str_eq(parse_str, "Parse error");
}

END_TEST START_TEST(test_config_port_too_low)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with port below 1024
    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_port_low_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"openai_api_key\": \"test\", \"listen_address\": \"127.0.0.1\", \"listen_port\": 80}");
    fclose(f);

    // Try to load - should fail with OUT_OF_RANGE error
    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_OUT_OF_RANGE);

    // Clean up
    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_port_too_high)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with port above 65535
    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_port_high_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"openai_api_key\": \"test\", \"listen_address\": \"127.0.0.1\", \"listen_port\": 70000}");
    fclose(f);

    // Try to load - should fail with OUT_OF_RANGE error
    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_OUT_OF_RANGE);

    // Clean up
    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_port_valid_range)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Test minimum valid port (1024)
    char test_file1[256];
    snprintf(test_file1, sizeof(test_file1), "/tmp/ikigai_port_min_%d.json", getpid());

    FILE *f1 = fopen(test_file1, "w");
    ck_assert_ptr_nonnull(f1);
    fprintf(f1, "{\"openai_api_key\": \"test\", \"listen_address\": \"127.0.0.1\", \"listen_port\": 1024}");
    fclose(f1);

    res_t result1 = ik_cfg_load(ctx, test_file1);
    ck_assert(!result1.is_err);
    ck_assert_int_eq(((ik_cfg_t *)result1.ok)->listen_port, 1024);

    // Test maximum valid port (65535)
    char test_file2[256];
    snprintf(test_file2, sizeof(test_file2), "/tmp/ikigai_port_max_%d.json", getpid());

    FILE *f2 = fopen(test_file2, "w");
    ck_assert_ptr_nonnull(f2);
    fprintf(f2, "{\"openai_api_key\": \"test\", \"listen_address\": \"127.0.0.1\", \"listen_port\": 65535}");
    fclose(f2);

    res_t result2 = ik_cfg_load(ctx, test_file2);
    ck_assert(!result2.is_err);
    ck_assert_int_eq(((ik_cfg_t *)result2.ok)->listen_port, 65535);

    // Test default port (1984)
    char test_file3[256];
    snprintf(test_file3, sizeof(test_file3), "/tmp/ikigai_port_def_%d.json", getpid());

    FILE *f3 = fopen(test_file3, "w");
    ck_assert_ptr_nonnull(f3);
    fprintf(f3, "{\"openai_api_key\": \"test\", \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f3);

    res_t result3 = ik_cfg_load(ctx, test_file3);
    ck_assert(!result3.is_err);
    ck_assert_int_eq(((ik_cfg_t *)result3.ok)->listen_port, 1984);

    // Clean up
    unlink(test_file1);
    unlink(test_file2);
    unlink(test_file3);
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
    fprintf(f, "{\"openai_api_key\": \"test_key\", \"listen_address\": \"127.0.0.1\", \"listen_port\": 8080}");
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

END_TEST static Suite *config_suite(void)
{
    Suite *s = suite_create("Config");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_config_types_exist);
    tcase_add_test(tc_core, test_config_load_function_exists);
    tcase_add_test(tc_core, test_config_expand_tilde);
    tcase_add_test(tc_core, test_config_expand_tilde_home_unset);
    tcase_add_test(tc_core, test_config_auto_create_directory);
    tcase_add_test(tc_core, test_config_auto_create_with_existing_directory);
    tcase_add_test(tc_core, test_config_auto_create_defaults);
    tcase_add_test(tc_core, test_config_load_invalid_json);
    tcase_add_test(tc_core, test_config_missing_field_openai_key);
    tcase_add_test(tc_core, test_config_missing_field_listen_address);
    tcase_add_test(tc_core, test_config_missing_field_listen_port);
    tcase_add_test(tc_core, test_config_wrong_type_port);
    tcase_add_test(tc_core, test_config_wrong_type_api_key);
    tcase_add_test(tc_core, test_config_wrong_type_address);
    tcase_add_test(tc_core, test_config_load_tilde_home_unset);
    tcase_add_test(tc_core, test_error_code_strings);
    tcase_add_test(tc_core, test_config_port_too_low);
    tcase_add_test(tc_core, test_config_port_too_high);
    tcase_add_test(tc_core, test_config_port_valid_range);
    tcase_add_test(tc_core, test_config_memory_cleanup);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = config_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
