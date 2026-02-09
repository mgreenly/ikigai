/**
 * @file template_test.c
 * @brief Unit tests for template processor
 */

#include "apps/ikigai/template.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/config.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

static Suite *template_suite(void);

static void *ctx;
static ik_agent_ctx_t *agent;
static ik_config_t *config;

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->uuid = talloc_strdup(agent, "test-uuid-1234");
    agent->name = talloc_strdup(agent, "TestAgent");
    agent->parent_uuid = talloc_strdup(agent, "parent-uuid-5678");
    agent->provider = talloc_strdup(agent, "anthropic");
    agent->model = talloc_strdup(agent, "claude-sonnet-4-5");
    agent->created_at = 1704067200; // 2024-01-01 00:00:00 UTC

    config = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(config);
    config->openai_model = talloc_strdup(config, "gpt-4");
    config->db_host = talloc_strdup(config, "localhost");
    config->db_port = 5432;
    config->db_name = talloc_strdup(config, "ikigai_test");
    config->db_user = talloc_strdup(config, "testuser");
    config->default_provider = talloc_strdup(config, "openai");
    config->max_tool_turns = 10;
    config->max_output_size = 8192;
    config->history_size = 50;
    config->listen_address = talloc_strdup(config, "127.0.0.1");
    config->listen_port = 8080;
    config->openai_temperature = 0.7;
    config->openai_max_completion_tokens = 4096;
    config->openai_system_message = talloc_strdup(config, "You are a helpful assistant");
}

static void teardown(void)
{
    talloc_free(ctx);
}

START_TEST(test_no_variables) {
    const char *input = "Plain text without variables";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, input);
    ck_assert_int_eq((int32_t)result->unresolved_count, 0);
}
END_TEST


START_TEST(test_config_db_host) {
    const char *input = "Database: ${config.db_host}:${config.db_port}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Database: localhost:5432");
    ck_assert_int_eq((int32_t)result->unresolved_count, 0);
}
END_TEST

START_TEST(test_env_home) {
    const char *input = "Home: ${env.HOME}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);

    const char *expected_home = getenv("HOME");
    if (expected_home != NULL) {
        char *expected = talloc_asprintf(ctx, "Home: %s", expected_home);
        ck_assert_str_eq(result->processed, expected);
        ck_assert_int_eq((int32_t)result->unresolved_count, 0);
    }
}
END_TEST

START_TEST(test_escape_double_dollar) {
    const char *input = "Escaped: $${not.a.variable}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Escaped: ${not.a.variable}");
    ck_assert_int_eq((int32_t)result->unresolved_count, 0);
}
END_TEST

START_TEST(test_unresolved_variable) {
    const char *input = "Bad: ${agent.uuuid}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Bad: ${agent.uuuid}");
    ck_assert_int_eq((int32_t)result->unresolved_count, 1);
    ck_assert_str_eq(result->unresolved[0], "${agent.uuuid}");
}
END_TEST

START_TEST(test_multiple_unresolved) {
    const char *input = "${agent.uuuid} and ${config.foobar}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "${agent.uuuid} and ${config.foobar}");
    ck_assert_int_eq((int32_t)result->unresolved_count, 2);
}
END_TEST

START_TEST(test_func_cwd) {
    const char *input = "CWD: ${func.cwd}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result->processed, "CWD: ") == result->processed);
    ck_assert_int_eq((int32_t)result->unresolved_count, 0);
}
END_TEST

START_TEST(test_func_hostname) {
    const char *input = "Host: ${func.hostname}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result->processed, "Host: ") == result->processed);
    ck_assert_int_eq((int32_t)result->unresolved_count, 0);
}
END_TEST

START_TEST(test_agent_all_fields) {
    const char *input = "${agent.uuid}:${agent.name}:${agent.parent_uuid}:"
                       "${agent.provider}:${agent.model}:${agent.created_at}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "test-uuid-1234:TestAgent:parent-uuid-5678:"
                                        "anthropic:claude-sonnet-4-5:1704067200");
    ck_assert_int_eq((int32_t)result->unresolved_count, 0);
}
END_TEST

START_TEST(test_config_all_fields) {
    const char *input = "${config.openai_model}:${config.db_name}:${config.db_user}:"
                       "${config.default_provider}:${config.max_tool_turns}:"
                       "${config.max_output_size}:${config.history_size}:"
                       "${config.listen_address}:${config.listen_port}:"
                       "${config.openai_temperature}:${config.openai_max_completion_tokens}:"
                       "${config.openai_system_message}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result->processed, "gpt-4") != NULL);
    ck_assert(strstr(result->processed, "ikigai_test") != NULL);
    ck_assert(strstr(result->processed, "testuser") != NULL);
    ck_assert(strstr(result->processed, "openai") != NULL);
    ck_assert(strstr(result->processed, "10") != NULL);
    ck_assert(strstr(result->processed, "8192") != NULL);
    ck_assert(strstr(result->processed, "50") != NULL);
    ck_assert(strstr(result->processed, "127.0.0.1") != NULL);
    ck_assert(strstr(result->processed, "8080") != NULL);
    ck_assert(strstr(result->processed, "0.70") != NULL);
    ck_assert(strstr(result->processed, "4096") != NULL);
    ck_assert(strstr(result->processed, "helpful assistant") != NULL);
    ck_assert_int_eq((int32_t)result->unresolved_count, 0);
}
END_TEST

START_TEST(test_func_now) {
    const char *input = "Now: ${func.now}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result->processed, "Now: ") == result->processed);
    ck_assert(strstr(result->processed, "T") != NULL);
    ck_assert(strstr(result->processed, "Z") != NULL);
    ck_assert_int_eq((int32_t)result->unresolved_count, 0);
}
END_TEST

START_TEST(test_func_random) {
    const char *input = "Random: ${func.random}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result->processed, "Random: ") == result->processed);
    ck_assert(strlen(result->processed) > 10);
    ck_assert_int_eq((int32_t)result->unresolved_count, 0);
}
END_TEST

START_TEST(test_unclosed_variable) {
    const char *input = "Unclosed: ${agent.uuid";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Unclosed: ${agent.uuid");
    ck_assert_int_eq((int32_t)result->unresolved_count, 0);
}
END_TEST

START_TEST(test_single_dollar_sign) {
    const char *input = "Price: $100";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Price: $100");
    ck_assert_int_eq((int32_t)result->unresolved_count, 0);
}
END_TEST

START_TEST(test_env_missing) {
    const char *input = "Env: ${env.NONEXISTENT_VAR_12345}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Env: ${env.NONEXISTENT_VAR_12345}");
    ck_assert_int_eq((int32_t)result->unresolved_count, 1);
}
END_TEST

START_TEST(test_unknown_func) {
    const char *input = "Func: ${func.unknown}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Func: ${func.unknown}");
    ck_assert_int_eq((int32_t)result->unresolved_count, 1);
}
END_TEST

START_TEST(test_agent_null) {
    const char *input = "Agent: ${agent.uuid}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, NULL, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Agent: ${agent.uuid}");
    ck_assert_int_eq((int32_t)result->unresolved_count, 1);
}
END_TEST

START_TEST(test_config_null) {
    const char *input = "Config: ${config.db_host}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, NULL, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Config: ${config.db_host}");
    ck_assert_int_eq((int32_t)result->unresolved_count, 1);
}
END_TEST

START_TEST(test_agent_field_null) {
    ik_agent_ctx_t *null_agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(null_agent);

    const char *input = "UUID: ${agent.uuid}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, null_agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "UUID: ${agent.uuid}");
    ck_assert_int_eq((int32_t)result->unresolved_count, 1);
}
END_TEST

START_TEST(test_config_field_null) {
    ik_config_t *null_config = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(null_config);

    const char *input = "Model: ${config.openai_model}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, null_config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Model: ${config.openai_model}");
    ck_assert_int_eq((int32_t)result->unresolved_count, 1);
}
END_TEST

START_TEST(test_unknown_namespace) {
    const char *input = "Unknown: ${unknown.field}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Unknown: ${unknown.field}");
    ck_assert_int_eq((int32_t)result->unresolved_count, 1);
}
END_TEST

START_TEST(test_duplicate_unresolved) {
    const char *input = "${agent.uuuid} ${agent.uuuid}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "${agent.uuuid} ${agent.uuuid}");
    ck_assert_int_eq((int32_t)result->unresolved_count, 1);
}
END_TEST

START_TEST(test_agent_string_fields_null) {
    ik_agent_ctx_t *test_agent = talloc_zero(ctx, ik_agent_ctx_t);
    test_agent->uuid = talloc_strdup(test_agent, "test-uuid");
    ik_template_result_t *result = NULL;

    const char *fields[] = {"name", "parent_uuid", "provider", "model"};
    for (size_t i = 0; i < 4; i++) {
        char *input = talloc_asprintf(ctx, "Field: ${agent.%s}", fields[i]);
        res_t res = ik_template_process(ctx, input, test_agent, config, &result);
        ck_assert(is_ok(&res));
        ck_assert(strstr(result->processed, "${agent.") != NULL);
        ck_assert_int_eq((int32_t)result->unresolved_count, 1);
        talloc_free(input);
        talloc_free(result);
        result = NULL;
    }
}
END_TEST

START_TEST(test_config_string_fields_null) {
    ik_config_t *test_config = talloc_zero(ctx, ik_config_t);
    test_config->db_port = 5432;
    ik_template_result_t *result = NULL;

    const char *fields[] = {"db_host", "db_name", "db_user", "default_provider",
                            "listen_address", "openai_system_message"};
    for (size_t i = 0; i < 6; i++) {
        char *input = talloc_asprintf(ctx, "Field: ${config.%s}", fields[i]);
        res_t res = ik_template_process(ctx, input, agent, test_config, &result);
        ck_assert(is_ok(&res));
        ck_assert(strstr(result->processed, "${config.") != NULL);
        ck_assert_int_eq((int32_t)result->unresolved_count, 1);
        talloc_free(input);
        talloc_free(result);
        result = NULL;
    }
}
END_TEST

static Suite *template_suite(void)
{
    Suite *s = suite_create("template");
    TCase *tc = tcase_create("core");

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_no_variables);
    tcase_add_test(tc, test_agent_all_fields);
    tcase_add_test(tc, test_config_db_host);
    tcase_add_test(tc, test_config_all_fields);
    tcase_add_test(tc, test_env_home);
    tcase_add_test(tc, test_env_missing);
    tcase_add_test(tc, test_func_cwd);
    tcase_add_test(tc, test_func_hostname);
    tcase_add_test(tc, test_func_now);
    tcase_add_test(tc, test_func_random);
    tcase_add_test(tc, test_escape_double_dollar);
    tcase_add_test(tc, test_unresolved_variable);
    tcase_add_test(tc, test_multiple_unresolved);
    tcase_add_test(tc, test_unclosed_variable);
    tcase_add_test(tc, test_single_dollar_sign);
    tcase_add_test(tc, test_unknown_func);
    tcase_add_test(tc, test_agent_null);
    tcase_add_test(tc, test_config_null);
    tcase_add_test(tc, test_agent_field_null);
    tcase_add_test(tc, test_config_field_null);
    tcase_add_test(tc, test_unknown_namespace);
    tcase_add_test(tc, test_duplicate_unresolved);
    tcase_add_test(tc, test_agent_string_fields_null);
    tcase_add_test(tc, test_config_string_fields_null);

    suite_add_tcase(s, tc);
    return s;
}

int32_t main(void)
{
    Suite *s = template_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/template_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (num_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
