/**
 * @file cursor_left_right_test.c
 * @brief Unit tests for input_buffer horizontal cursor movement operations
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input_buffer/core.h"
#include "../../test_utils_helper.h"

/* Test: Cursor left - ASCII */
START_TEST(test_cursor_left_ascii) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "abc" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_codepoint(input_buffer, 'c');

    /* Move left */
    res_t res = ik_input_buffer_cursor_left(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 2, grapheme 2 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 2);
    ck_assert_uint_eq(grapheme_offset, 2);

    /* Move left again */
    res = ik_input_buffer_cursor_left(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 1, grapheme 1 */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}
END_TEST
/* Test: Cursor left - UTF-8 */
START_TEST(test_cursor_left_utf8) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "a" + é (2 bytes) + "b" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 0x00E9);
    ik_input_buffer_insert_codepoint(input_buffer, 'b');

    /* Move left (skip 'b') */
    res_t res = ik_input_buffer_cursor_left(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 3 (after é), grapheme 2 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 3);
    ck_assert_uint_eq(grapheme_offset, 2);

    /* Move left (skip é - both bytes) */
    res = ik_input_buffer_cursor_left(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 1 (after 'a'), grapheme 1 */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor left at start - no-op */
START_TEST(test_cursor_left_at_start) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Move left at start - should be no-op */
    res_t res = ik_input_buffer_cursor_left(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor still at 0,0 */
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor right - ASCII */
START_TEST(test_cursor_right_ascii) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "abc" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_codepoint(input_buffer, 'c');

    /* Move to start */
    input_buffer->cursor_byte_offset = 0;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 0);

    /* Move right */
    res_t res = ik_input_buffer_cursor_right(input_buffer);
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
/* Test: Cursor right - UTF-8 */
START_TEST(test_cursor_right_utf8) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "a" + 🎉 (4 bytes) */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 0x1F389);

    /* Move to start */
    input_buffer->cursor_byte_offset = 0;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, 0);

    /* Move right (skip 'a') */
    res_t res = ik_input_buffer_cursor_right(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 1, grapheme 1 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    /* Move right (skip 🎉 - all 4 bytes) */
    res = ik_input_buffer_cursor_right(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor at byte 5 (1 + 4), grapheme 2 */
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 5);
    ck_assert_uint_eq(grapheme_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor right at end - no-op */
START_TEST(test_cursor_right_at_end) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "a" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');

    /* Move right at end - should be no-op */
    res_t res = ik_input_buffer_cursor_right(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify cursor still at byte 1, grapheme 1 */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buffer, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
START_TEST(test_cursor_left_null_input_buffer_asserts) {
    /* input_buffer cannot be NULL - should abort */
    ik_input_buffer_cursor_left(NULL);
}

END_TEST

START_TEST(test_cursor_right_null_input_buffer_asserts) {
    /* input_buffer cannot be NULL - should abort */
    ik_input_buffer_cursor_right(NULL);
}

END_TEST
#endif

static Suite *input_buffer_cursor_left_right_suite(void)
{
    Suite *s = suite_create("Input Buffer Cursor Left/Right");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Normal tests */
    tcase_add_test(tc_core, test_cursor_left_ascii);
    tcase_add_test(tc_core, test_cursor_left_utf8);
    tcase_add_test(tc_core, test_cursor_left_at_start);
    tcase_add_test(tc_core, test_cursor_right_ascii);
    tcase_add_test(tc_core, test_cursor_right_utf8);
    tcase_add_test(tc_core, test_cursor_right_at_end);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    /* Assertion tests */
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_cursor_left_null_input_buffer_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_right_null_input_buffer_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_cursor_left_right_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/input_buffer/cursor_left_right_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
