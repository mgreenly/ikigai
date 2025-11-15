/**
 * @file repl_run_basic_test.c
 * @brief Unit tests for REPL event loop basic functionality
 */

#include "repl_run_test_common.h"

/* Test: Simple character input followed by Ctrl+C */
START_TEST(test_repl_run_simple_char_input) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;
    repl->quit = false;

    mock_input = "a\x03";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    char *text = NULL;
    size_t len = 0;
    res = ik_input_buffer_get_text(repl->input_buffer, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 1);
    ck_assert_int_eq(text[0], 'a');
    talloc_free(text);

    ck_assert(repl->quit);

    talloc_free(ctx);
}
END_TEST
/* Test: Multiple character input */
START_TEST(test_repl_run_multiple_chars)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;
    repl->quit = false;

    mock_input = "abc\x03";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    char *text = NULL;
    size_t len = 0;
    res = ik_input_buffer_get_text(input_buf, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 3);
    ck_assert_int_eq(text[0], 'a');
    ck_assert_int_eq(text[1], 'b');
    ck_assert_int_eq(text[2], 'c');
    talloc_free(text);

    ck_assert(repl->quit);

    talloc_free(ctx);
}

END_TEST
/* Test: Input with newline */
START_TEST(test_repl_run_with_newline)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;
    repl->quit = false;

    mock_input = "hi\n\x03";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    char *text = NULL;
    size_t len = 0;
    res = ik_input_buffer_get_text(input_buf, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 3);
    ck_assert_int_eq(text[0], 'h');
    ck_assert_int_eq(text[1], 'i');
    ck_assert_int_eq(text[2], '\n');
    talloc_free(text);

    talloc_free(ctx);
}

END_TEST
/* Test: Input with backspace */
START_TEST(test_repl_run_with_backspace)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;
    repl->quit = false;

    mock_input = "ab\x7f\x03";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    char *text = NULL;
    size_t len = 0;
    res = ik_input_buffer_get_text(input_buf, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 1);
    ck_assert_int_eq(text[0], 'a');
    talloc_free(text);

    talloc_free(ctx);
}

END_TEST
/* Test: Read EOF */
START_TEST(test_repl_run_read_eof)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;
    repl->quit = false;

    mock_input = "";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    char *text = NULL;
    size_t len = 0;
    res = ik_input_buffer_get_text(input_buf, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 0);
    talloc_free(text);

    ck_assert_int_eq(repl->quit, false);

    talloc_free(ctx);
}

END_TEST
/* Test: REPL handles incomplete escape sequence at EOF */
START_TEST(test_repl_run_unknown_action)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;
    repl->quit = false;

    mock_input = "a\x1b";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    ck_assert_int_eq(repl->quit, false);

    char *text = NULL;
    size_t len = 0;
    res = ik_input_buffer_get_text(input_buf, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 1);
    ck_assert_int_eq(text[0], 'a');
    talloc_free(text);

    talloc_free(ctx);
}

END_TEST

static Suite *repl_run_basic_suite(void)
{
    Suite *s = suite_create("REPL_Run_Basic");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_repl_run_simple_char_input);
    tcase_add_test(tc_core, test_repl_run_multiple_chars);
    tcase_add_test(tc_core, test_repl_run_with_newline);
    tcase_add_test(tc_core, test_repl_run_with_backspace);
    tcase_add_test(tc_core, test_repl_run_read_eof);
    tcase_add_test(tc_core, test_repl_run_unknown_action);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_run_basic_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
