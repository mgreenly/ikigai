/**
 * @file cursor_left_right_test.c
 * @brief Unit tests for workspace horizontal cursor movement operations
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/workspace.h"
#include "../../test_utils.h"

/* Test: Cursor left - ASCII */
START_TEST(test_workspace_cursor_left_ascii) {
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "abc" */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');
    ik_workspace_insert_codepoint(workspace, 'c');

    /* Move left */
    res_t res = ik_workspace_cursor_left(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 2, grapheme 2 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 2);
    ck_assert_uint_eq(grapheme_offset, 2);

    /* Move left again */
    res = ik_workspace_cursor_left(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 1, grapheme 1 */
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}
END_TEST
/* Test: Cursor left - UTF-8 */
START_TEST(test_workspace_cursor_left_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "a" + é (2 bytes) + "b" */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 0x00E9);
    ik_workspace_insert_codepoint(workspace, 'b');

    /* Move left (skip 'b') */
    res_t res = ik_workspace_cursor_left(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 3 (after é), grapheme 2 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 3);
    ck_assert_uint_eq(grapheme_offset, 2);

    /* Move left (skip é - both bytes) */
    res = ik_workspace_cursor_left(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 1 (after 'a'), grapheme 1 */
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor left at start - no-op */
START_TEST(test_workspace_cursor_left_at_start)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Move left at start - should be no-op */
    res_t res = ik_workspace_cursor_left(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor still at 0,0 */
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor right - ASCII */
START_TEST(test_workspace_cursor_right_ascii)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "abc" */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');
    ik_workspace_insert_codepoint(workspace, 'c');

    /* Move to start */
    workspace->cursor_byte_offset = 0;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 0);

    /* Move right */
    res_t res = ik_workspace_cursor_right(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 1, grapheme 1 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor right - UTF-8 */
START_TEST(test_workspace_cursor_right_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "a" + 🎉 (4 bytes) */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 0x1F389);

    /* Move to start */
    workspace->cursor_byte_offset = 0;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 0);

    /* Move right (skip 'a') */
    res_t res = ik_workspace_cursor_right(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 1, grapheme 1 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    /* Move right (skip 🎉 - all 4 bytes) */
    res = ik_workspace_cursor_right(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 5 (1 + 4), grapheme 2 */
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 5);
    ck_assert_uint_eq(grapheme_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor right at end - no-op */
START_TEST(test_workspace_cursor_right_at_end)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "a" */
    ik_workspace_insert_codepoint(workspace, 'a');

    /* Move right at end - should be no-op */
    res_t res = ik_workspace_cursor_right(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 1, grapheme 1 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
START_TEST(test_workspace_cursor_left_null_workspace_asserts)
{
    /* workspace cannot be NULL - should abort */
    ik_workspace_cursor_left(NULL);
}

END_TEST
START_TEST(test_workspace_cursor_right_null_workspace_asserts)
{
    /* workspace cannot be NULL - should abort */
    ik_workspace_cursor_right(NULL);
}

END_TEST

static Suite *workspace_cursor_left_right_suite(void)
{
    Suite *s = suite_create("Workspace Cursor Left/Right");
    TCase *tc_core = tcase_create("Core");
    TCase *tc_assertions = tcase_create("Assertions");

    /* Normal tests */
    tcase_add_test(tc_core, test_workspace_cursor_left_ascii);
    tcase_add_test(tc_core, test_workspace_cursor_left_utf8);
    tcase_add_test(tc_core, test_workspace_cursor_left_at_start);
    tcase_add_test(tc_core, test_workspace_cursor_right_ascii);
    tcase_add_test(tc_core, test_workspace_cursor_right_utf8);
    tcase_add_test(tc_core, test_workspace_cursor_right_at_end);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_assertions, test_workspace_cursor_left_null_workspace_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_workspace_cursor_right_null_workspace_asserts, SIGABRT);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_assertions);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = workspace_cursor_left_right_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
