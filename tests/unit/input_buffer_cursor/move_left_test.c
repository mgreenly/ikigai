/**
 * @file move_left_test.c
 * @brief Unit tests for cursor move_left functionality
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input_buffer/cursor.h"
#include "../../test_utils.h"

// Test move left with ASCII text
START_TEST(test_cursor_move_left_ascii) {
    void *ctx = talloc_new(NULL);
    const char *text = "abc";
    size_t text_len = 3;

    // Create cursor and set to end
    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);
    ik_input_buffer_cursor_set_position(cursor, text, text_len, 3);

    // Move left once: should move to byte 2, grapheme 2
    ik_input_buffer_cursor_move_left(cursor, text, text_len);
    ck_assert_uint_eq(cursor->byte_offset, 2);
    ck_assert_uint_eq(cursor->grapheme_offset, 2);

    // Move left again: should move to byte 1, grapheme 1
    ik_input_buffer_cursor_move_left(cursor, text, text_len);
    ck_assert_uint_eq(cursor->byte_offset, 1);
    ck_assert_uint_eq(cursor->grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
// Test move left with UTF-8 multi-byte character
START_TEST(test_cursor_move_left_utf8) {
    void *ctx = talloc_new(NULL);
    const char *text = "a\xC3\xA9" "b";  // "aéb" (4 bytes: a + C3 A9 + b)
    size_t text_len = 4;

    // Create cursor and set to end (byte 4)
    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);
    ik_input_buffer_cursor_set_position(cursor, text, text_len, 4);
    ck_assert_uint_eq(cursor->grapheme_offset, 3);

    // Move left once: should move to byte 3 (after é), grapheme 2
    ik_input_buffer_cursor_move_left(cursor, text, text_len);
    ck_assert_uint_eq(cursor->byte_offset, 3);
    ck_assert_uint_eq(cursor->grapheme_offset, 2);

    // Move left again: should skip both bytes of é, move to byte 1, grapheme 1
    ik_input_buffer_cursor_move_left(cursor, text, text_len);
    ck_assert_uint_eq(cursor->byte_offset, 1);
    ck_assert_uint_eq(cursor->grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
// Test move left with 4-byte emoji
START_TEST(test_cursor_move_left_emoji) {
    void *ctx = talloc_new(NULL);
    const char *text = "a\xF0\x9F\x8E\x89";  // "a🎉" (5 bytes: a + F0 9F 8E 89)
    size_t text_len = 5;

    // Create cursor and set to end (byte 5)
    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);
    ik_input_buffer_cursor_set_position(cursor, text, text_len, 5);
    ck_assert_uint_eq(cursor->grapheme_offset, 2);

    // Move left once: should skip all 4 bytes of 🎉, move to byte 1, grapheme 1
    ik_input_buffer_cursor_move_left(cursor, text, text_len);
    ck_assert_uint_eq(cursor->byte_offset, 1);
    ck_assert_uint_eq(cursor->grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
// Test move left with combining character
START_TEST(test_cursor_move_left_combining) {
    void *ctx = talloc_new(NULL);
    // e + combining acute accent (U+0301) = é
    const char *text = "e\xCC\x81";  // e + combining acute (3 bytes)
    size_t text_len = 3;

    // Create cursor and set to end (byte 3)
    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);
    ik_input_buffer_cursor_set_position(cursor, text, text_len, 3);
    ck_assert_uint_eq(cursor->grapheme_offset, 1);  // e+combining = 1 grapheme

    // Move left once: should skip both e and combining, move to byte 0, grapheme 0
    ik_input_buffer_cursor_move_left(cursor, text, text_len);
    ck_assert_uint_eq(cursor->byte_offset, 0);
    ck_assert_uint_eq(cursor->grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test move left at start (no-op)
START_TEST(test_cursor_move_left_at_start) {
    void *ctx = talloc_new(NULL);
    const char *text = "abc";
    size_t text_len = 3;

    // Create cursor (starts at position 0)
    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);

    // Move left at start: should be no-op
    ik_input_buffer_cursor_move_left(cursor, text, text_len);
    ck_assert_uint_eq(cursor->byte_offset, 0);
    ck_assert_uint_eq(cursor->grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test NULL cursor parameter assertion
START_TEST(test_cursor_move_left_null_cursor) {
    const char *text = "hello";

    /* cursor cannot be NULL - should abort */
    ik_input_buffer_cursor_move_left(NULL, text, 5);
}

END_TEST
// Test NULL text parameter assertion
START_TEST(test_cursor_move_left_null_text) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);

    /* text cannot be NULL - should abort */
    ik_input_buffer_cursor_move_left(cursor, NULL, 5);

    talloc_free(ctx);
}

END_TEST
#endif

// Test suite
static Suite *cursor_move_left_suite(void)
{
    Suite *s = suite_create("CursorMoveLeft");

    TCase *tc_move_left = tcase_create("MoveLeft");
    tcase_set_timeout(tc_move_left, 30);
    tcase_set_timeout(tc_move_left, 30);
    tcase_set_timeout(tc_move_left, 30);
    tcase_set_timeout(tc_move_left, 30);
    tcase_add_test(tc_move_left, test_cursor_move_left_ascii);
    tcase_add_test(tc_move_left, test_cursor_move_left_utf8);
    tcase_add_test(tc_move_left, test_cursor_move_left_emoji);
    tcase_add_test(tc_move_left, test_cursor_move_left_combining);
    tcase_add_test(tc_move_left, test_cursor_move_left_at_start);
    suite_add_tcase(s, tc_move_left);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_cursor_move_left_null_cursor, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_move_left_null_text, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    Suite *s = cursor_move_left_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
