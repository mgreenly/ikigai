/**
 * @file workspace_test.c
 * @brief Unit tests for workspace text buffer
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../src/workspace.h"
#include "../test_utils.h"

/* Test: Create workspace */
START_TEST(test_workspace_create) {
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    /* Create workspace */
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(workspace);

    /* Verify text buffer is empty */
    char *text = NULL;
    size_t len = 0;
    res = ik_workspace_get_text(workspace, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 0);

    /* Verify cursor at position 0 */
    ck_assert_uint_eq(workspace->cursor_byte_offset, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: Create workspace - OOM scenarios */
START_TEST(test_workspace_create_oom)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    /* Test OOM during workspace allocation */
    oom_test_fail_next_alloc();
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(workspace);
    oom_test_reset();

    /* Test OOM during byte array allocation (after workspace alloc succeeds) */
    //  Call 1: workspace struct allocation (succeeds)
    //  Call 2: array struct allocation (fails here)
    oom_test_fail_after_n_calls(2);
    res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(workspace);
    oom_test_reset();

    talloc_free(ctx);
}

END_TEST
/* Test: NULL parameter assertions */
START_TEST(test_workspace_create_null_param)
{
    void *ctx = talloc_new(NULL);

    /* workspace_out cannot be NULL - should abort */
    ik_workspace_create(ctx, NULL);

    talloc_free(ctx);
}

END_TEST
/* Test: Get text NULL parameter assertions */
START_TEST(test_workspace_get_text_null_params)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    char *text = NULL;
    size_t len = 0;

    ik_workspace_create(ctx, &workspace);

    /* workspace cannot be NULL */
    ik_workspace_get_text(NULL, &text, &len);

    talloc_free(ctx);
}

END_TEST START_TEST(test_workspace_get_text_null_text_out)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    size_t len = 0;

    ik_workspace_create(ctx, &workspace);

    /* text_out cannot be NULL */
    ik_workspace_get_text(workspace, NULL, &len);

    talloc_free(ctx);
}

END_TEST START_TEST(test_workspace_get_text_null_len_out)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;
    char *text = NULL;

    ik_workspace_create(ctx, &workspace);

    /* len_out cannot be NULL */
    ik_workspace_get_text(workspace, &text, NULL);

    talloc_free(ctx);
}

END_TEST
/* Test: Clear workspace */
START_TEST(test_workspace_clear)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Manually add some data to test clearing */
    const uint8_t test_data[] = {'h', 'e', 'l', 'l', 'o'};
    for (size_t i = 0; i < 5; i++) {
        ik_byte_array_append(workspace->text, test_data[i]);
    }
    workspace->cursor_byte_offset = 3;

    /* Clear the workspace */
    ik_workspace_clear(workspace);

    /* Verify empty */
    char *text = NULL;
    size_t len = 0;
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 0);

    /* Verify cursor at 0 */
    ck_assert_uint_eq(workspace->cursor_byte_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Clear NULL parameter assertion */
START_TEST(test_workspace_clear_null_param)
{
    /* workspace cannot be NULL - should abort */
    ik_workspace_clear(NULL);
}

END_TEST
/* Test: Insert ASCII character */
START_TEST(test_workspace_insert_ascii)
{
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
/* Test: Insert codepoint - OOM during byte array insert */
START_TEST(test_workspace_insert_codepoint_oom)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Trigger OOM during byte array insert
     * The byte array starts with capacity 64 (from workspace_create),
     * so inserting one byte doesn't trigger a grow/realloc.
     * We need to force the realloc path by filling the array first.
     */

    /* Fill the array to capacity (64 bytes) */
    for (size_t i = 0; i < 64; i++) {
        ik_workspace_insert_codepoint(workspace, 'x');
    }

    /* Now the next insert will trigger a realloc (grow to 128) */
    /* Fail the realloc */
    oom_test_fail_next_alloc();
    res_t res = ik_workspace_insert_codepoint(workspace, 'a');
    ck_assert(is_err(&res));
    oom_test_reset();

    /* Verify workspace is still consistent (should have 64 bytes) */
    char *text = NULL;
    size_t len = 0;
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 64);

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
/* Test: Insert newline - OOM during byte array insert */
START_TEST(test_workspace_insert_newline_oom)
{
    void *ctx = talloc_new(NULL);
    ik_workspace_t *workspace = NULL;

    ik_workspace_create(ctx, &workspace);

    /* Fill the array to capacity (64 bytes) */
    for (size_t i = 0; i < 64; i++) {
        ik_workspace_insert_codepoint(workspace, 'x');
    }

    /* Now the next insert will trigger a realloc (grow to 128) */
    /* Fail the realloc */
    oom_test_fail_next_alloc();
    res_t res = ik_workspace_insert_newline(workspace);
    ck_assert(is_err(&res));
    oom_test_reset();

    /* Verify workspace is still consistent (should have 64 bytes) */
    char *text = NULL;
    size_t len = 0;
    ik_workspace_get_text(workspace, &text, &len);
    ck_assert_uint_eq(len, 64);

    talloc_free(ctx);
}
END_TEST

static Suite *workspace_suite(void)
{
    Suite *s = suite_create("Workspace");
    TCase *tc_core = tcase_create("Core");

    /* Normal tests */
    tcase_add_test(tc_core, test_workspace_create);
    tcase_add_test(tc_core, test_workspace_create_oom);
    tcase_add_test(tc_core, test_workspace_clear);
    tcase_add_test(tc_core, test_workspace_insert_ascii);
    tcase_add_test(tc_core, test_workspace_insert_utf8);
    tcase_add_test(tc_core, test_workspace_insert_utf8_3byte);
    tcase_add_test(tc_core, test_workspace_insert_middle);
    tcase_add_test(tc_core, test_workspace_insert_invalid_codepoint);
    tcase_add_test(tc_core, test_workspace_insert_codepoint_oom);
    tcase_add_test(tc_core, test_workspace_insert_newline);
    tcase_add_test(tc_core, test_workspace_insert_newline_oom);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_core, test_workspace_create_null_param, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_workspace_get_text_null_params, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_workspace_get_text_null_text_out, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_workspace_get_text_null_len_out, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_workspace_clear_null_param, SIGABRT);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = workspace_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
