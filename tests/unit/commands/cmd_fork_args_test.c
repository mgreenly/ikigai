/**
 * @file cmd_fork_args_test.c
 * @brief Unit tests for fork argument parsing functions
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
START_TEST(test_parse_args_empty_input) {
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
START_TEST(test_parse_args_quoted_prompt) {
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
START_TEST(test_parse_args_model_only) {
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
START_TEST(test_parse_args_model_then_prompt) {
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
START_TEST(test_parse_args_prompt_then_model) {
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
START_TEST(test_parse_args_model_no_arg) {
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "--model", &model, &prompt);
    ck_assert(is_err(&res));
}

END_TEST
// Test: Parse --model with only whitespace
START_TEST(test_parse_args_model_whitespace_only) {
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "--model   ", &model, &prompt);
    ck_assert(is_err(&res));
}

END_TEST
// Test: Parse unterminated quote
START_TEST(test_parse_args_unterminated_quote) {
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "\"unterminated", &model, &prompt);
    ck_assert(is_err(&res));
}

END_TEST
// Test: Parse unquoted text
START_TEST(test_parse_args_unquoted_text) {
    char *model = NULL;
    char *prompt = NULL;

    res_t res = cmd_fork_parse_args(test_ctx, "unquoted", &model, &prompt);
    ck_assert(is_err(&res));
}

END_TEST
// Test: Parse with leading whitespace
START_TEST(test_parse_args_leading_whitespace) {
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
START_TEST(test_parse_args_with_tabs) {
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
START_TEST(test_parse_args_empty_quoted) {
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
START_TEST(test_parse_args_model_with_slash) {
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
START_TEST(test_parse_args_model_followed_by_quote) {
    char *model = NULL;
    char *prompt = NULL;

    // This tests the edge case where --model is followed immediately by a quote
    // which should be caught by the model_len == 0 check
    res_t res = cmd_fork_parse_args(test_ctx, "--model \"prompt\"", &model, &prompt);
    ck_assert(is_err(&res));
}

END_TEST

static Suite *cmd_fork_args_suite(void)
{
    Suite *s = suite_create("Fork Argument Parsing");
    TCase *tc_parse = tcase_create("Parse Args");
    tcase_set_timeout(tc_parse, 30);

    tcase_add_checked_fixture(tc_parse, setup, teardown);

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

    suite_add_tcase(s, tc_parse);

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
