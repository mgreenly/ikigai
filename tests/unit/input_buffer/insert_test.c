/**
 * @file insert_test.c
 * @brief Unit tests for input_buffer insert operations
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input_buffer/core.h"
#include "../../test_utils_helper.h"

/* Test: Insert ASCII character */
START_TEST(test_insert_ascii) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert 'a' */
    res_t res = ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ck_assert(is_ok(&res));

    /* Verify text is "a" */
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 1);
    ck_assert_mem_eq(text, "a", 1);

    /* Verify cursor at end */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 1);

    /* Insert 'b' */
    res = ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ck_assert(is_ok(&res));

    /* Verify text is "ab" */
    text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_mem_eq(text, "ab", 2);

    /* Verify cursor at end */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Insert UTF-8 characters */
START_TEST(test_insert_utf8) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert é (U+00E9) - 2-byte UTF-8 sequence */
    res_t res = ik_input_buffer_insert_codepoint(input_buffer, 0x00E9);
    ck_assert(is_ok(&res));

    /* Verify correct UTF-8 encoding: C3 A9 */
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_uint_eq((uint8_t)text[0], 0xC3);
    ck_assert_uint_eq((uint8_t)text[1], 0xA9);
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 2);

    /* Insert 🎉 (U+1F389) - 4-byte UTF-8 sequence */
    res = ik_input_buffer_insert_codepoint(input_buffer, 0x1F389);
    ck_assert(is_ok(&res));

    /* Verify correct UTF-8 encoding: F0 9F 8E 89 */
    text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 6);
    ck_assert_uint_eq((uint8_t)text[2], 0xF0);
    ck_assert_uint_eq((uint8_t)text[3], 0x9F);
    ck_assert_uint_eq((uint8_t)text[4], 0x8E);
    ck_assert_uint_eq((uint8_t)text[5], 0x89);
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 6);

    talloc_free(ctx);
}

END_TEST
/* Test: Insert 3-byte UTF-8 character */
START_TEST(test_insert_utf8_3byte) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert ☃ (U+2603) - 3-byte UTF-8 sequence */
    res_t res = ik_input_buffer_insert_codepoint(input_buffer, 0x2603);
    ck_assert(is_ok(&res));

    /* Verify correct UTF-8 encoding: E2 98 83 */
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 3);
    ck_assert_uint_eq((uint8_t)text[0], 0xE2);
    ck_assert_uint_eq((uint8_t)text[1], 0x98);
    ck_assert_uint_eq((uint8_t)text[2], 0x83);
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 3);

    talloc_free(ctx);
}

END_TEST
/* Test: Insert in middle of text */
START_TEST(test_insert_middle) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "ab" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');

    /* Move cursor to position 1 (between 'a' and 'b') */
    input_buffer->cursor_byte_offset = 1;

    /* Insert 'x' */
    res_t res = ik_input_buffer_insert_codepoint(input_buffer, 'x');
    ck_assert(is_ok(&res));

    /* Verify text is "axb" */
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 3);
    ck_assert_mem_eq(text, "axb", 3);

    /* Verify cursor at position 2 (after 'x') */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Insert invalid codepoint */
START_TEST(test_insert_invalid_codepoint) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Try to insert codepoint > U+10FFFF (invalid) */
    res_t res = ik_input_buffer_insert_codepoint(input_buffer, 0x110000);
    ck_assert(is_err(&res));

    /* Verify text buffer is still empty */
    size_t len = 0;
    (void)ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 0);

    /* Verify cursor still at 0 */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Insert newline */
START_TEST(test_insert_newline) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "hello" */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');

    /* Insert newline */
    res_t res = ik_input_buffer_insert_newline(input_buffer);
    ck_assert(is_ok(&res));

    /* Insert "world" */
    ik_input_buffer_insert_codepoint(input_buffer, 'w');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');
    ik_input_buffer_insert_codepoint(input_buffer, 'r');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'd');

    /* Verify text is "hello\nworld" */
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 11);
    ck_assert_mem_eq(text, "hello\nworld", 11);

    /* Verify cursor at end */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 11);

    talloc_free(ctx);
}

END_TEST

static Suite *input_buffer_insert_suite(void)
{
    Suite *s = suite_create("Input Buffer Insert");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Normal tests */
    tcase_add_test(tc_core, test_insert_ascii);
    tcase_add_test(tc_core, test_insert_utf8);
    tcase_add_test(tc_core, test_insert_utf8_3byte);
    tcase_add_test(tc_core, test_insert_middle);
    tcase_add_test(tc_core, test_insert_invalid_codepoint);
    tcase_add_test(tc_core, test_insert_newline);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_insert_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/input_buffer/insert_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
