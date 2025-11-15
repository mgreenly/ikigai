/**
 * @file scrollback_reflow_perf.c
 * @brief Performance tests for scrollback buffer reflow operations
 *
 * Tests verify that layout recalculation on terminal resize meets
 * performance targets:
 * - 1000 lines: < 5ms (target: < 1ms)
 *
 * Uses high-precision timing (clock_gettime with CLOCK_MONOTONIC) to
 * measure reflow performance.
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include <time.h>
#include <inttypes.h>
#include "../../src/scrollback.h"
#include "../test_utils.h"

/**
 * @brief Calculate elapsed time in milliseconds
 *
 * @param start Start timespec
 * @param end End timespec
 * @return Elapsed time in milliseconds (double precision)
 */
static double timespec_diff_ms(const struct timespec *start, const struct timespec *end) {
    double seconds = (double)(end->tv_sec - start->tv_sec);
    double nanoseconds = (double)(end->tv_nsec - start->tv_nsec);
    return (seconds * 1000.0) + (nanoseconds / 1000000.0);
}

/* Test: Reflow 1000 lines (target: < 5ms) */
START_TEST(test_scrollback_reflow_1000_lines) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = NULL;
    int32_t initial_width = 80;
    int32_t new_width = 120;
    const size_t num_lines = 1000;
    const size_t avg_line_length = 50;

    /* Create scrollback with initial width */
    res_t res = ik_scrollback_create(ctx, initial_width, &scrollback);
    ck_assert(is_ok(&res));

    /* Add 1000 lines with varied content (avg ~50 chars) */
    for (size_t i = 0; i < num_lines; i++) {
        char line_text[256];
        size_t text_len;

        /* Vary line length: 30-70 chars (avg 50) */
        int32_t variation = (int32_t)(i % 41) - 20; // -20 to +20
        size_t line_len = avg_line_length + (size_t)variation;

        /* Create line with ASCII text */
        int32_t ret = snprintf(line_text, sizeof(line_text),
                               "Line %zu: This is test content with some text %zu",
                               i, i * 42);
        text_len = (size_t)ret;

        /* Pad or truncate to desired length */
        if (text_len < line_len && line_len < sizeof(line_text)) {
            for (size_t j = text_len; j < line_len; j++) {
                line_text[j] = 'x';
            }
            line_text[line_len] = '\0';
            text_len = line_len;
        } else if (text_len > line_len) {
            line_text[line_len] = '\0';
            text_len = line_len;
        }

        res = ik_scrollback_append_line(scrollback, line_text, text_len);
        ck_assert(is_ok(&res));
    }

    /* Verify lines were added */
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), num_lines);

    /* Measure reflow time: 80 -> 120 columns */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    res = ik_scrollback_ensure_layout(scrollback, new_width);

    clock_gettime(CLOCK_MONOTONIC, &end);

    /* Verify success */
    ck_assert(is_ok(&res));

    /* Calculate elapsed time */
    double elapsed_ms = timespec_diff_ms(&start, &end);

    /* Print result for verification */
    printf("\nReflow Performance (1000 lines, 80->120 cols):\n");
    printf("  Elapsed: %.3f ms\n", elapsed_ms);
    printf("  Target:  < 5.000 ms\n");
    printf("  Status:  %s\n", elapsed_ms < 5.0 ? "PASS" : "FAIL");

    /* Verify performance target */
    ck_assert_msg(elapsed_ms < 5.0,
                  "Reflow took %.3f ms, expected < 5.0 ms", elapsed_ms);

    /* Optional: Check for ideal target (< 1ms) - informational only */
    if (elapsed_ms < 1.0) {
        printf("  Note:    Ideal target (< 1ms) achieved!\n");
    } else {
        printf("  Note:    Ideal target (< 1ms) not achieved (%.3f ms)\n", elapsed_ms);
    }

    talloc_free(ctx);
}
END_TEST

/* Test: Reflow with UTF-8 content (1000 lines with emoji) */
START_TEST(test_scrollback_reflow_1000_lines_utf8) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = NULL;
    int32_t initial_width = 80;
    int32_t new_width = 120;
    const size_t num_lines = 1000;

    /* Create scrollback with initial width */
    res_t res = ik_scrollback_create(ctx, initial_width, &scrollback);
    ck_assert(is_ok(&res));

    /* Add 1000 lines with UTF-8 content (emoji, CJK) */
    for (size_t i = 0; i < num_lines; i++) {
        char line_text[256];
        size_t text_len;

        /* Mix of ASCII and UTF-8 */
        int32_t ret;
        if (i % 3 == 0) {
            /* Emoji */
            ret = snprintf(line_text, sizeof(line_text),
                          "Line %zu: 😀 🎉 🚀 test content %zu",
                          i, i * 42);
        } else if (i % 3 == 1) {
            /* CJK */
            ret = snprintf(line_text, sizeof(line_text),
                          "Line %zu: 你好世界 こんにちは content %zu",
                          i, i * 42);
        } else {
            /* ASCII */
            ret = snprintf(line_text, sizeof(line_text),
                          "Line %zu: Regular ASCII test content here %zu",
                          i, i * 42);
        }
        text_len = (size_t)ret;

        res = ik_scrollback_append_line(scrollback, line_text, text_len);
        ck_assert(is_ok(&res));
    }

    /* Verify lines were added */
    ck_assert_uint_eq(ik_scrollback_get_line_count(scrollback), num_lines);

    /* Measure reflow time: 80 -> 120 columns */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    res = ik_scrollback_ensure_layout(scrollback, new_width);

    clock_gettime(CLOCK_MONOTONIC, &end);

    /* Verify success */
    ck_assert(is_ok(&res));

    /* Calculate elapsed time */
    double elapsed_ms = timespec_diff_ms(&start, &end);

    /* Print result for verification */
    printf("\nReflow Performance (1000 lines UTF-8, 80->120 cols):\n");
    printf("  Elapsed: %.3f ms\n", elapsed_ms);
    printf("  Target:  < 5.000 ms\n");
    printf("  Status:  %s\n", elapsed_ms < 5.0 ? "PASS" : "FAIL");

    /* Verify performance target */
    ck_assert_msg(elapsed_ms < 5.0,
                  "Reflow took %.3f ms, expected < 5.0 ms", elapsed_ms);

    talloc_free(ctx);
}
END_TEST

/* Test: Multiple reflows (verify consistent performance) */
START_TEST(test_scrollback_multiple_reflows) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = NULL;
    int32_t initial_width = 80;
    const size_t num_lines = 1000;

    /* Create scrollback with initial width */
    res_t res = ik_scrollback_create(ctx, initial_width, &scrollback);
    ck_assert(is_ok(&res));

    /* Add 1000 lines */
    for (size_t i = 0; i < num_lines; i++) {
        char line_text[256];
        int32_t ret = snprintf(line_text, sizeof(line_text),
                               "Line %zu: Test content here %zu",
                               i, i * 42);
        size_t text_len = (size_t)ret;

        res = ik_scrollback_append_line(scrollback, line_text, text_len);
        ck_assert(is_ok(&res));
    }

    /* Perform multiple reflows and measure each */
    int32_t widths[] = {120, 100, 60, 140, 80};
    const size_t num_reflows = sizeof(widths) / sizeof(widths[0]);

    printf("\nMultiple Reflow Performance (1000 lines):\n");

    for (size_t i = 0; i < num_reflows; i++) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        res = ik_scrollback_ensure_layout(scrollback, widths[i]);

        clock_gettime(CLOCK_MONOTONIC, &end);

        ck_assert(is_ok(&res));

        double elapsed_ms = timespec_diff_ms(&start, &end);
        printf("  Reflow %zu (width=%d): %.3f ms %s\n",
               i + 1, widths[i], elapsed_ms,
               elapsed_ms < 5.0 ? "PASS" : "FAIL");

        ck_assert_msg(elapsed_ms < 5.0,
                      "Reflow %zu took %.3f ms, expected < 5.0 ms", i, elapsed_ms);
    }

    talloc_free(ctx);
}
END_TEST

/* Test: No reflow when width unchanged (should be instant) */
START_TEST(test_scrollback_no_reflow_same_width) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = NULL;
    int32_t width = 80;
    const size_t num_lines = 1000;

    /* Create scrollback */
    res_t res = ik_scrollback_create(ctx, width, &scrollback);
    ck_assert(is_ok(&res));

    /* Add 1000 lines */
    for (size_t i = 0; i < num_lines; i++) {
        char line_text[256];
        int32_t ret = snprintf(line_text, sizeof(line_text),
                               "Line %zu: Test content %zu", i, i * 42);
        size_t text_len = (size_t)ret;

        res = ik_scrollback_append_line(scrollback, line_text, text_len);
        ck_assert(is_ok(&res));
    }

    /* Measure ensure_layout with SAME width (should be O(1) cache hit) */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    res = ik_scrollback_ensure_layout(scrollback, width);

    clock_gettime(CLOCK_MONOTONIC, &end);

    ck_assert(is_ok(&res));

    double elapsed_ms = timespec_diff_ms(&start, &end);

    printf("\nNo-Reflow Performance (1000 lines, same width):\n");
    printf("  Elapsed: %.3f ms\n", elapsed_ms);
    printf("  Note:    Should be near-instant (cache hit)\n");

    /* Should be extremely fast (< 0.1ms) since it's just a width comparison */
    ck_assert_msg(elapsed_ms < 0.1,
                  "No-op reflow took %.3f ms, expected < 0.1 ms (cache hit)", elapsed_ms);

    talloc_free(ctx);
}
END_TEST

/* Create test suite */
static Suite *scrollback_reflow_perf_suite(void) {
    Suite *s = suite_create("Scrollback Reflow Performance");
    TCase *tc_perf = tcase_create("Performance");
    tcase_set_timeout(tc_perf, 30);

    tcase_add_test(tc_perf, test_scrollback_reflow_1000_lines);
    tcase_add_test(tc_perf, test_scrollback_reflow_1000_lines_utf8);
    tcase_add_test(tc_perf, test_scrollback_multiple_reflows);
    tcase_add_test(tc_perf, test_scrollback_no_reflow_same_width);

    suite_add_tcase(s, tc_perf);
    return s;
}

int32_t main(void) {
    int32_t number_failed;
    Suite *s = scrollback_reflow_perf_suite();
    SRunner *sr = srunner_create(s);

    /* Run with normal verbosity to see printf output */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
