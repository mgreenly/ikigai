/**
 * @file kill_line_test.c
 * @brief Unit tests for workspace kill_line operation (Ctrl+U)
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/workspace.h"
#include "../../test_utils.h"

/* Test: kill_line basic operation */
START_TEST(test_workspace_kill_line_basic) {
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "hello\nworld\ntest" */
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
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 't');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 's');
    ik_workspace_insert_codepoint(workspace, 't');

    /* Move cursor to middle of "world" line (after "wor") */
    /* Current position: after "test" (16) */
    /* Target position: after "wor" in world line (9) */
    for (int i = 0; i < 7; i++) {
        ik_workspace_cursor_left(workspace);
    }

    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 9); /* After "hello\nwor" */

    /* Action: kill line (should delete entire "world\n" line) */
    res = ik_workspace_kill_line(workspace);
    ck_assert(is_ok(&res));

    /* Assert: text is "hello\ntest", cursor at start of "test" line */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 10); /* "hello\ntest" */
    ck_assert_mem_eq(result_text, "hello\ntest", 10);

    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 6); /* At start of "test" line */

    talloc_free(ctx);
}

END_TEST
/* Test: kill_line on first line */
START_TEST(test_workspace_kill_line_first_line)
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

    /* Move cursor to middle of first line (after "hel") */
    for (int i = 0; i < 8; i++) {
        ik_workspace_cursor_left(workspace);
    }

    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 3); /* After "hel" */

    /* Action: kill line (should delete entire "hello\n" line) */
    res = ik_workspace_kill_line(workspace);
    ck_assert(is_ok(&res));

    /* Assert: text is "world", cursor at start */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 5); /* "world" */
    ck_assert_mem_eq(result_text, "world", 5);

    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 0); /* At start */

    talloc_free(ctx);
}

END_TEST
/* Test: kill_line on last line (no trailing newline) */
START_TEST(test_workspace_kill_line_last_line)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "hello\nworld" (no trailing newline) */
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

    /* Cursor is at end of "world" */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 11); /* After "hello\nworld" */

    /* Action: kill line (should delete "world" line, leave "hello\n") */
    res = ik_workspace_kill_line(workspace);
    ck_assert(is_ok(&res));

    /* Assert: text is "hello\n", cursor at position 6 (after "hello\n") */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 6); /* "hello\n" */
    ck_assert_mem_eq(result_text, "hello\n", 6);

    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 6); /* At end (after newline) */

    talloc_free(ctx);
}

END_TEST
/* Test: kill_line on empty line */
START_TEST(test_workspace_kill_line_empty_line)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "hello\n\nworld" (middle line is empty) */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_newline(workspace);
    ik_workspace_insert_codepoint(workspace, 'w');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, 'r');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'd');

    /* Move cursor to empty line (position 6, after "hello\n") */
    for (int i = 0; i < 6; i++) {
        ik_workspace_cursor_left(workspace);
    }

    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 6); /* After "hello\n" */

    /* Action: kill line (should delete the empty line's newline) */
    res = ik_workspace_kill_line(workspace);
    ck_assert(is_ok(&res));

    /* Assert: text is "hello\nworld", cursor still at 6 (after "hello\n") */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 11); /* "hello\nworld" */
    ck_assert_mem_eq(result_text, "hello\nworld", 11);

    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 6); /* At start of "world" line */

    talloc_free(ctx);
}

END_TEST
/* Test: kill_line followed by insert (reproduces crash bug) */
START_TEST(test_workspace_kill_line_then_insert)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "test" */
    ik_workspace_insert_codepoint(workspace, 't');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 's');
    ik_workspace_insert_codepoint(workspace, 't');

    /* Verify cursor at position 4 */
    size_t cursor_before = 0;
    size_t grapheme_before = 0;
    res_t res = ik_workspace_get_cursor_position(workspace, &cursor_before, &grapheme_before);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_before, 4); /* After "test" */

    /* Action: kill line (should delete entire "test" line) */
    res = ik_workspace_kill_line(workspace);
    ck_assert(is_ok(&res));

    /* Assert: text is empty */
    char *result_text = NULL;
    size_t result_len = 0;
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 0);

    /* Verify cursor at position 0 */
    size_t cursor_after = 0;
    size_t grapheme_after = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_after, &grapheme_after);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_after, 0); /* At start of empty workspace */

    /* BUG REPRODUCTION: Insert character after kill_line (this would crash before fix) */
    res = ik_workspace_insert_codepoint(workspace, 'a');
    ck_assert(is_ok(&res));

    /* Assert: text is "a", cursor at position 1 */
    res = ik_workspace_get_text(workspace, &result_text, &result_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(result_len, 1);
    ck_assert_mem_eq(result_text, "a", 1);

    size_t cursor_final = 0;
    size_t grapheme_final = 0;
    res = ik_workspace_get_cursor_position(workspace, &cursor_final, &grapheme_final);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_final, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: NULL workspace should assert */
START_TEST(test_workspace_kill_line_null_workspace_asserts)
{
    /* workspace cannot be NULL - should abort */
    ik_workspace_kill_line(NULL);
}

END_TEST

static Suite *workspace_kill_line_suite(void)
{
    Suite *s = suite_create("Workspace Kill Line");
    TCase *tc_core = tcase_create("Core");
    TCase *tc_assertions = tcase_create("Assertions");

    /* Normal tests */
    tcase_add_test(tc_core, test_workspace_kill_line_basic);
    tcase_add_test(tc_core, test_workspace_kill_line_first_line);
    tcase_add_test(tc_core, test_workspace_kill_line_last_line);
    tcase_add_test(tc_core, test_workspace_kill_line_empty_line);
    tcase_add_test(tc_core, test_workspace_kill_line_then_insert);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_assertions, test_workspace_kill_line_null_workspace_asserts, SIGABRT);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_assertions);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = workspace_kill_line_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
