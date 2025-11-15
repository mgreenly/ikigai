#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

START_TEST(test_config_missing_field_openai_key) {
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

END_TEST static Suite *config_validation_suite(void)
{
    Suite *s = suite_create("Config Validation");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_config_missing_field_openai_key);
    tcase_add_test(tc_core, test_config_missing_field_listen_address);
    tcase_add_test(tc_core, test_config_missing_field_listen_port);
    tcase_add_test(tc_core, test_config_wrong_type_port);
    tcase_add_test(tc_core, test_config_wrong_type_api_key);
    tcase_add_test(tc_core, test_config_wrong_type_address);
    tcase_add_test(tc_core, test_error_code_strings);
    tcase_add_test(tc_core, test_config_port_too_low);
    tcase_add_test(tc_core, test_config_port_too_high);
    tcase_add_test(tc_core, test_config_port_valid_range);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = config_validation_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
