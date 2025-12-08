#include "../../../src/config.h"

#include "../../../src/error.h"
#include "../../test_utils.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

START_TEST(test_config_with_valid_max_tool_turns) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a test config file with valid max_tool_turns
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_max_tool_turns_test_%d.json", getpid());

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_api_key\": \"test-key\",\n"
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

    res_t result = ik_cfg_load(ctx, test_config);
    ck_assert(!result.is_err);

    ik_cfg_t *cfg = result.ok;
    ck_assert_ptr_nonnull(cfg);
    ck_assert_int_eq(cfg->max_tool_turns, 50);
    ck_assert_int_eq(cfg->max_output_size, 1048576);

    unlink(test_config);
    talloc_free(ctx);
}
END_TEST START_TEST(test_config_missing_max_tool_turns)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_missing_max_tool_turns_%d.json", getpid());

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_api_key\": \"test-key\",\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_output_size\": 1048576\n"
            "}\n");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_PARSE);

    unlink(test_config);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_missing_max_output_size)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_missing_max_output_size_%d.json", getpid());

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_api_key\": \"test-key\",\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 50\n"
            "}\n");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_PARSE);

    unlink(test_config);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_max_tool_turns_out_of_range_low)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_max_tool_turns_low_%d.json", getpid());

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_api_key\": \"test-key\",\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 0,\n"
            "  \"max_output_size\": 1048576\n"
            "}\n");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_OUT_OF_RANGE);

    unlink(test_config);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_max_tool_turns_out_of_range_high)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_max_tool_turns_high_%d.json", getpid());

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_api_key\": \"test-key\",\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 1001,\n"
            "  \"max_output_size\": 1048576\n"
            "}\n");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_OUT_OF_RANGE);

    unlink(test_config);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_max_output_size_out_of_range_low)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_max_output_size_low_%d.json", getpid());

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_api_key\": \"test-key\",\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 50,\n"
            "  \"max_output_size\": 1023\n"
            "}\n");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_OUT_OF_RANGE);

    unlink(test_config);
    talloc_free(ctx);
}

END_TEST START_TEST(test_config_max_output_size_out_of_range_high)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_max_output_size_high_%d.json", getpid());

    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_api_key\": \"test-key\",\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 50,\n"
            "  \"max_output_size\": 104857601\n"
            "}\n");
    fclose(f);

    res_t result = ik_cfg_load(ctx, test_config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_OUT_OF_RANGE);

    unlink(test_config);
    talloc_free(ctx);
}

END_TEST

static Suite *tool_limits_suite(void)
{
    Suite *s = suite_create("Config Tool Limits");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_config_with_valid_max_tool_turns);
    tcase_add_test(tc_core, test_config_missing_max_tool_turns);
    tcase_add_test(tc_core, test_config_missing_max_output_size);
    tcase_add_test(tc_core, test_config_max_tool_turns_out_of_range_low);
    tcase_add_test(tc_core, test_config_max_tool_turns_out_of_range_high);
    tcase_add_test(tc_core, test_config_max_output_size_out_of_range_low);
    tcase_add_test(tc_core, test_config_max_output_size_out_of_range_high);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = tool_limits_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
