/**
 * @file cursor_line_test.c
 * @brief Unit tests for workspace line-based cursor operations (Ctrl+A, Ctrl+E)
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/workspace.h"
#include "../../test_utils.h"

/* Test: Cursor to line start - basic */
START_TEST(test_workspace_cursor_to_line_start_basic) {
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

    /* Cursor is at end of "world" (byte 11, after 'd') */
    /* Position cursor in middle of "world" - move left twice to be after 'r' */
    ik_workspace_cursor_left(workspace);
    ik_workspace_cursor_left(workspace);

    /* Cursor should be at byte 9 (after 'r' in "world") */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 9);  /* "hello\nwor" = 9 bytes */

    /* Call cursor_to_line_start - should move to start of "world" (after \n) */
    res = ik_workspace_cursor_to_line_start(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 6 (start of "world", after \n) */
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);  /* "hello\n" = 6 bytes */
    ck_assert_uint_eq(grapheme_offset, 6);

    talloc_free(ctx);
}
END_TEST

/* Test: Cursor to line start - first line */
START_TEST(test_workspace_cursor_to_line_start_first_line) {
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "hello" (single line) */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'o');

    /* Cursor is at end (byte 5) */
    /* Move to middle - move left twice */
    ik_workspace_cursor_left(workspace);
    ik_workspace_cursor_left(workspace);

    /* Cursor should be at byte 3 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 3);

    /* Call cursor_to_line_start - should move to byte 0 */
    res = ik_workspace_cursor_to_line_start(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 0 */
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}
END_TEST

/* Test: Cursor to line start - already at start */
START_TEST(test_workspace_cursor_to_line_start_already_at_start) {
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

    /* Move cursor to start of "world" line */
    /* Move left 5 times to get to start of "world" */
    ik_workspace_cursor_left(workspace);
    ik_workspace_cursor_left(workspace);
    ik_workspace_cursor_left(workspace);
    ik_workspace_cursor_left(workspace);
    ik_workspace_cursor_left(workspace);

    /* Cursor should be at byte 6 (start of "world") */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);

    /* Call cursor_to_line_start - should remain at byte 6 (no-op) */
    res = ik_workspace_cursor_to_line_start(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 6 */
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);
    ck_assert_uint_eq(grapheme_offset, 6);

    talloc_free(ctx);
}
END_TEST

/* Test: Cursor to line start - after newline */
START_TEST(test_workspace_cursor_to_line_start_after_newline) {
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "line1\n\nline3" (empty line in middle) */
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'n');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, '1');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_newline(workspace);  /* Empty line */
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'i');
    ik_workspace_insert_codepoint(workspace, 'n');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, '3');

    /* Cursor is at end of "line3" (byte 13) */
    /* Move to start of line3 (byte 7) using cursor_left */
    ik_workspace_cursor_left(workspace);
    ik_workspace_cursor_left(workspace);
    ik_workspace_cursor_left(workspace);
    ik_workspace_cursor_left(workspace);
    ik_workspace_cursor_left(workspace);

    /* Cursor should be at byte 7 (start of "line3") */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 7);

    /* Call cursor_to_line_start - should remain at byte 7 (already at start) */
    res = ik_workspace_cursor_to_line_start(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 7 */
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 7);
    ck_assert_uint_eq(grapheme_offset, 7);

    /* Now move to the empty line (byte 6) */
    ik_workspace_cursor_left(workspace);  /* Move to byte 6, which is after second \n */

    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);

    /* Call cursor_to_line_start on empty line - should remain at byte 6 */
    res = ik_workspace_cursor_to_line_start(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 6 */
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);
    ck_assert_uint_eq(grapheme_offset, 6);

    talloc_free(ctx);
}
END_TEST

/* Test: NULL workspace should assert */
START_TEST(test_workspace_cursor_to_line_start_null_workspace_asserts)
{
    /* workspace cannot be NULL - should abort */
    ik_workspace_cursor_to_line_start(NULL);
}
END_TEST

static Suite *workspace_cursor_line_suite(void)
{
    Suite *s = suite_create("Workspace Cursor Line Operations");
    TCase *tc_core = tcase_create("Core");
    TCase *tc_assertions = tcase_create("Assertions");

    /* Normal tests */
    tcase_add_test(tc_core, test_workspace_cursor_to_line_start_basic);
    tcase_add_test(tc_core, test_workspace_cursor_to_line_start_first_line);
    tcase_add_test(tc_core, test_workspace_cursor_to_line_start_already_at_start);
    tcase_add_test(tc_core, test_workspace_cursor_to_line_start_after_newline);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_assertions, test_workspace_cursor_to_line_start_null_workspace_asserts, SIGABRT);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_assertions);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = workspace_cursor_line_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
