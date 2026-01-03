#include "../../../src/config.h"

#include "../../../src/error.h"
#include "../../test_utils.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

START_TEST(test_config_history_size_default) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config without history_size field
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_history_default_%d.json", getpid());

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

    // Load config - should succeed with default history_size
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, test_config, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    // Default history_size should be 10000
    ck_assert_int_eq(cfg->history_size, 10000);

    // Clean up
    unlink(test_config);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_config_history_size_custom) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with custom history_size
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_history_custom_%d.json", getpid());

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
            "  \"history_size\": 5000\n"
            "}\n");
    fclose(f);

    // Load config - should succeed with custom history_size
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, test_config, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    // Custom history_size should be 5000
    ck_assert_int_eq(cfg->history_size, 5000);

    // Clean up
    unlink(test_config);
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_history_size_zero) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with zero history_size (should fail)
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_history_zero_%d.json", getpid());

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
            "  \"history_size\": 0\n"
            "}\n");
    fclose(f);

    // Load config - should fail with OUT_OF_RANGE error
    ik_config_t *config = NULL;

    res_t result = ik_config_load(ctx, test_config, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_OUT_OF_RANGE);

    // Clean up
    unlink(test_config);
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_history_size_negative) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with negative history_size (should fail)
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_history_neg_%d.json", getpid());

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
            "  \"history_size\": -100\n"
            "}\n");
    fclose(f);

    // Load config - should fail with OUT_OF_RANGE error
    ik_config_t *config = NULL;

    res_t result = ik_config_load(ctx, test_config, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_OUT_OF_RANGE);

    // Clean up
    unlink(test_config);
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_history_size_large_value) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with large history_size (should succeed)
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_history_large_%d.json", getpid());

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
            "  \"history_size\": 1000000\n"
            "}\n");
    fclose(f);

    // Load config - should succeed with large history_size
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, test_config, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    ck_assert_int_eq(cfg->history_size, 1000000);

    // Clean up
    unlink(test_config);
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_history_size_exceeds_int32) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with history_size exceeding INT32_MAX
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_history_exceed_%d.json", getpid());

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
            "  \"history_size\": 2147483648\n"
            "}\n");
    fclose(f);

    // Load config - should fail with OUT_OF_RANGE error
    ik_config_t *config = NULL;

    res_t result = ik_config_load(ctx, test_config, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_OUT_OF_RANGE);

    // Clean up
    unlink(test_config);
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_history_size_invalid_type) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a config with invalid history_size type (string instead of int)
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_history_type_%d.json", getpid());

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
            "  \"history_size\": \"5000\"\n"
            "}\n");
    fclose(f);

    // Load config - should fail with PARSE error
    ik_config_t *config = NULL;

    res_t result = ik_config_load(ctx, test_config, &config);
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_PARSE);

    // Clean up
    unlink(test_config);
    talloc_free(ctx);
}

END_TEST

static Suite *config_history_size_suite(void)
{
    Suite *s = suite_create("Config History Size");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_config_history_size_default);
    tcase_add_test(tc_core, test_config_history_size_custom);
    tcase_add_test(tc_core, test_config_history_size_zero);
    tcase_add_test(tc_core, test_config_history_size_negative);
    tcase_add_test(tc_core, test_config_history_size_large_value);
    tcase_add_test(tc_core, test_config_history_size_exceeds_int32);
    tcase_add_test(tc_core, test_config_history_size_invalid_type);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = config_history_size_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
