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

START_TEST(test_agent_uuid) {
    const char *input = "Agent: ${agent.uuid}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Agent: test-uuid-1234");
    ck_assert_int_eq((int32_t)result->unresolved_count, 0);
}
END_TEST

START_TEST(test_agent_name) {
    const char *input = "Name: ${agent.name}";
    ik_template_result_t *result = NULL;

    res_t res = ik_template_process(ctx, input, agent, config, &result);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result->processed, "Name: TestAgent");
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

static Suite *template_suite(void)
{
    Suite *s = suite_create("template");
    TCase *tc = tcase_create("core");

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_no_variables);
    tcase_add_test(tc, test_agent_uuid);
    tcase_add_test(tc, test_agent_name);
    tcase_add_test(tc, test_config_db_host);
    tcase_add_test(tc, test_env_home);
    tcase_add_test(tc, test_escape_double_dollar);
    tcase_add_test(tc, test_unresolved_variable);
    tcase_add_test(tc, test_multiple_unresolved);
    tcase_add_test(tc, test_func_cwd);
    tcase_add_test(tc, test_func_hostname);

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
