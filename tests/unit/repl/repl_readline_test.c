/**
 * @file repl_readline_test.c
 * @brief Unit tests for REPL readline-style editing shortcuts
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/repl.h"
#include "../../../src/input.h"
#include "../../test_utils.h"

/* Test: Process CTRL_A action (beginning of line) */
START_TEST(test_repl_process_action_ctrl_a) {
    void *ctx = talloc_new(NULL);

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Insert "hello\nworld" and position cursor at end
    res = ik_workspace_insert_codepoint(workspace, 'h');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'e');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'o');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_newline(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'w');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'o');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'r');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'd');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->quit = false;

    // Cursor is at byte 11 (after "hello\nworld"), grapheme 11
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 11);
    ck_assert_uint_eq(grapheme_offset, 11);

    // Process Ctrl+A action
    ik_input_action_t action = {.type = IK_INPUT_CTRL_A};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Cursor should be at byte 6 (after "\n", start of "world" line), grapheme 6
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
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

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Insert "hello\nworld"
    res = ik_workspace_insert_codepoint(workspace, 'h');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'e');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'o');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_newline(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'w');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'o');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'r');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'd');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->quit = false;

    // Move cursor to start of "world" line
    res = ik_workspace_cursor_to_line_start(workspace);
    ck_assert(is_ok(&res));

    // Cursor should be at byte 6 (after "\n", start of "world" line)
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);
    ck_assert_uint_eq(grapheme_offset, 6);

    // Process Ctrl+E action
    ik_input_action_t action = {.type = IK_INPUT_CTRL_E};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Cursor should be at byte 11 (end of "world" line)
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
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

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Insert "hello\nworld\ntest"
    res = ik_workspace_insert_codepoint(workspace, 'h');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'e');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'o');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_newline(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'w');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'o');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'r');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'd');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_newline(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 't');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'e');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 's');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 't');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->quit = false;

    // Move cursor to middle of "world" line (after "wo")
    // Cursor is at byte 16 (after "hello\nworld\ntest")
    // Move to byte 8 (after "hello\nwo")
    char *text = NULL;
    size_t text_len = 0;
    res = ik_workspace_get_text(workspace, &text, &text_len);
    ck_assert(is_ok(&res));
    workspace->cursor_byte_offset = 8;
    ik_cursor_set_position(workspace->cursor, text, text_len, 8);

    // Verify cursor is at byte 8
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 8);

    // Process Ctrl+K action
    ik_input_action_t action = {.type = IK_INPUT_CTRL_K};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Text should be "hello\nwo\ntest" (removed "rld")
    res = ik_workspace_get_text(workspace, &text, &text_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(text_len, 13);
    ck_assert_mem_eq(text, "hello\nwo\ntest", 13);

    // Cursor should still be at byte 8
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 8);

    talloc_free(ctx);
}

END_TEST
/* Test: Process CTRL_U action (kill line) */
START_TEST(test_repl_process_action_ctrl_u)
{
    void *ctx = talloc_new(NULL);

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Insert "hello\nworld\ntest"
    res = ik_workspace_insert_codepoint(workspace, 'h');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'e');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'o');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_newline(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'w');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'o');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'r');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'd');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_newline(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 't');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'e');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 's');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 't');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->quit = false;

    // Move cursor to middle of "world" line (after "wo")
    char *text = NULL;
    size_t text_len = 0;
    res = ik_workspace_get_text(workspace, &text, &text_len);
    ck_assert(is_ok(&res));
    workspace->cursor_byte_offset = 8;
    ik_cursor_set_position(workspace->cursor, text, text_len, 8);

    // Verify cursor is at byte 8
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 8);

    // Process Ctrl+U action
    ik_input_action_t action = {.type = IK_INPUT_CTRL_U};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Text should be "hello\ntest" (removed entire "world\n" line)
    res = ik_workspace_get_text(workspace, &text, &text_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(text_len, 10);
    ck_assert_mem_eq(text, "hello\ntest", 10);

    // Cursor should be at byte 6 (start of "test" line)
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);

    talloc_free(ctx);
}

END_TEST
/* Test: Process CTRL_W action (delete word backward) */
START_TEST(test_repl_process_action_ctrl_w)
{
    void *ctx = talloc_new(NULL);

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Insert "hello world test"
    res = ik_workspace_insert_codepoint(workspace, 'h');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'e');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'o');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, ' ');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'w');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'o');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'r');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'l');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'd');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, ' ');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 't');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'e');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 's');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 't');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->quit = false;

    // Cursor is at byte 16 (after "hello world test")
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 16);

    // Process Ctrl+W action
    ik_input_action_t action = {.type = IK_INPUT_CTRL_W};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Text should be "hello world " (removed "test")
    char *text = NULL;
    size_t text_len = 0;
    res = ik_workspace_get_text(workspace, &text, &text_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(text_len, 12);
    ck_assert_mem_eq(text, "hello world ", 12);

    // Cursor should be at byte 12
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 12);

    talloc_free(ctx);
}

END_TEST

static Suite *repl_readline_suite(void)
{
    Suite *s = suite_create("REPL_Readline");
    TCase *tc_core = tcase_create("Core");

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
