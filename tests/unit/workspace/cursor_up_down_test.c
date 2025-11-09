/**
 * @file cursor_up_down_test.c
 * @brief Unit tests for workspace vertical cursor movement operations
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/workspace.h"
#include "../../test_utils.h"

/* Test: Cursor up - basic */
START_TEST(test_workspace_cursor_up_basic)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "line1\nline2\nline3" */
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'n');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, '1');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'n');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, '2');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'n');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, '3');

    /* Cursor should be at end: byte 17 (after "line1\nline2\nline3"), grapheme 17 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 17);
    ck_assert_uint_eq(grapheme_offset, 17);

    /* Move cursor to start of line2 (byte 6, after first newline) */
    workspace->cursor_byte_offset = 6;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 6);

    /* Move up - should go to start of line1 (byte 0) */
    res = ik_workspace_cursor_up(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 0, grapheme 0 */
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor up from first line - no-op */
START_TEST(test_workspace_cursor_up_from_first_line)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "hello\nworld" */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'w');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, 'r');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'd');

    /* Move to position 2 (middle of first line) */
    workspace->cursor_byte_offset = 2;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 2);

    /* Move up - should be no-op (already on first line) */
    res_t res = ik_workspace_cursor_up(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 2, grapheme 2 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 2);
    ck_assert_uint_eq(grapheme_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor up with column preservation */
START_TEST(test_workspace_cursor_up_column_preservation)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "abcde\nfghij" */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');
    ik_workspace_insert_codepoint(workspace, 'c');
    ik_workspace_insert_codepoint(workspace, 'd');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'f');
    ik_workspace_insert_codepoint(workspace, 'g');
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'j');

    /* Move to position 9 (column 3 of second line: after 'h') */
    workspace->cursor_byte_offset = 9;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 9);

    /* Move up - should go to column 3 of first line (after 'c') */
    res_t res = ik_workspace_cursor_up(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 3, grapheme 3 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 3);
    ck_assert_uint_eq(grapheme_offset, 3);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor up to shorter line */
START_TEST(test_workspace_cursor_up_shorter_line)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "ab\nabcdef" (first line shorter) */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');
    ik_workspace_insert_codepoint(workspace, 'c');
    ik_workspace_insert_codepoint(workspace, 'd');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'f');

    /* Move to position 7 (column 4 of second line: after 'd') */
    workspace->cursor_byte_offset = 7;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 7);

    /* Move up - should go to end of first line (byte 2, after 'b') */
    res_t res = ik_workspace_cursor_up(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 2, grapheme 2 (end of first line) */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 2);
    ck_assert_uint_eq(grapheme_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor up with empty line */
START_TEST(test_workspace_cursor_up_empty_line)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "\nabc" (first line empty) */
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');
    ik_workspace_insert_codepoint(workspace, 'c');

    /* Move to position 2 (column 1 of second line: after 'a') */
    workspace->cursor_byte_offset = 2;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 2);

    /* Move up - should go to start of first line (byte 0) */
    res_t res = ik_workspace_cursor_up(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 0, grapheme 0 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor up with UTF-8 */
START_TEST(test_workspace_cursor_up_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "aé中🎉\ndefg" (2-byte, 3-byte, 4-byte UTF-8) */
    ik_workspace_insert_codepoint(workspace, 'a');          // 1 byte
    ik_workspace_insert_codepoint(workspace, 0x00E9);       // é (2 bytes)
    ik_workspace_insert_codepoint(workspace, 0x4E2D);       // 中 (3 bytes)
    ik_workspace_insert_codepoint(workspace, 0x1F389);      // 🎉 (4 bytes)
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'd');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'f');
    ik_workspace_insert_codepoint(workspace, 'g');

    /* Move to position 15 (column 4 of second line: after 'g') */
    // Text is: a(1) + é(2) + 中(3) + 🎉(4) + \n(1) + d(1) + e(1) + f(1) + g(1) = byte 15
    workspace->cursor_byte_offset = 15;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 15);

    /* Move up - should go to column 4 of first line (after 🎉, byte 10) */
    // Line 1: a(1) + é(2) + 中(3) + 🎉(4) = byte 10, grapheme 4
    res_t res = ik_workspace_cursor_up(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 10 (a + é + 中 + 🎉 = 1 + 2 + 3 + 4), grapheme 4 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 10);
    ck_assert_uint_eq(grapheme_offset, 4);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor down - basic */
START_TEST(test_workspace_cursor_down_basic)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "line1\nline2\nline3" */
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'n');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, '1');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'n');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, '2');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'n');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, '3');

    /* Move cursor to start of line1 (byte 0) */
    workspace->cursor_byte_offset = 0;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 0);

    /* Move down - should go to start of line2 (byte 6) */
    res_t res = ik_workspace_cursor_down(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 6, grapheme 6 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);
    ck_assert_uint_eq(grapheme_offset, 6);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor down from last line - no-op */
START_TEST(test_workspace_cursor_down_from_last_line)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "hello\nworld" */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'w');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, 'r');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'd');

    /* Already at end (byte 11), on last line */
    /* Move down - should be no-op */
    res_t res = ik_workspace_cursor_down(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 11, grapheme 11 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 11);
    ck_assert_uint_eq(grapheme_offset, 11);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor down with column preservation */
START_TEST(test_workspace_cursor_down_column_preservation)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "abcde\nfghij" */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');
    ik_workspace_insert_codepoint(workspace, 'c');
    ik_workspace_insert_codepoint(workspace, 'd');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'f');
    ik_workspace_insert_codepoint(workspace, 'g');
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'j');

    /* Move to position 3 (column 3 of first line: after 'c') */
    workspace->cursor_byte_offset = 3;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 3);

    /* Move down - should go to column 3 of second line (after 'h', byte 9) */
    res_t res = ik_workspace_cursor_down(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 9, grapheme 9 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 9);
    ck_assert_uint_eq(grapheme_offset, 9);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor down to shorter line */
START_TEST(test_workspace_cursor_down_shorter_line)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "abcdef\nab" (second line shorter) */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');
    ik_workspace_insert_codepoint(workspace, 'c');
    ik_workspace_insert_codepoint(workspace, 'd');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'f');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');

    /* Move to position 4 (column 4 of first line: after 'd') */
    workspace->cursor_byte_offset = 4;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 4);

    /* Move down - should go to end of second line (byte 9, after 'b') */
    res_t res = ik_workspace_cursor_down(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 9, grapheme 9 (end of second line) */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 9);
    ck_assert_uint_eq(grapheme_offset, 9);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor down with empty line */
START_TEST(test_workspace_cursor_down_empty_line)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "abc\n" (second line empty) */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');
    ik_workspace_insert_codepoint(workspace, 'c');
    ik_workspace_insert_newline(workspace);

    /* Move to position 1 (column 1 of first line: after 'a') */
    workspace->cursor_byte_offset = 1;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 1);

    /* Move down - should go to start of second line (byte 4, after newline) */
    res_t res = ik_workspace_cursor_down(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 4, grapheme 4 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 4);
    ck_assert_uint_eq(grapheme_offset, 4);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor down with UTF-8 */
START_TEST(test_workspace_cursor_down_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "abc\naé中🎉" (2-byte, 3-byte, 4-byte UTF-8 in second line) */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');
    ik_workspace_insert_codepoint(workspace, 'c');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'a');          // 1 byte
    ik_workspace_insert_codepoint(workspace, 0x00E9);       // é (2 bytes)
    ik_workspace_insert_codepoint(workspace, 0x4E2D);       // 中 (3 bytes)
    ik_workspace_insert_codepoint(workspace, 0x1F389);      // 🎉 (4 bytes)

    /* Move to position 2 (column 2 of first line: after 'b') */
    workspace->cursor_byte_offset = 2;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 2);

    /* Move down - should go to column 2 of second line (after é, byte 7) */
    // Line 2 starts at byte 4: a(1) + é(2) = byte 7, grapheme 6 (a,b,c,\n,a,é)
    res_t res = ik_workspace_cursor_down(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 7 (after a + é), grapheme 6 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 7);
    ck_assert_uint_eq(grapheme_offset, 6);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor up - NULL workspace asserts */
START_TEST(test_workspace_cursor_up_null_workspace_asserts)
{
    ik_workspace_cursor_up(NULL);
}

END_TEST
/* Test: Cursor down - NULL workspace asserts */
START_TEST(test_workspace_cursor_down_null_workspace_asserts)
{
    ik_workspace_cursor_down(NULL);
}

END_TEST

static Suite *workspace_cursor_up_down_suite(void)
{
    Suite *s = suite_create("Workspace Cursor Up/Down");
    TCase *tc_core = tcase_create("Core");
    TCase *tc_assertions = tcase_create("Assertions");

    /* Normal tests */
    tcase_add_test(tc_core, test_workspace_cursor_up_basic);
    tcase_add_test(tc_core, test_workspace_cursor_up_from_first_line);
    tcase_add_test(tc_core, test_workspace_cursor_up_column_preservation);
    tcase_add_test(tc_core, test_workspace_cursor_up_shorter_line);
    tcase_add_test(tc_core, test_workspace_cursor_up_empty_line);
    tcase_add_test(tc_core, test_workspace_cursor_up_utf8);
    tcase_add_test(tc_core, test_workspace_cursor_down_basic);
    tcase_add_test(tc_core, test_workspace_cursor_down_from_last_line);
    tcase_add_test(tc_core, test_workspace_cursor_down_column_preservation);
    tcase_add_test(tc_core, test_workspace_cursor_down_shorter_line);
    tcase_add_test(tc_core, test_workspace_cursor_down_empty_line);
    tcase_add_test(tc_core, test_workspace_cursor_down_utf8);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_assertions, test_workspace_cursor_up_null_workspace_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_workspace_cursor_down_null_workspace_asserts, SIGABRT);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_assertions);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = workspace_cursor_up_down_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
