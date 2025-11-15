/**
 * @file repl_readline_test.c
 * @brief Unit tests for REPL readline-style editing shortcuts
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/input.h"
#include "../../test_utils.h"

/* Test: Process CTRL_A action (beginning of line) */
START_TEST(test_repl_process_action_ctrl_a) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    // Insert "hello\nworld" and position cursor at end
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'o');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_newline(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'w');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'o');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'r');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'd');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Cursor is at byte 11 (after "hello\nworld"), grapheme 11
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 11);
    ck_assert_uint_eq(grapheme_offset, 11);

    // Process Ctrl+A action
    ik_input_action_t action = {.type = IK_INPUT_CTRL_A};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Cursor should be at byte 6 (after "\n", start of "world" line), grapheme 6
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);
    ck_assert_uint_eq(grapheme_offset, 6);

    talloc_free(ctx);
}
END_TEST
/* Test: Process CTRL_E action (end of line) */
START_TEST(test_repl_process_action_ctrl_e)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    // Insert "hello\nworld"
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'o');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_newline(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'w');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'o');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'r');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'd');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Move cursor to start of "world" line
    res = ik_input_buffer_cursor_to_line_start(input_buf);
    ck_assert(is_ok(&res));

    // Cursor should be at byte 6 (after "\n", start of "world" line)
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);
    ck_assert_uint_eq(grapheme_offset, 6);

    // Process Ctrl+E action
    ik_input_action_t action = {.type = IK_INPUT_CTRL_E};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Cursor should be at byte 11 (end of "world" line)
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 11);
    ck_assert_uint_eq(grapheme_offset, 11);

    talloc_free(ctx);
}

END_TEST
/* Test: Process CTRL_K action (kill to end of line) */
START_TEST(test_repl_process_action_ctrl_k)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    // Insert "hello\nworld\ntest"
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'o');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_newline(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'w');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'o');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'r');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'd');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_newline(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 't');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 's');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 't');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Move cursor to middle of "world" line (after "wo")
    // Cursor is at byte 16 (after "hello\nworld\ntest")
    // Move to byte 8 (after "hello\nwo")
    char *text = NULL;
    size_t text_len = 0;
    res = ik_input_buffer_get_text(input_buf, &text, &text_len);
    ck_assert(is_ok(&res));
    input_buf->cursor_byte_offset = 8;
    ik_cursor_set_position(input_buf->cursor, text, text_len, 8);

    // Verify cursor is at byte 8
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 8);

    // Process Ctrl+K action
    ik_input_action_t action = {.type = IK_INPUT_CTRL_K};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Text should be "hello\nwo\ntest" (removed "rld")
    res = ik_input_buffer_get_text(input_buf, &text, &text_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(text_len, 13);
    ck_assert_mem_eq(text, "hello\nwo\ntest", 13);

    // Cursor should still be at byte 8
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 8);

    talloc_free(ctx);
}

END_TEST
/* Test: Process CTRL_U action (kill line) */
START_TEST(test_repl_process_action_ctrl_u)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    // Insert "hello\nworld\ntest"
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'o');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_newline(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'w');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'o');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'r');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'd');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_newline(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 't');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 's');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 't');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Move cursor to middle of "world" line (after "wo")
    char *text = NULL;
    size_t text_len = 0;
    res = ik_input_buffer_get_text(input_buf, &text, &text_len);
    ck_assert(is_ok(&res));
    input_buf->cursor_byte_offset = 8;
    ik_cursor_set_position(input_buf->cursor, text, text_len, 8);

    // Verify cursor is at byte 8
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 8);

    // Process Ctrl+U action
    ik_input_action_t action = {.type = IK_INPUT_CTRL_U};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Text should be "hello\ntest" (removed entire "world\n" line)
    res = ik_input_buffer_get_text(input_buf, &text, &text_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(text_len, 10);
    ck_assert_mem_eq(text, "hello\ntest", 10);

    // Cursor should be at byte 6 (start of "test" line)
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);

    talloc_free(ctx);
}

END_TEST
/* Test: Process CTRL_W action (delete word backward) */
START_TEST(test_repl_process_action_ctrl_w)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res = ik_input_buffer_create(ctx, &input_buf);
    ck_assert(is_ok(&res));

    // Insert "hello world test"
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'o');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, ' ');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'w');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'o');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'r');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'd');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, ' ');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 't');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 's');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 't');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Cursor is at byte 16 (after "hello world test")
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 16);

    // Process Ctrl+W action
    ik_input_action_t action = {.type = IK_INPUT_CTRL_W};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Text should be "hello world " (removed "test")
    char *text = NULL;
    size_t text_len = 0;
    res = ik_input_buffer_get_text(input_buf, &text, &text_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(text_len, 12);
    ck_assert_mem_eq(text, "hello world ", 12);

    // Cursor should be at byte 12
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 12);

    talloc_free(ctx);
}

END_TEST

static Suite *repl_readline_suite(void)
{
    Suite *s = suite_create("REPL_Readline");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_repl_process_action_ctrl_a);
    tcase_add_test(tc_core, test_repl_process_action_ctrl_e);
    tcase_add_test(tc_core, test_repl_process_action_ctrl_k);
    tcase_add_test(tc_core, test_repl_process_action_ctrl_u);
    tcase_add_test(tc_core, test_repl_process_action_ctrl_w);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_readline_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
