#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

START_TEST(test_config_wrong_type_port) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with listen_port as string
    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_wrongtype_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_api_key\": \"test\", \"openai_model\": \"gpt-5-mini\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_address\": \"127.0.0.1\", \"listen_port\": \"1984\"}");
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
    fprintf(f,
            "{\"openai_api_key\": \"test\", \"openai_model\": \"gpt-5-mini\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": null, \"listen_address\": 12345, \"listen_port\": 1984}");
    fclose(f);

    // Try to load - should fail with PARSE error
    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    // Clean up
    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_wrong_type_openai_model)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_wrong_model_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_api_key\": \"test\", \"openai_model\": 123, \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_wrong_type_openai_temperature)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_wrong_temp_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_api_key\": \"test\", \"openai_model\": \"gpt-5-mini\", \"openai_temperature\": \"0.7\", \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_wrong_type_openai_max_completion_tokens)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_wrong_tokens_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_api_key\": \"test\", \"openai_model\": \"gpt-5-mini\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": \"4096\", \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    unlink(test_file);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_wrong_type_openai_system_message)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_file[256];
    snprintf(test_file, sizeof(test_file), "/tmp/ikigai_wrong_sysmsg_%d.json", getpid());

    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f,
            "{\"openai_api_key\": \"test\", \"openai_model\": \"gpt-5-mini\", \"openai_temperature\": 0.7, \"openai_max_completion_tokens\": 4096, \"openai_system_message\": 123, \"listen_address\": \"127.0.0.1\", \"listen_port\": 1984}");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_file);
    ck_assert(result.is_err);
    ck_assert_int_eq(result.err->code, ERR_PARSE);

    unlink(test_file);
    talloc_free(ctx);
}

END_TEST

static Suite *config_validation_types_suite(void)
{
    Suite *s = suite_create("Config Validation - Wrong Types");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_config_wrong_type_port);
    tcase_add_test(tc_core, test_config_wrong_type_api_key);
    tcase_add_test(tc_core, test_config_wrong_type_address);
    tcase_add_test(tc_core, test_config_wrong_type_openai_model);
    tcase_add_test(tc_core, test_config_wrong_type_openai_temperature);
    tcase_add_test(tc_core, test_config_wrong_type_openai_max_completion_tokens);
    tcase_add_test(tc_core, test_config_wrong_type_openai_system_message);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = config_validation_types_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
