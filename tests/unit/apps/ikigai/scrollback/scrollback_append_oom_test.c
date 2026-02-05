#include <check.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/scrollback.h"
#include "tests/helpers/test_utils_helper.h"

// Test: OOM handling during append (array reallocation failure - text_offsets)
START_TEST(test_scrollback_append_oom_array_realloc) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Fill scrollback to initial capacity (16 lines)
    for (int i = 0; i < 16; i++) {
        char line[32];
        snprintf(line, sizeof(line), "Line %d", i);
        res_t res = ik_scrollback_append_line(sb, line, strlen(line));
        ck_assert(is_ok(&res));
    }

    // Verify we're at capacity
    ck_assert_uint_eq(sb->count, 16);
    ck_assert_uint_eq(sb->capacity, 16);

    // Reset realloc counter and set to fail on first call
    ik_test_talloc_realloc_call_count = 0;
    ik_test_talloc_realloc_fail_on_call = 0;

    // Try to append one more line - should trigger array reallocation
    // and fail when trying to reallocate text_offsets
    res_t res = ik_scrollback_append_line(sb, "Overflow line", 13);

    // Should return OUT_OF_MEMORY error
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_OUT_OF_MEMORY);

    // State should not have changed
    ck_assert_uint_eq(sb->count, 16);

    // Cleanup
    talloc_free(res.err);
    ik_test_talloc_realloc_fail_on_call = -1;
    talloc_free(ctx);
}
END_TEST
// Test: OOM handling during append (text_lengths reallocation failure)
START_TEST(test_scrollback_append_oom_lengths_realloc) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Fill scrollback to initial capacity (16 lines)
    for (int i = 0; i < 16; i++) {
        char line[32];
        snprintf(line, sizeof(line), "Line %d", i);
        res_t res = ik_scrollback_append_line(sb, line, strlen(line));
        ck_assert(is_ok(&res));
    }

    // Reset realloc counter and set to fail on SECOND call
    // (first = text_offsets, second = text_lengths)
    ik_test_talloc_realloc_call_count = 0;
    ik_test_talloc_realloc_fail_on_call = 1;

    // Try to append one more line
    res_t res = ik_scrollback_append_line(sb, "Overflow line", 13);

    // Should return OUT_OF_MEMORY error
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_OUT_OF_MEMORY);

    // Cleanup
    talloc_free(res.err);
    ik_test_talloc_realloc_fail_on_call = -1;
    talloc_free(ctx);
}

END_TEST
// Test: OOM handling during append (layouts reallocation failure)
START_TEST(test_scrollback_append_oom_layouts_realloc) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Fill scrollback to initial capacity (16 lines)
    for (int i = 0; i < 16; i++) {
        char line[32];
        snprintf(line, sizeof(line), "Line %d", i);
        res_t res = ik_scrollback_append_line(sb, line, strlen(line));
        ck_assert(is_ok(&res));
    }

    // Reset realloc counter and set to fail on THIRD call
    // (first = text_offsets, second = text_lengths, third = layouts)
    ik_test_talloc_realloc_call_count = 0;
    ik_test_talloc_realloc_fail_on_call = 2;

    // Try to append one more line
    res_t res = ik_scrollback_append_line(sb, "Overflow line", 13);

    // Should return OUT_OF_MEMORY error
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_OUT_OF_MEMORY);

    // Cleanup
    talloc_free(res.err);
    ik_test_talloc_realloc_fail_on_call = -1;
    talloc_free(ctx);
}

END_TEST
// Test: OOM handling during append (buffer reallocation failure)
START_TEST(test_scrollback_append_oom_buffer_realloc) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Create a very long line that exceeds initial buffer capacity (1024 bytes)
    char long_line[2000];
    memset(long_line, 'A', sizeof(long_line) - 1);
    long_line[sizeof(long_line) - 1] = '\0';

    // Reset realloc counter and set to fail on first call
    ik_test_talloc_realloc_call_count = 0;
    ik_test_talloc_realloc_fail_on_call = 0;

    // Try to append the long line - should trigger buffer reallocation
    res_t res = ik_scrollback_append_line(sb, long_line, strlen(long_line));

    // Should return OUT_OF_MEMORY error
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_OUT_OF_MEMORY);

    // State should not have changed
    ck_assert_uint_eq(sb->count, 0);

    // Cleanup
    talloc_free(res.err);
    ik_test_talloc_realloc_fail_on_call = -1;
    talloc_free(ctx);
}

END_TEST

static Suite *scrollback_append_oom_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Scrollback Append OOM");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_scrollback_append_oom_array_realloc);
    tcase_add_test(tc_core, test_scrollback_append_oom_lengths_realloc);
    tcase_add_test(tc_core, test_scrollback_append_oom_layouts_realloc);
    tcase_add_test(tc_core, test_scrollback_append_oom_buffer_realloc);

    suite_add_tcase(s, tc_core);
    return s;
}

int32_t main(void)
{
    int32_t number_failed;
    Suite *s;
    SRunner *sr;

    s = scrollback_append_oom_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/scrollback/scrollback_append_oom_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
