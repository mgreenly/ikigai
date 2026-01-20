/**
 * @file cursor_test.c
 * @brief Unit tests for cursor module (create and set_position)
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input_buffer/cursor.h"
#include "../../test_utils_helper.h"

// Test cursor creation
START_TEST(test_cursor_create) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);

    ck_assert_ptr_nonnull(cursor);
    ck_assert_uint_eq(cursor->byte_offset, 0);
    ck_assert_uint_eq(cursor->grapheme_offset, 0);

    talloc_free(ctx);
}
END_TEST
// Test set position with ASCII text
START_TEST(test_cursor_set_position_ascii) {
    void *ctx = talloc_new(NULL);
    const char *text = "hello";
    size_t text_len = 5;

    // Create cursor
    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);

    // Set position to byte 3 (after "hel")
    ik_input_buffer_cursor_set_position(cursor, text, text_len, 3);
    ck_assert_uint_eq(cursor->byte_offset, 3);
    ck_assert_uint_eq(cursor->grapheme_offset, 3);  // 3 ASCII chars = 3 graphemes

    talloc_free(ctx);
}

END_TEST
// Test set position with UTF-8 multi-byte character
START_TEST(test_cursor_set_position_utf8) {
    void *ctx = talloc_new(NULL);
    const char *text = "a\xC3\xA9" "b";  // "aéb" (4 bytes: a + C3 A9 + b)
    size_t text_len = 4;

    // Create cursor
    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);

    // Set position to byte 3 (after é)
    ik_input_buffer_cursor_set_position(cursor, text, text_len, 3);
    ck_assert_uint_eq(cursor->byte_offset, 3);
    ck_assert_uint_eq(cursor->grapheme_offset, 2);  // a + é = 2 graphemes

    talloc_free(ctx);
}

END_TEST
// Test set position with 4-byte emoji
START_TEST(test_cursor_set_position_emoji) {
    void *ctx = talloc_new(NULL);
    const char *text = "a\xF0\x9F\x8E\x89" "b";  // "a🎉b" (6 bytes: a + F0 9F 8E 89 + b)
    size_t text_len = 6;

    // Create cursor
    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);

    // Set position to byte 5 (after 🎉)
    ik_input_buffer_cursor_set_position(cursor, text, text_len, 5);
    ck_assert_uint_eq(cursor->byte_offset, 5);
    ck_assert_uint_eq(cursor->grapheme_offset, 2);  // a + 🎉 = 2 graphemes

    talloc_free(ctx);
}

END_TEST
// Test get_position
START_TEST(test_cursor_get_position) {
    void *ctx = talloc_new(NULL);
    const char *text = "hello";
    size_t text_len = 5;

    // Create cursor
    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);

    // Set position
    ik_input_buffer_cursor_set_position(cursor, text, text_len, 3);

    // Get position
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    ik_input_buffer_cursor_get_position(cursor, &byte_offset, &grapheme_offset);

    ck_assert_uint_eq(byte_offset, 3);
    ck_assert_uint_eq(grapheme_offset, 3);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test NULL parent parameter assertion
START_TEST(test_cursor_create_null_parent) {
    /* parent cannot be NULL - should abort */
    ik_input_buffer_cursor_create(NULL);
}

END_TEST
// Test NULL cursor parameter assertion
START_TEST(test_cursor_set_position_null_cursor) {
    const char *text = "hello";

    /* cursor cannot be NULL - should abort */
    ik_input_buffer_cursor_set_position(NULL, text, 5, 3);
}

END_TEST
// Test NULL text parameter assertion
START_TEST(test_cursor_set_position_null_text) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);

    /* text cannot be NULL - should abort */
    ik_input_buffer_cursor_set_position(cursor, NULL, 5, 3);

    talloc_free(ctx);
}

END_TEST
// Test byte_offset > text_len assertion
START_TEST(test_cursor_set_position_offset_too_large) {
    void *ctx = talloc_new(NULL);
    const char *text = "hello";
    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);

    /* byte_offset must be <= text_len - should abort */
    ik_input_buffer_cursor_set_position(cursor, text, 5, 10);

    talloc_free(ctx);
}

END_TEST
// Test get_position NULL cursor assertion
START_TEST(test_cursor_get_position_null_cursor) {
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;

    /* cursor cannot be NULL - should abort */
    ik_input_buffer_cursor_get_position(NULL, &byte_offset, &grapheme_offset);
}

END_TEST
// Test get_position NULL byte_offset_out assertion
START_TEST(test_cursor_get_position_null_byte_out) {
    void *ctx = talloc_new(NULL);
    size_t grapheme_offset = 0;

    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);

    /* byte_offset_out cannot be NULL - should abort */
    ik_input_buffer_cursor_get_position(cursor, NULL, &grapheme_offset);

    talloc_free(ctx);
}

END_TEST
// Test get_position NULL grapheme_offset_out assertion
START_TEST(test_cursor_get_position_null_grapheme_out) {
    void *ctx = talloc_new(NULL);
    size_t byte_offset = 0;

    ik_input_buffer_cursor_t *cursor = ik_input_buffer_cursor_create(ctx);

    /* grapheme_offset_out cannot be NULL - should abort */
    ik_input_buffer_cursor_get_position(cursor, &byte_offset, NULL);

    talloc_free(ctx);
}

END_TEST
#endif

// Test suite
static Suite *cursor_suite(void)
{
    Suite *s = suite_create("Cursor");

    TCase *tc_create = tcase_create("Create");
    tcase_set_timeout(tc_create, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_create, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_create, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_create, IK_TEST_TIMEOUT);
    tcase_add_test(tc_create, test_cursor_create);
    suite_add_tcase(s, tc_create);

    TCase *tc_set_position = tcase_create("SetPosition");
    tcase_set_timeout(tc_set_position, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_set_position, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_set_position, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_set_position, IK_TEST_TIMEOUT);
    tcase_add_test(tc_set_position, test_cursor_set_position_ascii);
    tcase_add_test(tc_set_position, test_cursor_set_position_utf8);
    tcase_add_test(tc_set_position, test_cursor_set_position_emoji);
    suite_add_tcase(s, tc_set_position);

    TCase *tc_get_position = tcase_create("GetPosition");
    tcase_set_timeout(tc_get_position, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_get_position, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_get_position, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_get_position, IK_TEST_TIMEOUT);
    tcase_add_test(tc_get_position, test_cursor_get_position);
    suite_add_tcase(s, tc_get_position);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_cursor_create_null_parent, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_set_position_null_cursor, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_set_position_null_text, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_set_position_offset_too_large, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_get_position_null_cursor, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_get_position_null_byte_out, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_get_position_null_grapheme_out, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

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
