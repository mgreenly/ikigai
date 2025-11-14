/**
 * @file insert_test.c
 * @brief Unit tests for workspace insert operations
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/workspace.h"
#include "../../test_utils.h"

/* Test: Insert ASCII character */
START_TEST(test_workspace_insert_ascii) {
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert 'a' */
    res_t res = ik_workspace_insert_codepoint(workspace, 'a');
    ck_assert(is_ok(&res));

    /* Verify text is "a" */
    char *text = NULL;
    size_t len = 0;
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 1);
    ck_assert_mem_eq(text, "a", 1);

    /* Verify cursor at end */
    ck_assert_uint_eq(workspace->cursor_byte_offset, 1);

    /* Insert 'b' */
    res = ik_workspace_insert_codepoint(workspace, 'b');
    ck_assert(is_ok(&res));

    /* Verify text is "ab" */
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_mem_eq(text, "ab", 2);

    /* Verify cursor at end */
    ck_assert_uint_eq(workspace->cursor_byte_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Insert UTF-8 characters */
START_TEST(test_workspace_insert_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert é (U+00E9) - 2-byte UTF-8 sequence */
    res_t res = ik_workspace_insert_codepoint(workspace, 0x00E9);
    ck_assert(is_ok(&res));

    /* Verify correct UTF-8 encoding: C3 A9 */
    char *text = NULL;
    size_t len = 0;
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_uint_eq((uint8_t)text[0], 0xC3);
    ck_assert_uint_eq((uint8_t)text[1], 0xA9);
    ck_assert_uint_eq(workspace->cursor_byte_offset, 2);

    /* Insert 🎉 (U+1F389) - 4-byte UTF-8 sequence */
    res = ik_workspace_insert_codepoint(workspace, 0x1F389);
    ck_assert(is_ok(&res));

    /* Verify correct UTF-8 encoding: F0 9F 8E 89 */
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 6);
    ck_assert_uint_eq((uint8_t)text[2], 0xF0);
    ck_assert_uint_eq((uint8_t)text[3], 0x9F);
    ck_assert_uint_eq((uint8_t)text[4], 0x8E);
    ck_assert_uint_eq((uint8_t)text[5], 0x89);
    ck_assert_uint_eq(workspace->cursor_byte_offset, 6);

    talloc_free(ctx);
}

END_TEST
/* Test: Insert 3-byte UTF-8 character */
START_TEST(test_workspace_insert_utf8_3byte)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert ☃ (U+2603) - 3-byte UTF-8 sequence */
    res_t res = ik_workspace_insert_codepoint(workspace, 0x2603);
    ck_assert(is_ok(&res));

    /* Verify correct UTF-8 encoding: E2 98 83 */
    char *text = NULL;
    size_t len = 0;
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 3);
    ck_assert_uint_eq((uint8_t)text[0], 0xE2);
    ck_assert_uint_eq((uint8_t)text[1], 0x98);
    ck_assert_uint_eq((uint8_t)text[2], 0x83);
    ck_assert_uint_eq(workspace->cursor_byte_offset, 3);

    talloc_free(ctx);
}

END_TEST
/* Test: Insert in middle of text */
START_TEST(test_workspace_insert_middle)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Insert "ab" */
    ik_workspace_insert_codepoint(workspace, 'a');
    ik_workspace_insert_codepoint(workspace, 'b');

    /* Move cursor to position 1 (between 'a' and 'b') */
    workspace->cursor_byte_offset = 1;

    /* Insert 'x' */
    res_t res = ik_workspace_insert_codepoint(workspace, 'x');
    ck_assert(is_ok(&res));

    /* Verify text is "axb" */
    char *text = NULL;
    size_t len = 0;
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 3);
    ck_assert_mem_eq(text, "axb", 3);

    /* Verify cursor at position 2 (after 'x') */
    ck_assert_uint_eq(workspace->cursor_byte_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Insert invalid codepoint */
START_TEST(test_workspace_insert_invalid_codepoint)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Try to insert codepoint > U+10FFFF (invalid) */
    res_t res = ik_workspace_insert_codepoint(workspace, 0x110000);
    ck_assert(is_err(&res));

    /* Verify text buffer is still empty */
    char *text = NULL;
    size_t len = 0;
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 0);

    /* Verify cursor still at 0 */
    ck_assert_uint_eq(workspace->cursor_byte_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Insert newline */
START_TEST(test_workspace_insert_newline)
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

    /* Insert newline */
    res_t res = ik_workspace_insert_newline(workspace);
    ck_assert(is_ok(&res));

    /* Insert "world" */
    ik_workspace_insert_codepoint(workspace, 'w');
    ik_workspace_insert_codepoint(workspace, 'o');
    ik_workspace_insert_codepoint(workspace, 'r');
    ik_workspace_insert_codepoint(workspace, 'l');
    ik_workspace_insert_codepoint(workspace, 'd');

    /* Verify text is "hello\nworld" */
    char *text = NULL;
    size_t len = 0;
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 11);
    ck_assert_mem_eq(text, "hello\nworld", 11);

    /* Verify cursor at end */
    ck_assert_uint_eq(workspace->cursor_byte_offset, 11);

    talloc_free(ctx);
}

END_TEST

static Suite *workspace_insert_suite(void)
{
    Suite *s = suite_create("Workspace Insert");
    TCase *tc_core = tcase_create("Core");

    /* Normal tests */
    tcase_add_test(tc_core, test_workspace_insert_ascii);
    tcase_add_test(tc_core, test_workspace_insert_utf8);
    tcase_add_test(tc_core, test_workspace_insert_utf8_3byte);
    tcase_add_test(tc_core, test_workspace_insert_middle);
    tcase_add_test(tc_core, test_workspace_insert_invalid_codepoint);
    tcase_add_test(tc_core, test_workspace_insert_newline);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = workspace_insert_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
