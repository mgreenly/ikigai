/**
 * @file layout_cache_test.c
 * @brief Unit tests for input_buffer layout caching
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input_buffer/core.h"
#include "../../test_utils_helper.h"

/* Test: Initial state - no layout cached */
START_TEST(test_initial_state) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    /* Create input_buffer */
    input_buffer = ik_input_buffer_create(ctx);

    /* Initial state: layout should be dirty, physical_lines should be 0 */
    ck_assert_int_eq(input_buffer->layout_dirty, 1);
    ck_assert_uint_eq(input_buffer->physical_lines, 0);
    ck_assert_int_eq(input_buffer->cached_width, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: Ensure layout - first time (dirty) */
START_TEST(test_ensure_layout_initial) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    int32_t terminal_width = 80;

    input_buffer = ik_input_buffer_create(ctx);

    /* Add single-line text (no wrapping) */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'o');

    /* Ensure layout */
    ik_input_buffer_ensure_layout(input_buffer, terminal_width);

    /* Verify layout was calculated */
    ck_assert_int_eq(input_buffer->layout_dirty, 0);
    ck_assert_int_eq(input_buffer->cached_width, terminal_width);
    ck_assert_uint_eq(input_buffer->physical_lines, 1);  /* Single line, no wrapping */

    talloc_free(ctx);
}

END_TEST
/* Test: Ensure layout - clean cache (no recalculation) */
START_TEST(test_ensure_layout_clean) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    int32_t terminal_width = 80;

    input_buffer = ik_input_buffer_create(ctx);
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');

    /* First ensure layout */
    ik_input_buffer_ensure_layout(input_buffer, terminal_width);

    /* Manually mark as clean to verify no recalculation */
    input_buffer->layout_dirty = 0;
    size_t prev_physical = input_buffer->physical_lines;

    /* Second ensure layout with same width - should be no-op */
    ik_input_buffer_ensure_layout(input_buffer, terminal_width);
    ck_assert_int_eq(input_buffer->layout_dirty, 0);
    ck_assert_uint_eq(input_buffer->physical_lines, prev_physical);

    talloc_free(ctx);
}

END_TEST
/* Test: Ensure layout - terminal resize */
START_TEST(test_ensure_layout_resize) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Add text that will wrap differently at different widths */
    const char *text = "This is a long line that will wrap at different terminal widths";
    for (const char *p = text; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)*p);
    }

    /* Ensure layout at width 80 */
    ik_input_buffer_ensure_layout(input_buffer, 80);
    size_t lines_at_80 = input_buffer->physical_lines;

    /* Ensure layout at width 40 (should wrap more) */
    ik_input_buffer_ensure_layout(input_buffer, 40);
    size_t lines_at_40 = input_buffer->physical_lines;

    /* More wrapping at narrower width */
    ck_assert_uint_gt(lines_at_40, lines_at_80);
    ck_assert_int_eq(input_buffer->cached_width, 40);

    talloc_free(ctx);
}

END_TEST
/* Test: Invalidate layout */
START_TEST(test_invalidate_layout) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    int32_t terminal_width = 80;

    input_buffer = ik_input_buffer_create(ctx);
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');

    /* Ensure layout */
    ik_input_buffer_ensure_layout(input_buffer, terminal_width);
    ck_assert_int_eq(input_buffer->layout_dirty, 0);

    /* Invalidate layout */
    ik_input_buffer_invalidate_layout(input_buffer);
    ck_assert_int_eq(input_buffer->layout_dirty, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Get physical lines */
START_TEST(test_get_physical_lines) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    int32_t terminal_width = 80;

    input_buffer = ik_input_buffer_create(ctx);
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');

    /* Before ensuring layout */
    size_t physical = ik_input_buffer_get_physical_lines(input_buffer);
    ck_assert_uint_eq(physical, 0);

    /* After ensuring layout */
    ik_input_buffer_ensure_layout(input_buffer, terminal_width);
    physical = ik_input_buffer_get_physical_lines(input_buffer);
    ck_assert_uint_eq(physical, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - empty input_buffer */
START_TEST(test_layout_empty) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Ensure layout for empty input_buffer */
    ik_input_buffer_ensure_layout(input_buffer, 80);
    ck_assert_uint_eq(input_buffer->physical_lines, 0);  /* Empty input_buffer = 0 physical lines (Bug #10 fix) */

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - single line (no newline) */
START_TEST(test_layout_single_line_no_wrap) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Add short text */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');

    ik_input_buffer_ensure_layout(input_buffer, 80);
    ck_assert_uint_eq(input_buffer->physical_lines, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - single line with wrapping */
START_TEST(test_layout_single_line_wrap) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Add text that wraps at width 10: "hello world" = 11 chars, wraps to 2 lines */
    const char *text = "hello world";
    for (const char *p = text; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)*p);
    }

    ik_input_buffer_ensure_layout(input_buffer, 10);
    ck_assert_uint_eq(input_buffer->physical_lines, 2);  /* 11 chars / 10 width = 2 lines */

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - multi-line with newlines */
START_TEST(test_layout_multiline) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Add 3 logical lines */
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');
    ik_input_buffer_insert_codepoint(input_buffer, 'n');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, '1');
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');
    ik_input_buffer_insert_codepoint(input_buffer, 'n');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, '2');
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_codepoint(input_buffer, 'l');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');
    ik_input_buffer_insert_codepoint(input_buffer, 'n');
    ik_input_buffer_insert_codepoint(input_buffer, 'e');
    ik_input_buffer_insert_codepoint(input_buffer, '3');

    ik_input_buffer_ensure_layout(input_buffer, 80);
    ck_assert_uint_eq(input_buffer->physical_lines, 3);  /* 3 logical lines, no wrapping */

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - multi-line with wrapping */
START_TEST(test_layout_multiline_wrap) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Line 1: "hello world" (11 chars, wraps to 2 physical at width 10) */
    const char *line1 = "hello world";
    for (const char *p = line1; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)*p);
    }
    ik_input_buffer_insert_newline(input_buffer);

    /* Line 2: "hi" (2 chars, 1 physical line) */
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');

    ik_input_buffer_ensure_layout(input_buffer, 10);
    ck_assert_uint_eq(input_buffer->physical_lines, 3);  /* 2 + 1 = 3 physical lines */

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - UTF-8 content */
START_TEST(test_layout_utf8) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Add UTF-8 content: "你好" (2 wide chars = 4 display width) */
    ik_input_buffer_insert_codepoint(input_buffer, 0x4F60);  /* 你 */
    ik_input_buffer_insert_codepoint(input_buffer, 0x597D);  /* 好 */

    ik_input_buffer_ensure_layout(input_buffer, 80);
    ck_assert_uint_eq(input_buffer->physical_lines, 1);  /* Fits in one line */

    /* Narrow width: should wrap */
    ik_input_buffer_ensure_layout(input_buffer, 3);
    ck_assert_uint_eq(input_buffer->physical_lines, 2);  /* 4 display width / 3 = 2 lines */

    talloc_free(ctx);
}

END_TEST
/* Test: Text modifications invalidate layout */
START_TEST(test_text_modification_invalidates_layout) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);
    ik_input_buffer_insert_codepoint(input_buffer, 'h');
    ik_input_buffer_insert_codepoint(input_buffer, 'i');

    /* Ensure layout */
    ik_input_buffer_ensure_layout(input_buffer, 80);
    ck_assert_int_eq(input_buffer->layout_dirty, 0);

    /* Insert character - should invalidate */
    ik_input_buffer_insert_codepoint(input_buffer, '!');
    ck_assert_int_eq(input_buffer->layout_dirty, 1);

    /* Re-ensure */
    ik_input_buffer_ensure_layout(input_buffer, 80);
    ck_assert_int_eq(input_buffer->layout_dirty, 0);

    /* Backspace - should invalidate */
    ik_input_buffer_backspace(input_buffer);
    ck_assert_int_eq(input_buffer->layout_dirty, 1);

    /* Re-ensure */
    ik_input_buffer_ensure_layout(input_buffer, 80);
    ck_assert_int_eq(input_buffer->layout_dirty, 0);

    /* Delete - should invalidate */
    ik_input_buffer_cursor_left(input_buffer);
    ik_input_buffer_delete(input_buffer);
    ck_assert_int_eq(input_buffer->layout_dirty, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - empty lines (just newlines) */
START_TEST(test_layout_empty_lines) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Add 3 newlines: "\n\n\n" creates 4 lines (3 terminated + 1 after last) */
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_newline(input_buffer);
    ik_input_buffer_insert_newline(input_buffer);

    ik_input_buffer_ensure_layout(input_buffer, 80);
    ck_assert_uint_eq(input_buffer->physical_lines, 4);  /* 3 newlines = 4 lines */

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - zero-width characters */
START_TEST(test_layout_zero_width) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    input_buffer = ik_input_buffer_create(ctx);

    /* Add zero-width space (U+200B) */
    ik_input_buffer_insert_codepoint(input_buffer, 0x200B);
    ik_input_buffer_insert_codepoint(input_buffer, 0x200B);

    ik_input_buffer_ensure_layout(input_buffer, 80);
    ck_assert_uint_eq(input_buffer->physical_lines, 1);  /* Zero-width characters still occupy 1 line */

    talloc_free(ctx);
}

END_TEST
/* Test: Layout calculation - ANSI escape sequences ignored in width */
START_TEST(test_layout_ansi_width) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = ik_input_buffer_create(ctx);

    /* Add text: "\x1b[38;5;242mhello\x1b[0m world" (11 visible chars) */
    const char *text = "\x1b[38;5;242mhello\x1b[0m world";
    for (const char *p = text; *p; p++) {
        ik_input_buffer_insert_codepoint(input_buffer, (uint32_t)(unsigned char)*p);
    }

    /* Width 80: 1 line, width 10: 2 lines (11 chars ignoring escapes) */
    ik_input_buffer_ensure_layout(input_buffer, 80);
    ck_assert_uint_eq(input_buffer->physical_lines, 1);

    ik_input_buffer_ensure_layout(input_buffer, 10);
    ck_assert_uint_eq(input_buffer->physical_lines, 2);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
/* Test: NULL parameter assertions */
START_TEST(test_ensure_layout_null_asserts) {
    /* input_buffer cannot be NULL - should abort */
    ik_input_buffer_ensure_layout(NULL, 80);
}

END_TEST

START_TEST(test_invalidate_layout_null_asserts) {
    /* input_buffer cannot be NULL - should abort */
    ik_input_buffer_invalidate_layout(NULL);
}

END_TEST

START_TEST(test_get_physical_lines_null_asserts) {
    /* input_buffer cannot be NULL - should abort */
    ik_input_buffer_get_physical_lines(NULL);
}

END_TEST
#endif

static Suite *input_buffer_layout_cache_suite(void)
{
    Suite *s = suite_create("Input Buffer Layout Cache");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind

    /* Normal tests */
    tcase_add_test(tc_core, test_initial_state);
    tcase_add_test(tc_core, test_ensure_layout_initial);
    tcase_add_test(tc_core, test_ensure_layout_clean);
    tcase_add_test(tc_core, test_ensure_layout_resize);
    tcase_add_test(tc_core, test_invalidate_layout);
    tcase_add_test(tc_core, test_get_physical_lines);
    tcase_add_test(tc_core, test_layout_empty);
    tcase_add_test(tc_core, test_layout_single_line_no_wrap);
    tcase_add_test(tc_core, test_layout_single_line_wrap);
    tcase_add_test(tc_core, test_layout_multiline);
    tcase_add_test(tc_core, test_layout_multiline_wrap);
    tcase_add_test(tc_core, test_layout_utf8);
    tcase_add_test(tc_core, test_text_modification_invalidates_layout);
    tcase_add_test(tc_core, test_layout_empty_lines);
    tcase_add_test(tc_core, test_layout_zero_width);
    tcase_add_test(tc_core, test_layout_ansi_width);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    /* Assertion tests */
    tcase_add_test_raise_signal(tc_assertions, test_ensure_layout_null_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_invalidate_layout_null_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_get_physical_lines_null_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_layout_cache_suite();
    SRunner *sr = srunner_create(s);

    /* Disable forking with ASAN/SKIP_SIGNAL_TESTS for compatibility */
#if defined(SKIP_SIGNAL_TESTS)
    srunner_set_fork_status(sr, CK_NOFORK);
#endif

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
