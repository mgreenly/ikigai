#include "tests/test_constants.h"
/**
 * @file cmd_fork_args_override_test.c
 * @brief Unit tests for fork model override and config inheritance functions
 */

#include "apps/ikigai/commands_fork_args.h"

#include "apps/ikigai/agent.h"
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);
}

static void teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
}

// Test: Apply override with basic model
START_TEST(test_apply_override_basic_model) {
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = ik_commands_fork_apply_override(child, "gpt-4o");
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "gpt-4o");
}
END_TEST
// Test: Apply override with thinking level min
START_TEST(test_apply_override_thinking_none) {
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);
    child->thinking_level = IK_THINKING_HIGH;

    res_t res = ik_commands_fork_apply_override(child, "gpt-4o/min");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(child->thinking_level, IK_THINKING_MIN);
}

END_TEST
// Test: Apply override with thinking level low
START_TEST(test_apply_override_thinking_low) {
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = ik_commands_fork_apply_override(child, "gpt-4o/low");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(child->thinking_level, IK_THINKING_LOW);
}

END_TEST
// Test: Apply override with thinking level med
START_TEST(test_apply_override_thinking_med) {
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = ik_commands_fork_apply_override(child, "gpt-4o/med");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(child->thinking_level, IK_THINKING_MED);
}

END_TEST
// Test: Apply override with thinking level high
START_TEST(test_apply_override_thinking_high) {
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = ik_commands_fork_apply_override(child, "gpt-4o/high");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(child->thinking_level, IK_THINKING_HIGH);
}

END_TEST
// Test: Apply override with invalid thinking level
START_TEST(test_apply_override_invalid_thinking) {
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = ik_commands_fork_apply_override(child, "gpt-4o/invalid");
    ck_assert(is_err(&res));
}

END_TEST
// Test: Apply override with unknown model
START_TEST(test_apply_override_unknown_model) {
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = ik_commands_fork_apply_override(child, "unknown-model-xyz");
    ck_assert(is_err(&res));
}

END_TEST
// Test: Apply override replaces existing provider
START_TEST(test_apply_override_replaces_provider) {
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);
    child->provider = talloc_strdup(child, "anthropic");
    child->model = talloc_strdup(child, "claude-3-5-sonnet-20241022");

    res_t res = ik_commands_fork_apply_override(child, "gpt-4o");
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "gpt-4o");
}

END_TEST
// Test: Apply override with Anthropic model
START_TEST(test_apply_override_anthropic_model) {
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = ik_commands_fork_apply_override(child, "claude-3-5-sonnet-20241022");
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "anthropic");
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "claude-3-5-sonnet-20241022");
}

END_TEST
// Test: Apply override with Google model
START_TEST(test_apply_override_google_model) {
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = ik_commands_fork_apply_override(child, "gemini-2.0-flash-exp");
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "google");
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "gemini-2.0-flash-exp");
}

END_TEST
// Test: Apply override with invalid model parse (malformed MODEL/THINKING syntax)
START_TEST(test_apply_override_invalid_parse) {
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    // Test with trailing slash which should trigger ik_commands_model_parse error
    res_t res = ik_commands_fork_apply_override(child, "gpt-4o/");
    ck_assert(is_err(&res));
}

END_TEST
// Test: Inherit config from parent
START_TEST(test_inherit_config_basic) {
    ik_agent_ctx_t *parent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(parent);
    parent->provider = talloc_strdup(parent, "openai");
    parent->model = talloc_strdup(parent, "gpt-4o");
    parent->thinking_level = IK_THINKING_MED;

    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = ik_commands_fork_inherit_config(child, parent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "gpt-4o");
    ck_assert_int_eq(child->thinking_level, IK_THINKING_MED);
}

END_TEST
// Test: Inherit config replaces existing child config
START_TEST(test_inherit_config_replaces_existing) {
    ik_agent_ctx_t *parent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(parent);
    parent->provider = talloc_strdup(parent, "openai");
    parent->model = talloc_strdup(parent, "gpt-4o");
    parent->thinking_level = IK_THINKING_LOW;

    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);
    child->provider = talloc_strdup(child, "anthropic");
    child->model = talloc_strdup(child, "claude-3-5-sonnet-20241022");
    child->thinking_level = IK_THINKING_HIGH;

    res_t res = ik_commands_fork_inherit_config(child, parent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "gpt-4o");
    ck_assert_int_eq(child->thinking_level, IK_THINKING_LOW);
}

END_TEST
// Test: Inherit config with NULL parent provider
START_TEST(test_inherit_config_null_parent_provider) {
    ik_agent_ctx_t *parent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(parent);
    parent->provider = NULL;
    parent->model = talloc_strdup(parent, "gpt-4o");
    parent->thinking_level = IK_THINKING_MED;

    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = ik_commands_fork_inherit_config(child, parent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_null(child->provider);
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "gpt-4o");
    ck_assert_int_eq(child->thinking_level, IK_THINKING_MED);
}

END_TEST
// Test: Inherit config with NULL parent model
START_TEST(test_inherit_config_null_parent_model) {
    ik_agent_ctx_t *parent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(parent);
    parent->provider = talloc_strdup(parent, "openai");
    parent->model = NULL;
    parent->thinking_level = IK_THINKING_HIGH;

    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = ik_commands_fork_inherit_config(child, parent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_ptr_null(child->model);
    ck_assert_int_eq(child->thinking_level, IK_THINKING_HIGH);
}

END_TEST

static Suite *cmd_fork_args_override_suite(void)
{
    Suite *s = suite_create("Fork Model Override and Config Inheritance");
    TCase *tc_override = tcase_create("Apply Override");
    tcase_set_timeout(tc_override, IK_TEST_TIMEOUT);
    TCase *tc_inherit = tcase_create("Inherit Config");
    tcase_set_timeout(tc_inherit, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc_override, setup, teardown);
    tcase_add_checked_fixture(tc_inherit, setup, teardown);

    tcase_add_test(tc_override, test_apply_override_basic_model);
    tcase_add_test(tc_override, test_apply_override_thinking_none);
    tcase_add_test(tc_override, test_apply_override_thinking_low);
    tcase_add_test(tc_override, test_apply_override_thinking_med);
    tcase_add_test(tc_override, test_apply_override_thinking_high);
    tcase_add_test(tc_override, test_apply_override_invalid_thinking);
    tcase_add_test(tc_override, test_apply_override_unknown_model);
    tcase_add_test(tc_override, test_apply_override_replaces_provider);
    tcase_add_test(tc_override, test_apply_override_anthropic_model);
    tcase_add_test(tc_override, test_apply_override_google_model);
    tcase_add_test(tc_override, test_apply_override_invalid_parse);

    tcase_add_test(tc_inherit, test_inherit_config_basic);
    tcase_add_test(tc_inherit, test_inherit_config_replaces_existing);
    tcase_add_test(tc_inherit, test_inherit_config_null_parent_provider);
    tcase_add_test(tc_inherit, test_inherit_config_null_parent_model);

    suite_add_tcase(s, tc_override);
    suite_add_tcase(s, tc_inherit);

    return s;
}

int main(void)
{
    Suite *s = cmd_fork_args_override_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/cmd_fork_args_override_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
