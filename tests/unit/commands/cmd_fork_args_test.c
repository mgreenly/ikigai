/**
 * @file cmd_fork_args_test.c
 * @brief Unit tests for fork argument parsing and model override functions
 */

#include "../../../src/commands_fork_args.h"

#include "../../../src/agent.h"
#include "../../../src/error.h"
#include "../../../src/providers/provider.h"

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

// Test: Parse empty input
START_TEST(test_parse_args_empty_input)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, NULL, &model, &prompt);
    ck_assert(is_ok(&res));
    ck_assert_ptr_null(model);
    ck_assert_ptr_null(prompt);

    res = cmd_fork_parse_args(test_ctx, "", &model, &prompt);
    ck_assert(is_ok(&res));
    ck_assert_ptr_null(model);
    ck_assert_ptr_null(prompt);
}
END_TEST

// Test: Parse quoted prompt only
START_TEST(test_parse_args_quoted_prompt)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "\"Hello World\"", &model, &prompt);
    ck_assert(is_ok(&res));
    ck_assert_ptr_null(model);
    ck_assert_ptr_nonnull(prompt);
    ck_assert_str_eq(prompt, "Hello World");
}
END_TEST

// Test: Parse --model only
START_TEST(test_parse_args_model_only)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "--model gpt-4o", &model, &prompt);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(model);
    ck_assert_str_eq(model, "gpt-4o");
    ck_assert_ptr_null(prompt);
}
END_TEST

// Test: Parse --model followed by prompt
START_TEST(test_parse_args_model_then_prompt)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "--model gpt-4o \"Test prompt\"", &model, &prompt);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(model);
    ck_assert_str_eq(model, "gpt-4o");
    ck_assert_ptr_nonnull(prompt);
    ck_assert_str_eq(prompt, "Test prompt");
}
END_TEST

// Test: Parse prompt followed by --model
START_TEST(test_parse_args_prompt_then_model)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "\"Test prompt\" --model gpt-4o", &model, &prompt);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(model);
    ck_assert_str_eq(model, "gpt-4o");
    ck_assert_ptr_nonnull(prompt);
    ck_assert_str_eq(prompt, "Test prompt");
}
END_TEST

// Test: Parse --model with no argument
START_TEST(test_parse_args_model_no_arg)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "--model", &model, &prompt);
    ck_assert(is_err(&res));
}
END_TEST

// Test: Parse --model with only whitespace
START_TEST(test_parse_args_model_whitespace_only)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "--model   ", &model, &prompt);
    ck_assert(is_err(&res));
}
END_TEST

// Test: Parse unterminated quote
START_TEST(test_parse_args_unterminated_quote)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "\"unterminated", &model, &prompt);
    ck_assert(is_err(&res));
}
END_TEST

// Test: Parse unquoted text
START_TEST(test_parse_args_unquoted_text)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "unquoted", &model, &prompt);
    ck_assert(is_err(&res));
}
END_TEST

// Test: Parse with leading whitespace
START_TEST(test_parse_args_leading_whitespace)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "   \"prompt\"", &model, &prompt);
    ck_assert(is_ok(&res));
    ck_assert_ptr_null(model);
    ck_assert_ptr_nonnull(prompt);
    ck_assert_str_eq(prompt, "prompt");
}
END_TEST

// Test: Parse with tabs
START_TEST(test_parse_args_with_tabs)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "\t--model\tgpt-4o\t\"prompt\"", &model, &prompt);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(model);
    ck_assert_str_eq(model, "gpt-4o");
    ck_assert_ptr_nonnull(prompt);
    ck_assert_str_eq(prompt, "prompt");
}
END_TEST

// Test: Parse empty quoted string
START_TEST(test_parse_args_empty_quoted)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "\"\"", &model, &prompt);
    ck_assert(is_ok(&res));
    ck_assert_ptr_null(model);
    ck_assert_ptr_nonnull(prompt);
    ck_assert_str_eq(prompt, "");
}
END_TEST

// Test: Parse --model with slash syntax
START_TEST(test_parse_args_model_with_slash)
{
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "--model gpt-4o/high", &model, &prompt);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(model);
    ck_assert_str_eq(model, "gpt-4o/high");
    ck_assert_ptr_null(prompt);
}
END_TEST

// Test: Parse --model followed by quote (edge case for model_len == 0 check)
START_TEST(test_parse_args_model_followed_by_quote)
{
    char *model = NULL;
    char *prompt = NULL;

    // This tests the edge case where --model is followed immediately by a quote
    // which should be caught by the model_len == 0 check
    res_t res = cmd_fork_parse_args(test_ctx, "--model \"prompt\"", &model, &prompt);
    ck_assert(is_err(&res));
}
END_TEST

// Test: Apply override with basic model
START_TEST(test_apply_override_basic_model)
{
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = cmd_fork_apply_override(child, "gpt-4o");
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "gpt-4o");
}
END_TEST

// Test: Apply override with thinking level none
START_TEST(test_apply_override_thinking_none)
{
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);
    child->thinking_level = IK_THINKING_HIGH;

    res_t res = cmd_fork_apply_override(child, "gpt-4o/none");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(child->thinking_level, IK_THINKING_NONE);
}
END_TEST

// Test: Apply override with thinking level low
START_TEST(test_apply_override_thinking_low)
{
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = cmd_fork_apply_override(child, "gpt-4o/low");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(child->thinking_level, IK_THINKING_LOW);
}
END_TEST

// Test: Apply override with thinking level med
START_TEST(test_apply_override_thinking_med)
{
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = cmd_fork_apply_override(child, "gpt-4o/med");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(child->thinking_level, IK_THINKING_MED);
}
END_TEST

// Test: Apply override with thinking level high
START_TEST(test_apply_override_thinking_high)
{
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = cmd_fork_apply_override(child, "gpt-4o/high");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(child->thinking_level, IK_THINKING_HIGH);
}
END_TEST

// Test: Apply override with invalid thinking level
START_TEST(test_apply_override_invalid_thinking)
{
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = cmd_fork_apply_override(child, "gpt-4o/invalid");
    ck_assert(is_err(&res));
}
END_TEST

// Test: Apply override with unknown model
START_TEST(test_apply_override_unknown_model)
{
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = cmd_fork_apply_override(child, "unknown-model-xyz");
    ck_assert(is_err(&res));
}
END_TEST

// Test: Apply override replaces existing provider
START_TEST(test_apply_override_replaces_provider)
{
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);
    child->provider = talloc_strdup(child, "anthropic");
    child->model = talloc_strdup(child, "claude-3-5-sonnet-20241022");

    res_t res = cmd_fork_apply_override(child, "gpt-4o");
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "gpt-4o");
}
END_TEST

// Test: Apply override with Anthropic model
START_TEST(test_apply_override_anthropic_model)
{
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = cmd_fork_apply_override(child, "claude-3-5-sonnet-20241022");
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "anthropic");
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "claude-3-5-sonnet-20241022");
}
END_TEST

// Test: Apply override with Google model
START_TEST(test_apply_override_google_model)
{
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = cmd_fork_apply_override(child, "gemini-2.0-flash-exp");
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "google");
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "gemini-2.0-flash-exp");
}
END_TEST

// Test: Apply override with invalid model parse (malformed MODEL/THINKING syntax)
START_TEST(test_apply_override_invalid_parse)
{
    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    // Test with trailing slash which should trigger cmd_model_parse error
    res_t res = cmd_fork_apply_override(child, "gpt-4o/");
    ck_assert(is_err(&res));
}
END_TEST

// Test: Inherit config from parent
START_TEST(test_inherit_config_basic)
{
    ik_agent_ctx_t *parent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(parent);
    parent->provider = talloc_strdup(parent, "openai");
    parent->model = talloc_strdup(parent, "gpt-4o");
    parent->thinking_level = IK_THINKING_MED;

    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = cmd_fork_inherit_config(child, parent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "gpt-4o");
    ck_assert_int_eq(child->thinking_level, IK_THINKING_MED);
}
END_TEST

// Test: Inherit config replaces existing child config
START_TEST(test_inherit_config_replaces_existing)
{
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

    res_t res = cmd_fork_inherit_config(child, parent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "gpt-4o");
    ck_assert_int_eq(child->thinking_level, IK_THINKING_LOW);
}
END_TEST

// Test: Inherit config with NULL parent provider
START_TEST(test_inherit_config_null_parent_provider)
{
    ik_agent_ctx_t *parent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(parent);
    parent->provider = NULL;
    parent->model = talloc_strdup(parent, "gpt-4o");
    parent->thinking_level = IK_THINKING_MED;

    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = cmd_fork_inherit_config(child, parent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_null(child->provider);
    ck_assert_ptr_nonnull(child->model);
    ck_assert_str_eq(child->model, "gpt-4o");
    ck_assert_int_eq(child->thinking_level, IK_THINKING_MED);
}
END_TEST

// Test: Inherit config with NULL parent model
START_TEST(test_inherit_config_null_parent_model)
{
    ik_agent_ctx_t *parent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(parent);
    parent->provider = talloc_strdup(parent, "openai");
    parent->model = NULL;
    parent->thinking_level = IK_THINKING_HIGH;

    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);

    res_t res = cmd_fork_inherit_config(child, parent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(child->provider);
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_ptr_null(child->model);
    ck_assert_int_eq(child->thinking_level, IK_THINKING_HIGH);
}
END_TEST

static Suite *cmd_fork_args_suite(void)
{
    Suite *s = suite_create("Fork Argument Parsing");
    TCase *tc_parse = tcase_create("Parse Args");
    TCase *tc_override = tcase_create("Apply Override");
    TCase *tc_inherit = tcase_create("Inherit Config");

    tcase_add_checked_fixture(tc_parse, setup, teardown);
    tcase_add_checked_fixture(tc_override, setup, teardown);
    tcase_add_checked_fixture(tc_inherit, setup, teardown);

    tcase_add_test(tc_parse, test_parse_args_empty_input);
    tcase_add_test(tc_parse, test_parse_args_quoted_prompt);
    tcase_add_test(tc_parse, test_parse_args_model_only);
    tcase_add_test(tc_parse, test_parse_args_model_then_prompt);
    tcase_add_test(tc_parse, test_parse_args_prompt_then_model);
    tcase_add_test(tc_parse, test_parse_args_model_no_arg);
    tcase_add_test(tc_parse, test_parse_args_model_whitespace_only);
    tcase_add_test(tc_parse, test_parse_args_unterminated_quote);
    tcase_add_test(tc_parse, test_parse_args_unquoted_text);
    tcase_add_test(tc_parse, test_parse_args_leading_whitespace);
    tcase_add_test(tc_parse, test_parse_args_with_tabs);
    tcase_add_test(tc_parse, test_parse_args_empty_quoted);
    tcase_add_test(tc_parse, test_parse_args_model_with_slash);
    tcase_add_test(tc_parse, test_parse_args_model_followed_by_quote);

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

    suite_add_tcase(s, tc_parse);
    suite_add_tcase(s, tc_override);
    suite_add_tcase(s, tc_inherit);

    return s;
}

int main(void)
{
    Suite *s = cmd_fork_args_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
