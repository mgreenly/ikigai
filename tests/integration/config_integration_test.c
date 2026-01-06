#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../src/config.h"
#include "../../src/error.h"
#include "../../src/paths.h"
#include "../../src/wrapper.h"
#include "../test_utils.h"

START_TEST(test_config_full_flow) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Get config path from paths
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // First call: config doesn't exist, should create defaults
    ik_config_t *cfg1 = NULL;
    res_t result1 = ik_config_load(ctx, paths, &cfg1);
    ck_assert(!result1.is_err);
    ck_assert_ptr_nonnull(cfg1);
    ck_assert_str_eq(cfg1->openai_model, "gpt-5-mini");
    ck_assert(cfg1->openai_temperature >= 0.99 && cfg1->openai_temperature <= 1.01);
    ck_assert_int_eq(cfg1->openai_max_completion_tokens, 4096);
    ck_assert_ptr_null(cfg1->openai_system_message);
    ck_assert_str_eq(cfg1->listen_address, "127.0.0.1");
    ck_assert_int_eq(cfg1->listen_port, 1984);

    // Verify file was created
    struct stat st;
    ck_assert_int_eq(stat(test_config, &st), 0);
    ck_assert(S_ISREG(st.st_mode));

    // Second call: config exists, should load the same defaults
    ik_config_t *cfg2 = NULL;
    res_t result2 = ik_config_load(ctx, paths, &cfg2);
    ck_assert(!result2.is_err);
    ck_assert_ptr_nonnull(cfg2);
    ck_assert_str_eq(cfg2->openai_model, "gpt-5-mini");
    ck_assert(cfg2->openai_temperature >= 0.99 && cfg2->openai_temperature <= 1.01);
    ck_assert_int_eq(cfg2->openai_max_completion_tokens, 4096);
    ck_assert_ptr_null(cfg2->openai_system_message);
    ck_assert_str_eq(cfg2->listen_address, "127.0.0.1");
    ck_assert_int_eq(cfg2->listen_port, 1984);

    // Modify the file with custom values
    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n");
    fprintf(f, "  \"openai_model\": \"gpt-3.5-turbo\",\n");
    fprintf(f, "  \"openai_temperature\": 1.5,\n");
    fprintf(f, "  \"openai_max_completion_tokens\": 2048,\n");
    fprintf(f, "  \"openai_system_message\": \"You are a helpful assistant\",\n");
    fprintf(f, "  \"listen_address\": \"0.0.0.0\",\n");
    fprintf(f, "  \"listen_port\": 3000,\n");
    fprintf(f, "  \"max_tool_turns\": 50,\n");
    fprintf(f, "  \"max_output_size\": 1048576\n");
    fprintf(f, "}\n");
    fclose(f);

    // Third call: should load modified values
    ik_config_t *cfg3 = NULL;
    res_t result3 = ik_config_load(ctx, paths, &cfg3);
    ck_assert(!result3.is_err);
    ck_assert_ptr_nonnull(cfg3);
    ck_assert_str_eq(cfg3->openai_model, "gpt-3.5-turbo");
    ck_assert(cfg3->openai_temperature >= 1.49 && cfg3->openai_temperature <= 1.51);
    ck_assert_int_eq(cfg3->openai_max_completion_tokens, 2048);
    ck_assert_str_eq(cfg3->openai_system_message, "You are a helpful assistant");
    ck_assert_str_eq(cfg3->listen_address, "0.0.0.0");
    ck_assert_int_eq(cfg3->listen_port, 3000);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

// Mock for yyjson_mut_write_file failure
static bool mock_write_failure = false;
bool yyjson_mut_write_file_(const char *path, const yyjson_mut_doc *doc,
                            yyjson_write_flag flg, const yyjson_alc *alc,
                            yyjson_write_err *err)
{
    if (mock_write_failure) {
        if (err) {
            err->msg = "Mock write error";
            err->code = YYJSON_WRITE_ERROR_FILE_OPEN;
        }
        return false;
    }
    return yyjson_mut_write_file(path, doc, flg, alc, err);
}

START_TEST(test_config_write_failure) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Enable mock write failure
    mock_write_failure = true;

    // Try to load config - should fail to create default config
    ik_config_t *config = NULL;

    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_IO);

    // Disable mock write failure
    mock_write_failure = false;

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

// Mock for yyjson_read_file failure
static bool mock_read_failure = false;
yyjson_doc *yyjson_read_file_(const char *path, yyjson_read_flag flg,
                              const yyjson_alc *alc, yyjson_read_err *err)
{
    if (mock_read_failure) {
        if (err) {
            err->msg = "Mock read error";
            err->code = YYJSON_READ_ERROR_FILE_OPEN;
        }
        return NULL;
    }
    return yyjson_read_file(path, flg, alc, err);
}

START_TEST(test_config_read_failure) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // First create a valid config file
    ik_config_t *config1 = NULL;
    res_t result1 = ik_config_load(ctx, paths, &config1);
    ck_assert(!result1.is_err);

    // Now enable mock read failure
    mock_read_failure = true;

    // Try to load config - should fail to read
    ik_config_t *config2 = NULL;
    res_t result2 = ik_config_load(ctx, paths, &config2);
    ck_assert(result2.is_err);
    ck_assert_int_eq(result2.err->code, ERR_PARSE);

    // Disable mock read failure
    mock_read_failure = false;

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

START_TEST(test_config_invalid_json_root) {

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

    // Create a JSON file where root is an array, not an object
    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "[\n");
    fprintf(f, "  \"item1\",\n");
    fprintf(f, "  \"item2\"\n");
    fprintf(f, "]\n");
    fclose(f);

    // Try to load config - should fail because root is not an object
    ik_config_t *config = NULL;

    res_t result = ik_config_load(ctx, paths, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

// Mock for yyjson_doc_get_root returning NULL
static bool mock_doc_get_root_null = false;
yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc)
{
    if (mock_doc_get_root_null) {
        return NULL;
    }
    return yyjson_doc_get_root(doc);
}

START_TEST(test_config_doc_get_root_null) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // First create a valid config file
    ik_config_t *config1 = NULL;
    res_t result1 = ik_config_load(ctx, paths, &config1);
    ck_assert(!result1.is_err);

    // Now enable mock to return NULL from doc_get_root
    mock_doc_get_root_null = true;

    // Try to load config - should fail because root is NULL
    ik_config_t *config2 = NULL;
    res_t result2 = ik_config_load(ctx, paths, &config2);
    ck_assert(result2.is_err);
    ck_assert_int_eq(result2.err->code, ERR_PARSE);

    // Disable mock
    mock_doc_get_root_null = false;

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

static Suite *config_integration_suite(void)
{
    Suite *s = suite_create("ConfigIntegration");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_config_full_flow);
    tcase_add_test(tc_core, test_config_write_failure);
    tcase_add_test(tc_core, test_config_read_failure);
    tcase_add_test(tc_core, test_config_invalid_json_root);
    tcase_add_test(tc_core, test_config_doc_get_root_null);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = config_integration_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
