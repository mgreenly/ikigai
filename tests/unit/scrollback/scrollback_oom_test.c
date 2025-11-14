#include <check.h>
#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/scrollback.h"
#include "../../test_utils.h"

// Test: OOM during scrollback structure allocation
START_TEST(test_scrollback_create_oom_struct)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_next_alloc();

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);
    ck_assert_ptr_null(sb);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST

// Test: OOM during text_offsets allocation
START_TEST(test_scrollback_create_oom_offsets)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Fail on second allocation (call_count >= 2)
    oom_test_fail_after_n_calls(2);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST

// Test: OOM during text_lengths allocation
START_TEST(test_scrollback_create_oom_lengths)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Fail on third allocation (call_count >= 3)
    oom_test_fail_after_n_calls(3);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST

// Test: OOM during layouts allocation
START_TEST(test_scrollback_create_oom_layouts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Fail on fourth allocation (call_count >= 4)
    oom_test_fail_after_n_calls(4);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST

// Test: OOM during text_buffer allocation
START_TEST(test_scrollback_create_oom_buffer)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Fail on fifth allocation (call_count >= 5)
    oom_test_fail_after_n_calls(5);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST

// Test: OOM during line array growth (text_offsets)
START_TEST(test_scrollback_append_oom_grow_offsets)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    // Fill to capacity (16 lines)
    for (size_t i = 0; i < 16; i++) {
        res = ik_scrollback_append_line(sb, "line", 4);
        ck_assert(is_ok(&res));
    }
    ck_assert_uint_eq(sb->count, 16);
    ck_assert_uint_eq(sb->capacity, 16);

    // Next append will require growth - fail on text_offsets realloc
    oom_test_fail_next_alloc();
    res = ik_scrollback_append_line(sb, "overflow", 8);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    // Verify scrollback state unchanged
    ck_assert_uint_eq(sb->count, 16);
    ck_assert_uint_eq(sb->capacity, 16);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST

// Test: OOM during line array growth (text_lengths)
START_TEST(test_scrollback_append_oom_grow_lengths)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    // Fill to capacity
    for (size_t i = 0; i < 16; i++) {
        res = ik_scrollback_append_line(sb, "line", 4);
        ck_assert(is_ok(&res));
    }

    // Fail on second realloc (call_count >= 2)
    oom_test_fail_after_n_calls(2);
    res = ik_scrollback_append_line(sb, "overflow", 8);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST

// Test: OOM during line array growth (layouts)
START_TEST(test_scrollback_append_oom_grow_layouts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    // Fill to capacity
    for (size_t i = 0; i < 16; i++) {
        res = ik_scrollback_append_line(sb, "line", 4);
        ck_assert(is_ok(&res));
    }

    // Fail on third realloc (call_count >= 3)
    oom_test_fail_after_n_calls(3);
    res = ik_scrollback_append_line(sb, "overflow", 8);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST

// Test: OOM during text buffer growth
START_TEST(test_scrollback_append_oom_grow_buffer)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    // Fill text buffer to capacity (1024 bytes)
    char large_line[256];
    memset(large_line, 'x', 255);
    large_line[255] = '\0';

    for (size_t i = 0; i < 4; i++) {  // 4 × 255 = 1020 bytes
        res = ik_scrollback_append_line(sb, large_line, 255);
        ck_assert(is_ok(&res));
    }
    ck_assert_uint_eq(sb->buffer_used, 1020);
    ck_assert_uint_eq(sb->buffer_capacity, 1024);

    // Next append will require buffer growth
    oom_test_fail_next_alloc();
    res = ik_scrollback_append_line(sb, "overflow", 8);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    // Verify scrollback state unchanged
    ck_assert_uint_eq(sb->buffer_used, 1020);
    ck_assert_uint_eq(sb->buffer_capacity, 1024);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST

static Suite *scrollback_oom_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Scrollback OOM");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_scrollback_create_oom_struct);
    tcase_add_test(tc_core, test_scrollback_create_oom_offsets);
    tcase_add_test(tc_core, test_scrollback_create_oom_lengths);
    tcase_add_test(tc_core, test_scrollback_create_oom_layouts);
    tcase_add_test(tc_core, test_scrollback_create_oom_buffer);
    tcase_add_test(tc_core, test_scrollback_append_oom_grow_offsets);
    tcase_add_test(tc_core, test_scrollback_append_oom_grow_lengths);
    tcase_add_test(tc_core, test_scrollback_append_oom_grow_layouts);
    tcase_add_test(tc_core, test_scrollback_append_oom_grow_buffer);

    suite_add_tcase(s, tc_core);
    return s;
}

int32_t main(void)
{
    int32_t number_failed;
    Suite *s;
    SRunner *sr;

    s = scrollback_oom_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
