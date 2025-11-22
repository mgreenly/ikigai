/**
 * @file cursor_sync_test.c
 * @brief Unit tests for input_buffer cursor synchronization with text operations
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

/* Test: Input buffer cursor initialized to 0,0 */
START_TEST(test_cursor_initialized) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    /* Create input_buffer */
    input_buffer = ik_input_buffer_create(ctx);

    /* Verify cursor at position 0,0 */
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: Cursor advances after inserting ASCII */
START_TEST(test_cursor_after_insert_ascii)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert 'a' */
    res_t res = ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 1, grapheme 1 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    /* Insert 'b' */
    res = ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 2, grapheme 2 */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 2);
    ck_assert_uint_eq(grapheme_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor advances correctly for UTF-8 */
START_TEST(test_cursor_after_insert_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert 'a' */
    res_t res = ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ck_assert(is_ok(&res));

    /* Insert é (U+00E9, 2 bytes) */
    res = ik_input_buffer_insert_codepoint(input_buffer, 0x00E9);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 3 (1 + 2), grapheme 2 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 3);
    ck_assert_uint_eq(grapheme_offset, 2);

    /* Insert 🎉 (U+1F389, 4 bytes) */
    res = ik_input_buffer_insert_codepoint(input_buffer, 0x1F389);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 7 (3 + 4), grapheme 3 */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 7);
    ck_assert_uint_eq(grapheme_offset, 3);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor after newline insert */
START_TEST(test_cursor_after_newline)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "hi" */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');

    /* Insert newline */
    res_t res = ik_input_buffer_insert_newline(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 3, grapheme 3 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 3);
    ck_assert_uint_eq(grapheme_offset, 3);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor after backspace */
START_TEST(test_cursor_after_backspace)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "abc" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_codepoint(input_buffer, 'c');

    /* Backspace once */
    res_t res = ik_input_buffer_backspace(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 2, grapheme 2 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 2);
    ck_assert_uint_eq(grapheme_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor after backspace UTF-8 */
START_TEST(test_cursor_after_backspace_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "a" + é (2 bytes) */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 0x00E9);

    /* Backspace once (delete é) */
    res_t res = ik_input_buffer_backspace(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 1, grapheme 1 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor after delete (stays same) */
START_TEST(test_cursor_after_delete)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "abc" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_codepoint(input_buffer, 'c');

    /* Move cursor to middle (byte 1, after 'a') */
    input_buffer->cursor_byte_offset = 1;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 1);

    /* Delete (removes 'b') */
    res_t res = ik_input_buffer_delete(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor stays at byte 1, grapheme 1 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor after clear */
START_TEST(test_cursor_after_clear)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "hello" */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');

    /* Clear */
    ik_input_buffer_clear(input_buffer);

    /* Verify cursor reset to 0,0 */
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    res_t res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST

static Suite *input_buffer_cursor_sync_suite(void)
{
    Suite *s = suite_create("Input Buffer Cursor Sync");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    /* Normal tests */
    tcase_add_test(tc_core, test_cursor_initialized);
    tcase_add_test(tc_core, test_cursor_after_insert_ascii);
    tcase_add_test(tc_core, test_cursor_after_insert_utf8);
    tcase_add_test(tc_core, test_cursor_after_newline);
    tcase_add_test(tc_core, test_cursor_after_backspace);
    tcase_add_test(tc_core, test_cursor_after_backspace_utf8);
    tcase_add_test(tc_core, test_cursor_after_delete);
    tcase_add_test(tc_core, test_cursor_after_clear);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_cursor_sync_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
