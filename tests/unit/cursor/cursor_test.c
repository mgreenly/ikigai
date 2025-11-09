/**
 * @file cursor_test.c
 * @brief Unit tests for cursor module
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/cursor.h"
#include "../../test_utils.h"

// Test cursor creation
START_TEST(test_cursor_create) {
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;

    res_t result = ik_cursor_create(ctx, &cursor);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(cursor);
    ck_assert_uint_eq(cursor->byte_offset, 0);
    ck_assert_uint_eq(cursor->grapheme_offset, 0);

    talloc_free(ctx);
}
END_TEST
// Test cursor creation OOM
START_TEST(test_cursor_create_oom)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;

    /* Test OOM during cursor allocation */
    oom_test_fail_next_alloc();
    res_t result = ik_cursor_create(ctx, &cursor);
    ck_assert(is_err(&result));
    ck_assert_ptr_null(cursor);
    oom_test_reset();

    talloc_free(ctx);
}

END_TEST
// Test NULL parent parameter assertion
START_TEST(test_cursor_create_null_parent)
{
    ik_cursor_t *cursor = NULL;

    /* parent cannot be NULL - should abort */
    ik_cursor_create(NULL, &cursor);
}

END_TEST
// Test NULL out parameter assertion
START_TEST(test_cursor_create_null_out)
{
    void *ctx = talloc_new(NULL);

    /* cursor_out cannot be NULL - should abort */
    ik_cursor_create(ctx, NULL);

    talloc_free(ctx);
}

END_TEST
// Test set position with ASCII text
START_TEST(test_cursor_set_position_ascii)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    const char *text = "hello";
    size_t text_len = 5;

    // Create cursor
    res_t result = ik_cursor_create(ctx, &cursor);
    ck_assert(is_ok(&result));

    // Set position to byte 3 (after "hel")
    result = ik_cursor_set_position(cursor, text, text_len, 3);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->byte_offset, 3);
    ck_assert_uint_eq(cursor->grapheme_offset, 3);  // 3 ASCII chars = 3 graphemes

    talloc_free(ctx);
}

END_TEST
// Test set position with UTF-8 multi-byte character
START_TEST(test_cursor_set_position_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    const char *text = "a\xC3\xA9" "b";  // "aéb" (4 bytes: a + C3 A9 + b)
    size_t text_len = 4;

    // Create cursor
    res_t result = ik_cursor_create(ctx, &cursor);
    ck_assert(is_ok(&result));

    // Set position to byte 3 (after é)
    result = ik_cursor_set_position(cursor, text, text_len, 3);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->byte_offset, 3);
    ck_assert_uint_eq(cursor->grapheme_offset, 2);  // a + é = 2 graphemes

    talloc_free(ctx);
}

END_TEST
// Test set position with 4-byte emoji
START_TEST(test_cursor_set_position_emoji)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    const char *text = "a\xF0\x9F\x8E\x89" "b";  // "a🎉b" (6 bytes: a + F0 9F 8E 89 + b)
    size_t text_len = 6;

    // Create cursor
    res_t result = ik_cursor_create(ctx, &cursor);
    ck_assert(is_ok(&result));

    // Set position to byte 5 (after 🎉)
    result = ik_cursor_set_position(cursor, text, text_len, 5);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->byte_offset, 5);
    ck_assert_uint_eq(cursor->grapheme_offset, 2);  // a + 🎉 = 2 graphemes

    talloc_free(ctx);
}

END_TEST
// Test set position with invalid UTF-8
START_TEST(test_cursor_set_position_invalid_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    const char *text = "a\xFF" "b";  // Invalid UTF-8: 0xFF is not a valid UTF-8 byte
    size_t text_len = 3;

    // Create cursor
    res_t result = ik_cursor_create(ctx, &cursor);
    ck_assert(is_ok(&result));

    // Set position to byte 2 (tries to iterate through invalid UTF-8)
    result = ik_cursor_set_position(cursor, text, text_len, 2);
    ck_assert(is_err(&result));

    talloc_free(ctx);
}

END_TEST
// Test NULL cursor parameter assertion
START_TEST(test_cursor_set_position_null_cursor)
{
    const char *text = "hello";

    /* cursor cannot be NULL - should abort */
    ik_cursor_set_position(NULL, text, 5, 3);
}

END_TEST
// Test NULL text parameter assertion
START_TEST(test_cursor_set_position_null_text)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    ik_cursor_create(ctx, &cursor);

    /* text cannot be NULL - should abort */
    ik_cursor_set_position(cursor, NULL, 5, 3);

    talloc_free(ctx);
}

END_TEST
// Test byte_offset > text_len assertion
START_TEST(test_cursor_set_position_offset_too_large)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    const char *text = "hello";
    ik_cursor_create(ctx, &cursor);

    /* byte_offset must be <= text_len - should abort */
    ik_cursor_set_position(cursor, text, 5, 10);

    talloc_free(ctx);
}

END_TEST
// Test move left with ASCII text
START_TEST(test_cursor_move_left_ascii)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    const char *text = "abc";
    size_t text_len = 3;

    // Create cursor and set to end
    res_t result = ik_cursor_create(ctx, &cursor);
    ck_assert(is_ok(&result));
    result = ik_cursor_set_position(cursor, text, text_len, 3);
    ck_assert(is_ok(&result));

    // Move left once: should move to byte 2, grapheme 2
    result = ik_cursor_move_left(cursor, text, text_len);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->byte_offset, 2);
    ck_assert_uint_eq(cursor->grapheme_offset, 2);

    // Move left again: should move to byte 1, grapheme 1
    result = ik_cursor_move_left(cursor, text, text_len);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->byte_offset, 1);
    ck_assert_uint_eq(cursor->grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
// Test move left with UTF-8 multi-byte character
START_TEST(test_cursor_move_left_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    const char *text = "a\xC3\xA9" "b";  // "aéb" (4 bytes: a + C3 A9 + b)
    size_t text_len = 4;

    // Create cursor and set to end (byte 4)
    res_t result = ik_cursor_create(ctx, &cursor);
    ck_assert(is_ok(&result));
    result = ik_cursor_set_position(cursor, text, text_len, 4);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->grapheme_offset, 3);

    // Move left once: should move to byte 3 (after é), grapheme 2
    result = ik_cursor_move_left(cursor, text, text_len);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->byte_offset, 3);
    ck_assert_uint_eq(cursor->grapheme_offset, 2);

    // Move left again: should skip both bytes of é, move to byte 1, grapheme 1
    result = ik_cursor_move_left(cursor, text, text_len);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->byte_offset, 1);
    ck_assert_uint_eq(cursor->grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
// Test move left with 4-byte emoji
START_TEST(test_cursor_move_left_emoji)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    const char *text = "a\xF0\x9F\x8E\x89";  // "a🎉" (5 bytes: a + F0 9F 8E 89)
    size_t text_len = 5;

    // Create cursor and set to end (byte 5)
    res_t result = ik_cursor_create(ctx, &cursor);
    ck_assert(is_ok(&result));
    result = ik_cursor_set_position(cursor, text, text_len, 5);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->grapheme_offset, 2);

    // Move left once: should skip all 4 bytes of 🎉, move to byte 1, grapheme 1
    result = ik_cursor_move_left(cursor, text, text_len);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->byte_offset, 1);
    ck_assert_uint_eq(cursor->grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
// Test move left with combining character
START_TEST(test_cursor_move_left_combining)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    // e + combining acute accent (U+0301) = é
    const char *text = "e\xCC\x81";  // e + combining acute (3 bytes)
    size_t text_len = 3;

    // Create cursor and set to end (byte 3)
    res_t result = ik_cursor_create(ctx, &cursor);
    ck_assert(is_ok(&result));
    result = ik_cursor_set_position(cursor, text, text_len, 3);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->grapheme_offset, 1);  // e+combining = 1 grapheme

    // Move left once: should skip both e and combining, move to byte 0, grapheme 0
    result = ik_cursor_move_left(cursor, text, text_len);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->byte_offset, 0);
    ck_assert_uint_eq(cursor->grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test move left at start (no-op)
START_TEST(test_cursor_move_left_at_start)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    const char *text = "abc";
    size_t text_len = 3;

    // Create cursor (starts at position 0)
    res_t result = ik_cursor_create(ctx, &cursor);
    ck_assert(is_ok(&result));

    // Move left at start: should be no-op
    result = ik_cursor_move_left(cursor, text, text_len);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(cursor->byte_offset, 0);
    ck_assert_uint_eq(cursor->grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test move left with invalid UTF-8
START_TEST(test_cursor_move_left_invalid_utf8)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    const char *text = "a\xFF" "b";  // Invalid UTF-8: 0xFF is not a valid UTF-8 byte
    size_t text_len = 3;

    // Create cursor
    res_t result = ik_cursor_create(ctx, &cursor);
    ck_assert(is_ok(&result));

    // Manually set cursor position to byte 2 (to avoid set_position which also validates)
    cursor->byte_offset = 2;
    cursor->grapheme_offset = 2;

    // Move left should fail due to invalid UTF-8
    result = ik_cursor_move_left(cursor, text, text_len);
    ck_assert(is_err(&result));

    talloc_free(ctx);
}

END_TEST
// Test NULL cursor parameter assertion
START_TEST(test_cursor_move_left_null_cursor)
{
    const char *text = "hello";

    /* cursor cannot be NULL - should abort */
    ik_cursor_move_left(NULL, text, 5);
}

END_TEST
// Test NULL text parameter assertion
START_TEST(test_cursor_move_left_null_text)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;
    ik_cursor_create(ctx, &cursor);

    /* text cannot be NULL - should abort */
    ik_cursor_move_left(cursor, NULL, 5);

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *cursor_suite(void)
{
    Suite *s = suite_create("Cursor");

    TCase *tc_create = tcase_create("Create");
    tcase_add_test(tc_create, test_cursor_create);
    tcase_add_test(tc_create, test_cursor_create_oom);
    suite_add_tcase(s, tc_create);

    TCase *tc_set_position = tcase_create("SetPosition");
    tcase_add_test(tc_set_position, test_cursor_set_position_ascii);
    tcase_add_test(tc_set_position, test_cursor_set_position_utf8);
    tcase_add_test(tc_set_position, test_cursor_set_position_emoji);
    tcase_add_test(tc_set_position, test_cursor_set_position_invalid_utf8);
    suite_add_tcase(s, tc_set_position);

    TCase *tc_move_left = tcase_create("MoveLeft");
    tcase_add_test(tc_move_left, test_cursor_move_left_ascii);
    tcase_add_test(tc_move_left, test_cursor_move_left_utf8);
    tcase_add_test(tc_move_left, test_cursor_move_left_emoji);
    tcase_add_test(tc_move_left, test_cursor_move_left_combining);
    tcase_add_test(tc_move_left, test_cursor_move_left_at_start);
    tcase_add_test(tc_move_left, test_cursor_move_left_invalid_utf8);
    suite_add_tcase(s, tc_move_left);

    TCase *tc_assertions = tcase_create("Assertions");
    tcase_add_test_raise_signal(tc_assertions, test_cursor_create_null_parent, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_create_null_out, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_set_position_null_cursor, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_set_position_null_text, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_set_position_offset_too_large, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_move_left_null_cursor, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_move_left_null_text, SIGABRT);
    suite_add_tcase(s, tc_assertions);

    return s;
}

int main(void)
{
    Suite *s = cursor_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
