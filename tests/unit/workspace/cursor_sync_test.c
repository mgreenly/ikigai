/**
 * @file cursor_sync_test.c
 * @brief Unit tests for workspace cursor synchronization with text operations
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/workspace.h"
#include "../../test_utils.h"

/* Test: Workspace cursor initialized to 0,0 */
START_TEST(test_workspace_cursor_initialized) {
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    /* Create workspace */
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at position 0,0 */
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: Cursor advances after inserting ASCII */
START_TEST(test_workspace_cursor_after_insert_ascii)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert 'a' */
    res_t res = ik_workspace_insert_codepoint(workspace, 'a');
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 1, grapheme 1 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    /* Insert 'b' */
    res = ik_workspace_insert_codepoint(workspace, 'b');
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 2, grapheme 2 */
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 2);
    ck_assert_uint_eq(grapheme_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor advances correctly for UTF-8 */
START_TEST(test_workspace_cursor_after_insert_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert 'a' */
    res_t res = ik_workspace_insert_codepoint(workspace, 'a');
    ck_assert(is_ok(&res));

    /* Insert é (U+00E9, 2 bytes) */
    res = ik_workspace_insert_codepoint(workspace, 0x00E9);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 3 (1 + 2), grapheme 2 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 3);
    ck_assert_uint_eq(grapheme_offset, 2);

    /* Insert 🎉 (U+1F389, 4 bytes) */
    res = ik_workspace_insert_codepoint(workspace, 0x1F389);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 7 (3 + 4), grapheme 3 */
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 7);
    ck_assert_uint_eq(grapheme_offset, 3);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor after newline insert */
START_TEST(test_workspace_cursor_after_newline)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "hi" */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'i');

    /* Insert newline */
    res_t res = ik_workspace_insert_newline(workspace);
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
/* Test: Cursor after backspace */
START_TEST(test_workspace_cursor_after_backspace)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "abc" */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');
    ik_workspace_insert_codepoint(workspace, 'c');

    /* Backspace once */
    res_t res = ik_workspace_backspace(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 2, grapheme 2 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 2);
    ck_assert_uint_eq(grapheme_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor after backspace UTF-8 */
START_TEST(test_workspace_cursor_after_backspace_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "a" + é (2 bytes) */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 0x00E9);

    /* Backspace once (delete é) */
    res_t res = ik_workspace_backspace(workspace);
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
/* Test: Cursor after delete (stays same) */
START_TEST(test_workspace_cursor_after_delete)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "abc" */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');
    ik_workspace_insert_codepoint(workspace, 'c');

    /* Move cursor to middle (byte 1, after 'a') */
    workspace->cursor_byte_offset = 1;
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len);
    ik_cursor_set_position(workspace->cursor, text, text_len, 1);

    /* Delete (removes 'b') */
    res_t res = ik_workspace_delete(workspace);
    ck_assert(is_ok(&res));

    /* Verify cursor stays at byte 1, grapheme 1 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor after clear */
START_TEST(test_workspace_cursor_after_clear)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "hello" */
    ik_workspace_insert_codepoint(workspace, 'h');
    ik_workspace_insert_codepoint(workspace, 'e');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'o');

    /* Clear */
    ik_workspace_clear(workspace);

    /* Verify cursor reset to 0,0 */
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    res_t res = ik_workspace_get_cursor_position(workspace, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST

static Suite *workspace_cursor_sync_suite(void)
{
    Suite *s = suite_create("Workspace Cursor Sync");
    TCase *tc_core = tcase_create("Core");

    /* Normal tests */
    tcase_add_test(tc_core, test_workspace_cursor_initialized);
    tcase_add_test(tc_core, test_workspace_cursor_after_insert_ascii);
    tcase_add_test(tc_core, test_workspace_cursor_after_insert_utf8);
    tcase_add_test(tc_core, test_workspace_cursor_after_newline);
    tcase_add_test(tc_core, test_workspace_cursor_after_backspace);
    tcase_add_test(tc_core, test_workspace_cursor_after_backspace_utf8);
    tcase_add_test(tc_core, test_workspace_cursor_after_delete);
    tcase_add_test(tc_core, test_workspace_cursor_after_clear);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = workspace_cursor_sync_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
