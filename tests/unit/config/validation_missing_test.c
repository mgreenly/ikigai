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
    fprintf(f,
            "{\"openai_api_key\": \"test\", \"openai_model\": \"gpt-5-mini\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_port\": 1984}");
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
    fprintf(f,
            "{\"openai_api_key\": \"test\", \"openai_model\": \"gpt-5-mini\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\"}");
    fclose(f);

    // Try to load - should fail with PARSE error
    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    // Clean up
    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_missing_field_openai_model)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_missing_model_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"openai_api_key\": \"test\", \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_missing_field_openai_temperature)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_missing_temp_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_api_key\": \"test\", \"openai_model\": \"gpt-5-mini\", \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_missing_field_openai_max_completion_tokens)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_missing_tokens_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_api_key\": \"test\", \"openai_model\": \"gpt-5-mini\", \"openai_temperature\": 0.7, \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_missing_openai_system_message)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_no_sysmsg_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    // Completely omit openai_system_message field
    fprintf(f,
            "{\"openai_api_key\": \"test\", \"openai_model\": \"gpt-5-mini\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984, \"max_tool_turns\": 50, \"max_output_size\": 1048576}");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(!result.is_err);
    ik_cfg_t *cfg = result.ok;
    ck_assert_ptr_null(cfg->openai_system_message);

    unlink(test_file);
    talloc_free(ctx);
}

END_TEST

static Suite *config_validation_missing_suite(void)
{
    Suite *s = suite_create("Config Validation - Missing Fields");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_config_missing_field_openai_key);
    tcase_add_test(tc_core, test_config_missing_field_listen_address);
    tcase_add_test(tc_core, test_config_missing_field_listen_port);
    tcase_add_test(tc_core, test_config_missing_field_openai_model);
    tcase_add_test(tc_core, test_config_missing_field_openai_temperature);
    tcase_add_test(tc_core, test_config_missing_field_openai_max_completion_tokens);
    tcase_add_test(tc_core, test_config_missing_openai_system_message);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = config_validation_missing_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
