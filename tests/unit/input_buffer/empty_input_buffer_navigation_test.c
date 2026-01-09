#include <check.h>
#include <talloc.h>
#include "../../../src/input_buffer/core.h"
#include "../../../src/error.h"

// Test: cursor_up on empty input_buffer should not crash
START_TEST(test_cursor_up_empty_input_buffer) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_input_buffer_t *ws = NULL;
    res_t result;
    ws = ik_input_buffer_create(ctx);
    ck_assert_ptr_nonnull(ws);

    // Empty input_buffer - no text inserted
    // Cursor should be at position 0

    // Arrow Up on empty input_buffer should be a no-op (not crash)
    result = ik_input_buffer_cursor_up(ws);
    ck_assert(is_ok(&result));

    // Verify cursor still at position 0
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    result = ik_input_buffer_get_cursor_position(ws, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}
END_TEST
// Test: cursor_down on empty input_buffer should not crash
START_TEST(test_cursor_down_empty_input_buffer) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_input_buffer_t *ws = NULL;
    res_t result;
    ws = ik_input_buffer_create(ctx);
    ck_assert_ptr_nonnull(ws);

    // Empty input_buffer - no text inserted
    // Cursor should be at position 0

    // Arrow Down on empty input_buffer should be a no-op (not crash)
    result = ik_input_buffer_cursor_down(ws);
    ck_assert(is_ok(&result));

    // Verify cursor still at position 0
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    result = ik_input_buffer_get_cursor_position(ws, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test: cursor_up after delete to empty should not crash
START_TEST(test_cursor_up_after_delete_to_empty) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_input_buffer_t *ws = NULL;
    res_t result;
    ws = ik_input_buffer_create(ctx);
    ck_assert_ptr_nonnull(ws);

    // Insert one character, then delete it
    result = ik_input_buffer_insert_codepoint(ws, 'a');
    ck_assert(is_ok(&result));

    result = ik_input_buffer_backspace(ws);
    ck_assert(is_ok(&result));

    // Now input_buffer is empty again (text != NULL but text_len == 0)
    // Arrow Up should not crash
    result = ik_input_buffer_cursor_up(ws);
    ck_assert(is_ok(&result));

    // Verify cursor still at position 0
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    result = ik_input_buffer_get_cursor_position(ws, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test: cursor_down after delete to empty should not crash
START_TEST(test_cursor_down_after_delete_to_empty) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_input_buffer_t *ws = NULL;
    res_t result;
    ws = ik_input_buffer_create(ctx);
    ck_assert_ptr_nonnull(ws);

    // Insert one character, then delete it
    result = ik_input_buffer_insert_codepoint(ws, 'a');
    ck_assert(is_ok(&result));

    result = ik_input_buffer_backspace(ws);
    ck_assert(is_ok(&result));

    // Now input_buffer is empty again (text != NULL but text_len == 0)
    // Arrow Down should not crash
    result = ik_input_buffer_cursor_down(ws);
    ck_assert(is_ok(&result));

    // Verify cursor still at position 0
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    result = ik_input_buffer_get_cursor_position(ws, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Ctrl+A on empty input_buffer should not crash
START_TEST(test_ctrl_a_empty_input_buffer) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_input_buffer_t *ws = NULL;
    res_t result;
    ws = ik_input_buffer_create(ctx);
    ck_assert_ptr_nonnull(ws);

    // Ctrl+A on empty input_buffer should be a no-op (not crash)
    result = ik_input_buffer_cursor_to_line_start(ws);
    ck_assert(is_ok(&result));

    // Verify cursor still at position 0
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    result = ik_input_buffer_get_cursor_position(ws, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Ctrl+E on empty input_buffer should not crash
START_TEST(test_ctrl_e_empty_input_buffer) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_input_buffer_t *ws = NULL;
    res_t result;
    ws = ik_input_buffer_create(ctx);
    ck_assert_ptr_nonnull(ws);

    // Ctrl+E on empty input_buffer should be a no-op (not crash)
    result = ik_input_buffer_cursor_to_line_end(ws);
    ck_assert(is_ok(&result));

    // Verify cursor still at position 0
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    result = ik_input_buffer_get_cursor_position(ws, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Ctrl+K on empty input_buffer should not crash
START_TEST(test_ctrl_k_empty_input_buffer) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_input_buffer_t *ws = NULL;
    res_t result;
    ws = ik_input_buffer_create(ctx);
    ck_assert_ptr_nonnull(ws);

    // Ctrl+K on empty input_buffer should be a no-op (not crash)
    result = ik_input_buffer_kill_to_line_end(ws);
    ck_assert(is_ok(&result));

    // Verify cursor still at position 0
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    result = ik_input_buffer_get_cursor_position(ws, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Ctrl+U on empty input_buffer should not crash
START_TEST(test_ctrl_u_empty_input_buffer) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_input_buffer_t *ws = NULL;
    res_t result;
    ws = ik_input_buffer_create(ctx);
    ck_assert_ptr_nonnull(ws);

    // Ctrl+U on empty input_buffer should be a no-op (not crash)
    result = ik_input_buffer_kill_line(ws);
    ck_assert(is_ok(&result));

    // Verify cursor still at position 0
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    result = ik_input_buffer_get_cursor_position(ws, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test: Ctrl+A after delete to empty should not crash
START_TEST(test_ctrl_a_after_delete_to_empty) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_input_buffer_t *ws = NULL;
    res_t result;
    ws = ik_input_buffer_create(ctx);

    // Insert and delete to create empty text buffer (text != NULL but text_len == 0)
    result = ik_input_buffer_insert_codepoint(ws, 'a');
    ck_assert(is_ok(&result));
    result = ik_input_buffer_backspace(ws);
    ck_assert(is_ok(&result));

    // Ctrl+A should not crash
    result = ik_input_buffer_cursor_to_line_start(ws);
    ck_assert(is_ok(&result));

    talloc_free(ctx);
}

END_TEST
// Test: Ctrl+E after delete to empty should not crash
START_TEST(test_ctrl_e_after_delete_to_empty) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_input_buffer_t *ws = NULL;
    res_t result;
    ws = ik_input_buffer_create(ctx);

    // Insert and delete to create empty text buffer
    result = ik_input_buffer_insert_codepoint(ws, 'a');
    ck_assert(is_ok(&result));
    result = ik_input_buffer_backspace(ws);
    ck_assert(is_ok(&result));

    // Ctrl+E should not crash
    result = ik_input_buffer_cursor_to_line_end(ws);
    ck_assert(is_ok(&result));

    talloc_free(ctx);
}

END_TEST
// Test: Ctrl+K after delete to empty should not crash
START_TEST(test_ctrl_k_after_delete_to_empty) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_input_buffer_t *ws = NULL;
    res_t result;
    ws = ik_input_buffer_create(ctx);

    // Insert and delete to create empty text buffer
    result = ik_input_buffer_insert_codepoint(ws, 'a');
    ck_assert(is_ok(&result));
    result = ik_input_buffer_backspace(ws);
    ck_assert(is_ok(&result));

    // Ctrl+K should not crash
    result = ik_input_buffer_kill_to_line_end(ws);
    ck_assert(is_ok(&result));

    talloc_free(ctx);
}

END_TEST
// Test: Ctrl+U after delete to empty should not crash
START_TEST(test_ctrl_u_after_delete_to_empty) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_input_buffer_t *ws = NULL;
    res_t result;
    ws = ik_input_buffer_create(ctx);

    // Insert and delete to create empty text buffer
    result = ik_input_buffer_insert_codepoint(ws, 'a');
    ck_assert(is_ok(&result));
    result = ik_input_buffer_backspace(ws);
    ck_assert(is_ok(&result));

    // Ctrl+U should not crash
    result = ik_input_buffer_kill_line(ws);
    ck_assert(is_ok(&result));

    talloc_free(ctx);
}

END_TEST

static Suite *empty_input_buffer_navigation_suite(void)
{
    Suite *s = suite_create("empty_input_buffer_navigation");

    TCase *tc_empty = tcase_create("empty_input_buffer");
    tcase_set_timeout(tc_empty, 30);
    tcase_set_timeout(tc_empty, 30);
    tcase_set_timeout(tc_empty, 30);
    tcase_set_timeout(tc_empty, 30);
    tcase_set_timeout(tc_empty, 30);
    tcase_add_test(tc_empty, test_cursor_up_empty_input_buffer);
    tcase_add_test(tc_empty, test_cursor_down_empty_input_buffer);
    tcase_add_test(tc_empty, test_cursor_up_after_delete_to_empty);
    tcase_add_test(tc_empty, test_cursor_down_after_delete_to_empty);
    tcase_add_test(tc_empty, test_ctrl_a_empty_input_buffer);
    tcase_add_test(tc_empty, test_ctrl_e_empty_input_buffer);
    tcase_add_test(tc_empty, test_ctrl_k_empty_input_buffer);
    tcase_add_test(tc_empty, test_ctrl_u_empty_input_buffer);
    tcase_add_test(tc_empty, test_ctrl_a_after_delete_to_empty);
    tcase_add_test(tc_empty, test_ctrl_e_after_delete_to_empty);
    tcase_add_test(tc_empty, test_ctrl_k_after_delete_to_empty);
    tcase_add_test(tc_empty, test_ctrl_u_after_delete_to_empty);
    suite_add_tcase(s, tc_empty);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = empty_input_buffer_navigation_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
