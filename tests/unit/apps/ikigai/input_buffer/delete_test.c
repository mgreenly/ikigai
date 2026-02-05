/**
 * @file delete_test.c
 * @brief Unit tests for input_buffer delete and backspace operations
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "apps/ikigai/input_buffer/core.h"
#include "tests/helpers/test_utils_helper.h"

/* Test: Backspace ASCII character */
START_TEST(test_backspace_ascii) {
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

    /* Verify text is "ab" */
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_mem_eq(text, "ab", 2);

    /* Verify cursor at position 2 */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Backspace UTF-8 character */
START_TEST(test_backspace_utf8) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "a" + é (2 bytes) + "b" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 0x00E9); // é
    ik_input_buffer_insert_codepoint(input_buffer, 'b');

    /* Text should be "aéb" (4 bytes total: a + C3 A9 + b) */
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 4);

    /* Backspace once (should delete 'b') */
    res_t res = ik_input_buffer_backspace(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify text is "aé" (3 bytes) */
    text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 3);
    ck_assert_mem_eq(text, "a\xC3\xA9", 3);
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 3);

    /* Backspace again (should delete both bytes of é) */
    res = ik_input_buffer_backspace(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify text is "a" (1 byte) */
    text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 1);
    ck_assert_mem_eq(text, "a", 1);
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Backspace emoji (4-byte UTF-8) */
START_TEST(test_backspace_emoji) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert 🎉 (4 bytes: F0 9F 8E 89) */
    ik_input_buffer_insert_codepoint(input_buffer, 0x1F389);

    /* Verify text length */
    size_t len = 0;
    (void)ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 4);
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 4);

    /* Backspace once (should delete all 4 bytes) */
    res_t res = ik_input_buffer_backspace(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify text is empty */
    (void)ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 0);
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Backspace at start (no-op) */
START_TEST(test_backspace_at_start) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Cursor is at start (position 0) */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 0);

    /* Backspace should be a no-op */
    res_t res = ik_input_buffer_backspace(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify still empty */
    size_t len = 0;
    (void)ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 0);
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Delete ASCII character */
START_TEST(test_delete_ascii) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "abc" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_codepoint(input_buffer, 'c');

    /* Move cursor to position 0 (before 'a') */
    input_buffer->cursor_byte_offset = 0;

    /* Delete once (should delete 'a') */
    res_t res = ik_input_buffer_delete(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify text is "bc" */
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_mem_eq(text, "bc", 2);

    /* Verify cursor still at position 0 */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Delete UTF-8 character */
START_TEST(test_delete_utf8) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "a" + é (2 bytes) + "b" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 0x00E9); // é
    ik_input_buffer_insert_codepoint(input_buffer, 'b');

    /* Move cursor to position 1 (after 'a', before é) */
    input_buffer->cursor_byte_offset = 1;

    /* Delete once (should delete both bytes of é) */
    res_t res = ik_input_buffer_delete(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify text is "ab" */
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_mem_eq(text, "ab", 2);

    /* Verify cursor still at position 1 */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Delete 3-byte UTF-8 character */
START_TEST(test_delete_utf8_3byte) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "a" + ☃ (3 bytes) + "b" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 0x2603); // ☃ (snowman)
    ik_input_buffer_insert_codepoint(input_buffer, 'b');

    /* Move cursor to position 1 (after 'a', before ☃) */
    input_buffer->cursor_byte_offset = 1;

    /* Delete once (should delete all 3 bytes of ☃) */
    res_t res = ik_input_buffer_delete(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify text is "ab" */
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_mem_eq(text, "ab", 2);

    /* Verify cursor still at position 1 */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Delete emoji (4-byte UTF-8) */
START_TEST(test_delete_emoji) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert 🎉 (4 bytes: F0 9F 8E 89) */
    ik_input_buffer_insert_codepoint(input_buffer, 0x1F389);

    /* Move cursor to position 0 */
    input_buffer->cursor_byte_offset = 0;

    /* Delete once (should delete all 4 bytes) */
    res_t res = ik_input_buffer_delete(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify text is empty */
    size_t len = 0;
    (void)ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 0);

    /* Verify cursor still at position 0 */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Delete at end (no-op) */
START_TEST(test_delete_at_end) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Insert "abc" */
    ik_input_buffer_insert_codepoint(input_buffer, 'a');
    ik_input_buffer_insert_codepoint(input_buffer, 'b');
    ik_input_buffer_insert_codepoint(input_buffer, 'c');

    /* Cursor is at end (position 3) */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 3);

    /* Delete should be a no-op */
    res_t res = ik_input_buffer_delete(input_buffer);
    ck_assert(is_ok(&res));

    /* Verify text is still "abc" */
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 3);
    ck_assert_mem_eq(text, "abc", 3);

    /* Verify cursor still at position 3 */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 3);

    talloc_free(ctx);
}

END_TEST

static Suite *input_buffer_delete_suite(void)
{
    Suite *s = suite_create("Input Buffer Delete");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Normal tests */
    tcase_add_test(tc_core, test_backspace_ascii);
    tcase_add_test(tc_core, test_backspace_utf8);
    tcase_add_test(tc_core, test_backspace_emoji);
    tcase_add_test(tc_core, test_backspace_at_start);
    tcase_add_test(tc_core, test_delete_ascii);
    tcase_add_test(tc_core, test_delete_utf8);
    tcase_add_test(tc_core, test_delete_utf8_3byte);
    tcase_add_test(tc_core, test_delete_emoji);
    tcase_add_test(tc_core, test_delete_at_end);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_delete_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/input_buffer/delete_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
